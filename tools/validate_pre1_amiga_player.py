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
cases=[dict(level=n,screen=s,x=100,y=70,ticks=120,pattern=p) for n in [1,3,5,7] for s in range(19) for p in range(4)];(out/'physics-cases.json').write_text(json.dumps(cases));subprocess.run([str(ARGS.trace.resolve()),str(root),'player',str(out/'physics-cases.json'),str(out/'physics-native.json')],check=True)
frames=json.loads((out/'physics-native.json').read_text());sections=[(reference/f'section-{n}.bin').read_bytes() for n in range(4)];summary=[]
for index,(case,expected) in enumerate(zip(cases,frames)):
 u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
 for base,data in zip([0x20000,0x50000,0x80000,0xb0000],sections):u.mem_write(base,data)
 u.mem_write(0x1a0000,(reference/f'collision-{case["level"]}-{case["screen"]}.bin').read_bytes());fired=False;sprite=0
 def word(a):return int.from_bytes(u.mem_read(a,2),'big')
 def signed(n):return n-65536 if n>=32768 else n
 def long(a):return int.from_bytes(u.mem_read(a,4),'big')
 def put(a,v,n=2):u.mem_write(a,(v&((1<<(n*8))-1)).to_bytes(n,'big'))
 def ret(value=0):
  sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4);u.reg_write(UC_M68K_REG_D0,value)
 def hook(cpu,pc,size,unused):
  global sprite
  if pc==0x10002:cpu.emu_stop();return
  if pc==0xb3694:ret(int(fired));return
  if pc==0xbea7e:
   sp=cpu.reg_read(UC_M68K_REG_A7);sprite=long(sp+16)-1;ret();return
  if pc==0xb6de4:ret();return # audio trigger; sequencer is validated separately
  if 0xb9a22<=pc<0xba0ac or 0xba1fc<=pc<0xba262 or 0xb8fea<=pc<0xb906e:return
  raise ValueError((index,hex(pc),'escaped'))
 u.hook_add(UC_HOOK_CODE,hook)
 for a,v in {0x508f0:100,0x508f2:70,0x508f8:100,0x508fa:70,0x508e8:0,0x508e4:case['screen'],0x50988:case['level'],0x5097e:3,0x50980:10,0x5091c:20}.items():put(a,v)
 put(0x50d6c,0x1a0000,4)
 def call(pc):
  sp=0x2efffc;put(sp,0x10002,4);u.reg_write(UC_M68K_REG_A7,sp);u.emu_start(pc,0,count=100000);assert u.reg_read(UC_M68K_REG_PC)==0x10002,'limit'
 for tick,frame in enumerate(expected):
  p=case['pattern'];fired=p==3 and tick%45<5;inputs=[p==3 and tick%60<20,False,p==1 or (p==3 and tick%120<60),p==2 or (p==3 and tick%120>=60)]
  for a,v in zip([0x5098a,0x5098c,0x5098e,0x50990],inputs):put(a,int(v))
  call(0xba1fc);call(0xb9a22)
  if word(0x5091e):put(0x5091e,word(0x5091e)-1)
  got=[signed(word(a)) for a in [0x508f0,0x508f2,0x5091a,0x5091c,0x5090a,0x5090c,0x508f4,0x508f6]]+[sprite]+[signed(word(a)) for a in [0x5091e,0x50994,0x508f8,0x508fa,0x508e8]]
  if got!=frame:raise ValueError((case,tick,'native',frame,'original',got))
 summary.append({**case,'matched':True});print(index+1,'matched',case,flush=True)
(out/'physics-validation.json').write_text(json.dumps(summary,indent=2))

assert snapshot()==BEFORE
