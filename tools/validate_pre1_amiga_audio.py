#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Research-only isolated original routines, never a runtime dependency.
import argparse,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
for key in ['datapath','sections','probe','output']:parser.add_argument('--'+key,type=Path,required=True)
ARGS=parser.parse_args()
root=ARGS.datapath.resolve();out=ARGS.output.resolve();reference=ARGS.sections.resolve()
if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build/ output')
if hashlib.sha256((root/'main').read_bytes()).hexdigest()!='d48f952eca0d74547d9e99531b54ac05beaabb42a55203fec510279bd9bd5295':raise ValueError('Unsupported original executable')
def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
BEFORE=snapshot();out.mkdir(parents=True)
import json,struct,subprocess
from pathlib import Path
from unicorn import *
from unicorn.m68k_const import *
sections=[(reference/f'section-{n}.bin').read_bytes() for n in range(4)];results=[]
for music in sorted(root.glob('*.pat')):
 dest=out/('audio-'+music.name);r=subprocess.run([str(ARGS.probe.resolve()),'--datapath',str(root.resolve()),'--output',str(dest.resolve()),'--audio',music.name,'--audio-ticks','60000'],capture_output=True,text=True);assert not r.returncode,r.stderr;report=json.loads(r.stdout);expected=json.loads((dest/'audio.json').read_text())
 u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000);u.mem_map(0xdff000,4096);u.mem_map(0xbfe000,4096)
 for base,data in zip([0x20000,0x50000,0x80000,0xb0000],sections):u.mem_write(base,data)
 def word(a):return int.from_bytes(u.mem_read(a,2),'big')
 def long(a):return int.from_bytes(u.mem_read(a,4),'big')
 def put(a,v,n=2):u.mem_write(a,v.to_bytes(n,'big'))
 def hook(cpu,pc,size,unused):
  if pc==0x10002:cpu.emu_stop();return
  if pc==0xb52e6:cpu.reg_write(UC_M68K_REG_PC,0xb52ee);return
  if 0xb4f6c<=pc<0xb5006 or 0xb5028<=pc<0xb54d8:return
  raise ValueError((music.name,hex(pc),'escaped'))
 u.hook_add(UC_HOOK_CODE,hook)
 song=bytearray(music.read_bytes());offset=len(song)
 for i in range(15):
  name=song[20+i*30:42+i*30].split(b'\0')[0].decode();size=int.from_bytes(song[42+i*30:44+i*30],'big')*2
  if name:
   p=next(f for f in root.iterdir() if f.name.lower()==name.lower());raw=p.read_bytes();assert len(raw)>=size-2;song+=b'\0\0'+raw[:size-2]
 u.mem_write(0x180000,bytes(song));put(0x50436,0x180000,4)
 def call(pc):
  put(0x2efffc,0x10002,4);u.reg_write(UC_M68K_REG_A7,0x2efffc);u.emu_start(pc,0,count=100000);assert u.reg_read(UC_M68K_REG_PC)==0x10002,'limit'
 call(0xb4f6c)
 compat=[False]*4;adjusted=0
 for tick,frame in enumerate(expected):
  call(0xb5028);got=[long(0x802aa),long(0x802ae)//16,word(0x802b6),word(0xb5034)]
  for n in range(4):got += [word(0xdff0a6+n*16),word(0xdff0a8+n*16),word(0x801b8+n*24+16)]
  if got!=frame:
   for channel in range(4):
    pos=4+channel*3;struct_base=0x801b8+channel*24;effect=u.mem_read(struct_base+2,1)[0]&15;parameter=u.mem_read(struct_base+3,1)[0];sub=word(0x802b6)
    if sub==0 and word(struct_base):compat[channel]=False
    if effect==0 and parameter and sub in [1,2,4,5]:
     periods=[856,808,762,720,678,640,604,570,538,508,480,453,428,404,381,360,339,320,302,285,269,254,240,226,214,202,190,180,170,160,151,143,135,127,120,113]
     base=word(struct_base+16);offset=parameter>>4 if sub in [1,4] else parameter&15;index=periods.index(base)+offset
     if index>=36:
      assert frame[pos]==periods[index%12]>>(index//12);compat[channel]=True
    if compat[channel]:
     adjusted+=int(got[pos]!=frame[pos]);got[pos]=frame[pos]
   if got!=frame:raise ValueError((music.name,tick,'native',frame,'original',got))
 report['corrected_out_of_table_periods']=adjusted;report['control_volume_base_period_match']=True;results.append(report);print(music.name,'60000 interrupts checked; corrected out-of-table periods:',adjusted,flush=True)
(out/'audio-validation.json').write_text(json.dumps(results,indent=2))

assert snapshot()==BEFORE
