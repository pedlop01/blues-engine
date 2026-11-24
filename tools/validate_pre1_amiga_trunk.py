#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Research-only isolated original routines, never a runtime dependency.
import argparse,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
for key in ['datapath','sections','trace','output']:parser.add_argument('--'+key,type=Path,required=True)
ARGS=parser.parse_args()
root=ARGS.datapath.resolve();out=ARGS.output.resolve();reference=ARGS.sections.resolve()
if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build/ output')
if hashlib.sha256((root/'main').read_bytes()).hexdigest()!='d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295':raise ValueError('Unsupported original executable')
def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
BEFORE=snapshot();out.mkdir(parents=True)
import json,subprocess,hashlib
from pathlib import Path
from unicorn import *
from unicorn.m68k_const import *
(out/'trunk-cases.json').write_text('[{"level":3}]');subprocess.run([str(ARGS.trace.resolve()),str(root),'trunk',str(out/'trunk-cases.json'),str(out/'trunk-native.json')],check=True);expected=json.loads((out/'trunk-native.json').read_text())[0];u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
for n,base in enumerate([0x20000,0x50000,0x80000,0xb0000]):u.mem_write(base,(reference/f'section-{n}.bin').read_bytes())
def put(a,v,size=2):u.mem_write(a,(v&((1<<(size*8))-1)).to_bytes(size,'big'))
def long(a):return int.from_bytes(u.mem_read(a,4),'big')
def signed(n):return n-2**32 if n>=2**31 else n
def ret():
 sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4);u.reg_write(UC_M68K_REG_D0,0)
frames=[];plots=[]
def hook(cpu,pc,size,unused):
 global plots
 if pc==0x10002:cpu.emu_stop();return
 if pc in [0xc1368,0xc157a]:plots=[]
 sp=cpu.reg_read(UC_M68K_REG_A7)
 if pc==0xb130a:plots.append([signed(long(sp+4)),signed(long(sp+8)),long(sp+12)]);ret();return
 if pc==0xb0e36:frames.append(plots[:]);ret();return
 if pc in [0xb2d60,0xb2d3e,0xb3088,0xb906e,0xb91bc,0xb927e,0xb9376,0xb9482,0xb1594,0xb95ee]:ret();return
 if 0xc0fb8<=pc<0xc18f8 or 0xb70b2<=pc<0xb735a or 0xc5230<=pc<0xc54de or 0xb2a38<=pc<0xb2a50:return
 raise ValueError((hex(pc),'escaped'))
u.hook_add(UC_HOOK_CODE,hook);dat=(root/'level3.dat').read_bytes();tile_size=int.from_bytes(dat[:2],'big');u.mem_write(0x180000,dat[4:4+tile_size]);dur=(root/'LEVEL3.DUR').read_bytes();u.mem_write(0x509ac,b''.join(dur[i:i+2][::-1] for i in range(0,960,2)));put(0x50d6c,0x1a0000,4);put(0x509a6,1000);put(0x509aa,1028);put(0x509a4,500);put(0x50988,3);put(0x508f0,65);put(0x508f2,68);put(0x82008,0,4)
for routine,phase,screen in [(0xc132c,'first',17),(0xc1528,'second',18)]:
 frames.clear();sp=0x2effd0;put(sp,0x10002,4)
 for i,value in enumerate([0x8189a,0x180000,screen]):put(sp+4+i*4,value,4)
 u.reg_write(UC_M68K_REG_A7,sp);u.emu_start(routine,0,count=5000000);assert u.reg_read(UC_M68K_REG_PC)==0x10002
 assert len(frames)==len(expected[phase]),(phase,len(frames))
 for i,(plots,want) in enumerate(zip(frames,expected[phase])):
  tiles=[[x,y,s-1000] for x,y,s in plots if s>=1000];assert tiles[:16]==want,(phase,i,tiles[:16],want)
  dy=i*4 if phase=='first' else -80+i*4;actors=[[x,y,s-500] for x,y,s in plots if s<1000];want=[] if phase=='second' and i==20 else [[65,68+dy,0]]
  if phase=='second' and i<20:want += [[49,175+expected['jitter'][i][0],85+(i&1)],[65,170+expected['jitter'][i][1],86+(i&1)],[79,178,85+(i&1)]]
  assert actors==want,(phase,i,actors,want)
 print(phase,len(frames),'frames match',flush=True)
assert hashlib.sha256(u.mem_read(0x1a0000,8000)).hexdigest()==expected['collision_sha256']
assert long(0x82008)==expected['rng']
(out/'trunk-validation.json').write_text(json.dumps(dict(first_frames=44,second_frames=21,collision_match=True,rng_match=True),indent=2))

assert snapshot()==BEFORE
