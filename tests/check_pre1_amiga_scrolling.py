#!/usr/bin/env python3
"""Compare panning past the first cave seams with independently clipped scenes.

Requires private original-validated scene BMPs (validate_pre1_amiga_levels.py).
"""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ['trace','datapath','reference','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--windows',action='store_true');a=p.parse_args()
out=a.output.resolve();root=a.datapath.resolve()
if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use new private build output')
out.mkdir(parents=True)
cases=[dict(level=1,screen=s,camera=x) for s in [0,1,2] for x in [0,160,287,288,295,319,320,321,450,639,640]]
(out/'cases.json').write_text(json.dumps(cases))
args=[str(root),'scroll',str(out/'cases.json'),str(out/'native.json')]
cmd=[str(a.trace.resolve()),*args]
if a.windows:cmd=[sys.executable,str(ROOT/'tools/run_windows_probe.py'),'--program',str(a.trace.resolve()),'--output-dir',str(out/'runner'),'--timeout','90','--',*args]
r=subprocess.run(cmd,capture_output=True,text=True,timeout=100)
if r.returncode:raise ValueError((r.returncode,r.stdout,r.stderr))
reference=Image.new('RGB',(19*320,200))
for screen in range(19):reference.paste(Image.open(a.reference/f'scene-1-{screen}.bmp').convert('RGB'),(screen*320,0))
for case,native in zip(cases,json.loads((out/'native.json').read_text())):
 camera=case['camera'];expected=reference.crop((camera,22,camera+320,200)).tobytes()
 assert hashlib.sha256(expected).hexdigest()==native['sha256_rgb'],case
report=dict(cases=len(cases),level=1,includes_first_cave_seams=True,terrain_matches_independently_clipped_scenes=True,scope='integer 1x panning, excludes HUD and actors')
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
