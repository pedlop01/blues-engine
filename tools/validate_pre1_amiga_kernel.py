#!/usr/bin/env python3
"""Compare bounded native RNG and actor initialisation with original Amiga code.
Research only; original code/bytes remain in a new private build/ directory.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 for key in ['datapath','sections','trace','output']:p.add_argument('--'+key,type=Path,required=True)
 a=p.parse_args();root=a.datapath.resolve();out=a.output.resolve();sections=a.sections.resolve()
 if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build/ directory')
 def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
 before=snapshot();out.mkdir(parents=True)
 from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE
 from unicorn.m68k_const import UC_CPU_M68K_M68000,UC_M68K_REG_A7,UC_M68K_REG_PC,UC_M68K_REG_D0
 def vm():
  u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
  for n,base in enumerate([0x20000,0x50000,0x80000,0xb0000]):u.mem_write(base,(sections/f'section-{n}.bin').read_bytes())
  return u
 def native(mode,cases):
  c=out/(mode+'-cases.json');r=out/(mode+'-native.json');c.write_text(json.dumps(cases));subprocess.run([str(a.trace.resolve()),str(root),mode,str(c),str(r)],check=True,timeout=60);return json.loads(r.read_text())
 bounds=[0,1,2,3,5,10,15,30,100,134,210,320,1000,32767,32768,65535]
 cases=[dict(seed=seed,bounds=bounds*16) for seed in [0,1,2,0x80000000,0xffffffff,0x12345678,0xdeadbeef]];results=native('rng',cases);ticks=0
 for case,expected in zip(cases,results):
  u=vm();u.mem_write(0x82008,case['seed'].to_bytes(4,'big'))
  def hook(cpu,pc,size,unused):
   if pc==0x10002:cpu.emu_stop();return
   if not 0xc5494<=pc<0xc54ce:raise ValueError('RNG escaped')
  u.hook_add(UC_HOOK_CODE,hook)
  for bound,row in zip(case['bounds'],expected):
   u.mem_write(0x2efff8,(0x10002).to_bytes(4,'big')+bound.to_bytes(4,'big'));u.reg_write(UC_M68K_REG_A7,0x2efff8);u.emu_start(0xc5494,0,count=1000);assert u.reg_read(UC_M68K_REG_PC)==0x10002
   actual=[u.reg_read(UC_M68K_REG_D0)&65535,int.from_bytes(u.mem_read(0x82008,4),'big')];assert actual==row,(case['seed'],bound,row,actual);ticks+=1
 spawn=[]
 for n,result in zip([1,3,5,7],native('spawns',[dict(level=n) for n in [1,3,5,7]])):
  u=vm();dat=(root/f'level{n}.dat').read_bytes();tile_size=int.from_bytes(dat[:2],'big');u.mem_write(0x180000,dat[4+tile_size:]);u.mem_write(0x82008,b'\0'*4)
  def hook(cpu,pc,size,unused):
   if pc==0x10002:cpu.emu_stop();return
   if 0xb7490<=pc<0xb7a7c or 0xb2a38<=pc<0xb2a50 or 0xc5494<=pc<0xc54ce:return
   raise ValueError(('Actor reset escaped',hex(pc)))
  u.hook_add(UC_HOOK_CODE,hook)
  for table in [0x180000,0x80e28,0x80eba]:
   u.mem_write(0x2efff8,(0x10002).to_bytes(4,'big')+table.to_bytes(4,'big'));u.reg_write(UC_M68K_REG_A7,0x2efff8);u.emu_start(0xb74c2,0,count=1000000);assert u.reg_read(UC_M68K_REG_PC)==0x10002
  rng=int.from_bytes(u.mem_read(0x82008,4),'big');assert rng==result['rng'],(n,'initialisation RNG',rng,result['rng']);spawn.append(dict(level=n,rng=rng,original_match=True))
 assert snapshot()==before
 report=dict(schema_version=1,rng_calls=ticks,initialisation=spawn,originals_unchanged=True,scope='bounded RNG and full-table initialisation random-call order; not actor motion/contact parity')
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()
