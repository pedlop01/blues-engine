#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Research-only isolated original routines, never a runtime dependency.
import argparse,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
for key in ['datapath','sections','trace','output']:parser.add_argument('--'+key,type=Path,required=True)
parser.add_argument('--states-only',action='store_true')
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
sections=[(reference/f'section-{n}.bin').read_bytes() for n in range(4)]
sizes=[2,8,6,8,8,12,10,10,14,10,26,26,26,26,14,18,16,8,8,8,10,8,8,12,10,8,26,26,8,8,12,12,14,26,26,18,14,6,26,26]
cases=[dict(level=n,screen=s,x=-1000,y=-1000,ticks=120) for n in [1,3,5,7] for s in range(19)]
seen=set();extra=set()
for n in [1,3,5,7]:
 dat=(root/f'level{n}.dat').read_bytes();offset=4+int.from_bytes(dat[:2],'big');screen=0
 while int.from_bytes(dat[offset:offset+2],'big')!=255:
  typ=int.from_bytes(dat[offset:offset+2],'big')
  if typ in [10,11,12,13,26,27,33,34,38,39] and typ not in seen:
   seen.add(typ)
   for state in [1,4,5,6]:extra.add((n,screen,state))
  offset+=sizes[typ]
  if typ==0:screen+=1
if ARGS.states_only:cases=[]
cases += [dict(level=n,screen=s,x=-1000,y=-1000,ticks=120,monster_state=state) for n,s,state in sorted(extra)]
(out/'entities-cases.json').write_text(json.dumps(cases));subprocess.run([str(ARGS.trace.resolve()),str(root),'entities',str(out/'entities-cases.json'),str(out/'entities-native.json')],check=True)
metadata={r['name'].lower():r.get('sprites') for r in json.loads((reference/'native/report.json').read_text())['resources']};expected=json.loads((out/'entities-native.json').read_text());result=[]
for case,frames in zip(cases,expected):
 n=case['level'];dat=(root/f'level{n}.dat').read_bytes();start=4+int.from_bytes(dat[:2],'big');screen=0;p=start;records=[]
 while screen<case['screen']:
  typ=int.from_bytes(dat[p:p+2],'big');p+=sizes[typ]
  if not typ:screen+=1
 first=p
 while dat[p:p+2]!=b'\0\0':
  typ=int.from_bytes(dat[p:p+2],'big');records.append((typ,p-first));p+=sizes[typ]
 raw=dat[first:p]+b'\0\0\0\xff'
 u=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);u.ctl_set_cpu_model(UC_CPU_M68K_M68000);u.mem_map(0x10000,0x300000)
 for base,data in zip([0x20000,0x50000,0x80000,0xb0000],sections):u.mem_write(base,data)
 bank=metadata['char_set1.mat']+metadata[f'l{n}spr.mat'];u.mem_write(0x50004,(0x200000).to_bytes(4,'big'));u.mem_write(0x509a4,(len(metadata['char_set1.mat'])).to_bytes(2,'big'))
 for index,sprite in enumerate(bank):u.mem_write(0x200000+index*20,struct.pack('>HH',sprite['width'],sprite['height'])+bytes(16))
 u.mem_write(0x180000,raw);u.mem_write(0x1a0000,(reference/f'collision-{n}-{case["screen"]}.bin').read_bytes())
 def word(a):return int.from_bytes(u.mem_read(a,2),'big',signed=True)
 def long(a):return int.from_bytes(u.mem_read(a,4),'big')
 def put(a,v,size=2):u.mem_write(a,(v&((1<<(size*8))-1)).to_bytes(size,'big'))
 def ret():
  sp=u.reg_read(UC_M68K_REG_A7);u.reg_write(UC_M68K_REG_PC,long(sp));u.reg_write(UC_M68K_REG_A7,sp+4);u.reg_write(UC_M68K_REG_D0,0)
 def hook(cpu,pc,size,unused):
  if pc==0x10002:cpu.emu_stop();return
  if pc in [0xbea7e,0xb130a,0xb6de4,0xb6e1a,0xb6e46,0xbe5d2]:ret();return
  if 0xb7008<=pc<0xbcd98 or 0xb2a38<=pc<0xb2a50 or 0xc5230<=pc<0xc54de:return
  raise ValueError((case,hex(pc),'escaped'))
 u.hook_add(UC_HOOK_CODE,hook)
 def call(pc,args=[]):
  sp=0x2effd0;put(sp,0x10002,4)
  for i,v in enumerate(args):put(sp+4+i*4,v,4)
  u.reg_write(UC_M68K_REG_A7,sp);u.emu_start(pc,0,count=2000000);assert u.reg_read(UC_M68K_REG_PC)==0x10002,'limit'
 put(0x82008,0,4);call(0xb74c2,[0x180000]);
 if 'monster_state' in case:
  for typ,off in records:
   if typ in [10,11,12,13,26,27,33,34,38,39]:put(0x180000+off+10,case['monster_state']);put(0x180000+off+20,0);put(0x180000+off+22,40 if case['monster_state']==5 else 0)
 put(0x509a0,0x180000,4);put(0x50d6c,0x1a0000,4);put(0x508f0,-1000);put(0x508f2,-1000);put(0x50988,n);put(0x5097e,3);put(0x50980,10)
 for tick,frame in enumerate(frames):
  put(0x808c8,int(tick%2==0));
  try:call(0xba264)
  except Exception as e:raise ValueError((case,tick,hex(u.reg_read(UC_M68K_REG_PC)),hex(u.reg_read(UC_M68K_REG_A0)),str(e)))
  assert long(0x82008)==frame['rng'],(case,tick,'rng',long(0x82008),frame['rng'])
  ei=0
  for typ,off in records:
   e=frame['entities'][ei];ei+=2 if typ==8 else 1
   if typ in [10,11,12,13,26,27,33,34,38,39]:fields=[(4,'state'),(5,'respawns'),(6,'x'),(7,'y'),(8,'direction'),(9,'state_counter'),(10,'ko_counter')]
   elif typ==35:fields=list(enumerate(['anchor_a','anchor_b','x','wing_x','body_phase','wing_phase','body_subcounter','wing_subcounter']))
   elif typ in [9,31,36]:fields=[(0,'x'),(1,'y'),(2,'dy'),(3,'state')]
   elif typ in [14,32]:fields=list(enumerate(['state','x','y','throw_flag','throw_x','throw_y']))
   elif typ==16:fields=[(0,'x'),(2,'current_y'),(3,'y_top'),(4,'y_bottom'),(5,'direction'),(6,'speed')]
   elif typ in [3,4,28]:fields=[(2,'counter')]
   else:continue
   got=[word(0x180000+off+2+i*2) for i,f in fields];want=[e[f] for i,f in fields]
   if got!=want:raise ValueError((case,tick,typ,fields,'original',got,'native',want))
 result.append(case);print('match',case,flush=True)
(out/'entities-validation.json').write_text(json.dumps(result,indent=2))

assert snapshot()==BEFORE
(out/'report.json').write_text(json.dumps(dict(schema_version=1,cases=len(result),ticks=sum(c['ticks'] for c in result),originals_unchanged=True,scope='autonomous actor RNG plus selected mutable fields, distant player, no player contacts'),indent=2)+'\n')
