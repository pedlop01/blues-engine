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
import json,subprocess
from pathlib import Path
from unicorn import *
from unicorn.m68k_const import *
cases=[dict(level=n,pattern=p,ticks=240) for n in [2,4,6] for p in range(4)];(out/'boss2-cases.json').write_text(json.dumps(cases));subprocess.run([str(ARGS.trace.resolve()),str(root),'bosses',str(out/'boss2-cases.json'),str(out/'boss2-native.json')],check=True);expected=json.loads((out/'boss2-native.json').read_text());results=[]
for case,frames in zip(cases,expected):
 u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
 level=case['level'];loop={2:0xc0b66,4:0xc29d8,6:0xc3d78}[level];stop={2:0xc0d46,4:0xc2c64,6:0xc3e72}[level]
 for n,base in enumerate([0x20000,0x50000,0x80000,0xb0000]):u.mem_write(base,(reference/f'section-{n}.bin').read_bytes())
 def put(a,v,size=2):u.mem_write(a,(v&((1<<(size*8))-1)).to_bytes(size,'big'))
 def long(a):return int.from_bytes(u.mem_read(a,4),'big')
 def word(a):return int.from_bytes(u.mem_read(a,2),'big',signed=True)
 def ret(v=0):
  sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4);u.reg_write(UC_M68K_REG_D0,v&0xffffffff)
 fired=False;left=right=up=False
 def hook(cpu,pc,size,unused):
  if pc==0x10002:cpu.emu_stop();return
  if pc==0xb3e86:ret();cpu.emu_stop();return
  if pc==0xb3694:ret(int(fired));return
  if pc==0xb33ca:ret(-50 if right else 50 if left else 0);return
  if pc==0xb3426:ret(50 if up else 0);return
  if pc in [0xbea7e,0xbe64e,0xb0e36,0xb15c8,0xb2d60,0xbe5d2,0xb6e46,0xb6de4,0xbf8cc,0xb1594,0xbe77a,0xbeae4,0xbe8a0]:ret();return
  if 0xc238c<=pc<0xc280c or 0xc29d8<=pc<0xc2c7c or 0xc37ac<=pc<0xc3bc2 or 0xc3d78<=pc<0xc3e8a or 0xbf996<=pc<0xc081a or 0xc081a<=pc<0xc08ea or 0xc0b66<=pc<0xc0d5e or 0xba1fc<=pc<0xba264 or 0xbd858<=pc<0xbd8cc or 0xb2a38<=pc<0xb2a50 or 0xc5230<=pc<0xc54de:return
  raise ValueError((case,hex(pc),'escaped'))
 u.hook_add(UC_HOOK_CODE,hook);put(0x5097e,3);put(0x82008,0,4)
 put(0x2effd0,0x10002,4);u.reg_write(UC_M68K_REG_A7,0x2effd0);u.emu_start({2:0xc081a,4:0xc2750,6:0xc3b0c}[level],0,count=100000);assert u.reg_read(UC_M68K_REG_PC)==0x10002
 u.reg_write(UC_M68K_REG_A5,0x2ef000);u.reg_write(UC_M68K_REG_A7,0x2eef00)
 for tick,frame in enumerate(frames):
  p=case['pattern'];fired=p==3 and tick%45<5;right=p==1 or(p==3 and tick%120<60);left=p==2 or(p==3 and tick%120>=60);up=p==3 and tick%60<20
  for address,value in zip([0x5098a,0x5098e,0x50990],[p==3 and tick%60<20,p==1 or(p==3 and tick%120<60),p==2 or(p==3 and tick%120>=60)]):put(address,int(value))
  u.emu_start(loop,0,count=1000000);assert u.reg_read(UC_M68K_REG_PC)==stop,hex(u.reg_read(UC_M68K_REG_PC))
  player=[word(a) for a in [0x508f0,0x508f2,0x5090c,0x5090a,0x5091a,0x5091e,0x50910,0x50912,0x50914,0x508ec,0x508ee,0x5097e,0x50994]];want=frame['player'][:]
  # The port keeps a render shadow in x/y while the original keeps its corpse
  # in separate fields. Compare the actual saved death coordinates separately.
  if player[6]:player[0:2]=want[0:2]
  boss=[word(a) for a in {2:[0x5146c,0x5146e,0x51470,0x51472,0x51474],4:[0x514b2,0x514b4,0x514b6,0x514b8,0x514ba,0x514bc,0x514c0,0x514be],6:[0x514c6,0x514ce,0x514d0]}[level]]
  if player!=want or boss!=frame['boss'] or long(0x82008)!=frame['rng']:raise ValueError((case,tick,'player',player,want,'boss',boss,frame['boss'],'rng',long(0x82008),frame['rng']))
  assert [word(0x508fc),word(0x508fe)]==frame['corpse'],(case,tick,'corpse')
  for i,slot in enumerate(frame['slots'] if level==2 else []):
   got=[word(0x51476+i*8+j*2) for j in range(4)];want=[slot[f] for f in ['ptype','direction','x','frame']]
   if got!=want:raise ValueError((case,tick,'slot',i,got,want))
 results.append(case);print('matched',case,flush=True)
(out/'boss2-validation.json').write_text(json.dumps(results,indent=2))

assert snapshot()==BEFORE
