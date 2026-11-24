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
import json,subprocess,struct
from pathlib import Path
from unicorn import *
from unicorn.m68k_const import *
cases=[dict(level=n,room=c) for n,lo,hi in [(1,0,21),(3,22,25),(5,26,38),(7,39,52)] for c in range(lo,hi+1)];(out/'room-cases.json').write_text(json.dumps(cases));subprocess.run([str(ARGS.trace.resolve()),str(root),'rooms',str(out/'room-cases.json'),str(out/'room-native.json')],check=True);expected=json.loads((out/'room-native.json').read_text());results=[]
for case,want in zip(cases,expected):
 u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
 for n,base in enumerate([0x20000,0x50000,0x80000,0xb0000]):u.mem_write(base,(reference/f'section-{n}.bin').read_bytes())
 def put(a,v,size=2):u.mem_write(a,(v&((1<<(size*8))-1)).to_bytes(size,'big'))
 def long(a):return int.from_bytes(u.mem_read(a,4),'big')
 def signed(n):return n-2**32 if n>=2**31 else n
 commands=[]
 def ret():
  sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4)
 def hook(cpu,pc,size,unused):
  if pc==0x10002:cpu.emu_stop();return
  sp=cpu.reg_read(UC_M68K_REG_A7)
  if pc==0xb130a:commands.append([0,signed(long(sp+4)),signed(long(sp+8)),long(sp+12)-1000,0,0]);ret();return
  if pc==0xb2d60:commands.append([1]+[signed(long(sp+i)) for i in [4,8,12,16,20]]);ret();return
  if pc in [0xb3088,0xb2d3e]:ret();return
  if 0xbd246<=pc<0xbd858 or 0xb70b2<=pc<0xb71f4 or 0xc5230<=pc<0xc54de:return
  raise ValueError((case,hex(pc),'escaped'))
 u.hook_add(UC_HOOK_CODE,hook);put(0x50d6c,0x1a0000,4);put(0x509a6,1000)
 # Cave bank base follows the complete L3 element bank; other caves use their own atlas.
 put(0x509aa,1033 if case['level']==3 else 1000)
 dur=(root/f'LEVEL{case["level"]}.DUR').read_bytes();u.mem_write(0x509ac,b''.join(dur[i:i+2][::-1] for i in range(0,960,2)))
 put(0x2efff8,0x10002,4);put(0x2efffc,case['room'],4);u.reg_write(UC_M68K_REG_A7,0x2efff8);routine={1:0xbd246,3:0xbd63c,5:0xbd500,7:0xbd376}[case['level']];u.emu_start(routine,0,count=1000000);assert u.reg_read(UC_M68K_REG_PC)==0x10002
 import hashlib
 digest=hashlib.sha256(u.mem_read(0x1a0000,8000)).hexdigest();assert digest==want['collision_sha256'],(case,'collision',digest,want['collision_sha256']);assert commands==want['commands'],(case,'commands',commands,want['commands']);results.append(case);print('matched',case,flush=True)
(out/'cave-validation.json').write_text(json.dumps(results,indent=2))

assert snapshot()==BEFORE
