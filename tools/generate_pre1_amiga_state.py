#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate explicit native save fields; review format changes before replacing source.

Run with --output NEW.cpp, then clang-format and review the diff. Only the
selected simple value structs are supported; this is not a C++ parser.
"""
import argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output",required=True,type=__import__("pathlib").Path)
args=parser.parse_args()
if args.output.exists():raise ValueError("Output already exists")
from pathlib import Path
import re
base=Path(__file__).resolve().parents[1]/'engines/pre1_amiga/logic'
spec=[('core','types.hpp',['Entity']),('systems','player.hpp',['ScoreBonus','PlayerState','InputState','SystemsState']),('systems','boss.hpp',['BossPlayerState']),('systems','boss_l2.hpp',['L2ProjectileSlot','L2BossState']),('systems','boss_l4.hpp',['L4BossState']),('systems','boss_l6.hpp',['L6BossState'])]
out=['// SPDX-License-Identifier: GPL-3.0-or-later','// Explicit field serialization for the isolated Amiga state; no ABI-dependent','// structs, pointer bytes or platform padding are stored.','#include "titus/pre1_amiga_save.hpp"']
for ns,file,names in spec:
 s=re.sub(r'//[^\n]*','',(base/ns/file).read_text())
 out+=['namespace olduvai::'+ns+' {']
 for name in names:
  p=s.index('struct '+name+' {')+len('struct '+name+' {');depth=1;i=p
  while depth:
   if s[i]=='{':depth+=1
   if s[i]=='}':depth-=1
   i+=1
  body=s[p:i-1];fields=[]
  for line in body.split(';'):
   line=line.strip()
   if not line or '(' in line:continue
   # Ignore commas inside template type and array initializer.
   m=re.match(r'(?:std::.*>|int|bool|long|float|PlayerState|InputState|core::CollisionBitmap|ObjType|core::ObjType)\s+(.+)',line,re.S)
   if not m:raise ValueError((name,line))
   tail=re.sub(r'\{.*?\}','',m[1],flags=re.S)
   for part in tail.split(','):
    field=part.split('=')[0].strip()
    if not re.match(r'^\w+$',field):raise ValueError((name,field,line))
    fields.append(field)
  print(name,len(fields))
  out+=['void to_json(titus::Json& j,const '+name+'& v){j=titus::Json::object();']+[' j["'+f+'"]=v.'+f+';' for f in fields]+['}']
  out+=['void from_json(const titus::Json& j,'+name+'& v){if(!j.is_object() || j.size()!='+str(len(fields))+')throw std::runtime_error("Invalid '+name+' save fields");']+[' j.at("'+f+'").get_to(v.'+f+');' for f in fields]+['}']
 out+=['}']
args.output.write_text('\n'.join(out)+'\n')
