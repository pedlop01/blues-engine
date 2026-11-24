#!/usr/bin/env python3
"""Cross-build PAT replay against private original-validated traces and PCM."""
import argparse, hashlib, json, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for name in ['probe','datapath','reference','output']:p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--windows',action='store_true');a=p.parse_args()
root,out,ref=a.datapath.resolve(),a.output.resolve(),a.reference.resolve()
if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build output')
def snapshot():return {str(f.relative_to(root)):(f.stat().st_size,f.stat().st_mtime_ns,hashlib.sha256(f.read_bytes()).hexdigest()) for f in root.rglob('*') if f.is_file()}
before=snapshot();out.mkdir(parents=True);reports=[]
for case in json.loads((ref/'audio-validation.json').read_text()):
 name=case['song']
 if Path(name).name!=name:raise ValueError('Invalid song name')
 dest=out/name
 args=['--datapath',str(root),'--output',str(dest),'--audio',name,'--audio-ticks','12000']
 cmd=[str(a.probe.resolve()),*args]
 if a.windows:cmd=[sys.executable,str(ROOT/'tools/run_windows_probe.py'),'--program',str(a.probe.resolve()),'--output-dir',str(out/'runner'),'--timeout','90','--',*args]
 result=subprocess.run(cmd,capture_output=True,text=True,timeout=100)
 if result.returncode:raise ValueError((name,result.stdout,result.stderr))
 actual=json.loads((dest/'report.json').read_text())
 expected=json.loads((ref/('audio-'+name)/'audio.json').read_text())[:12000]
 assert json.loads((dest/'audio.json').read_text())==expected,(name,'trace')
 assert (dest/'audio.pcm').read_bytes()==(ref/('audio-'+name)/'audio.pcm').read_bytes(),(name,'PCM')
 assert actual['peak']<32767 and actual['nonzero']>0
 reports.append(actual)
assert snapshot()==before
(out/'report.json').write_text(json.dumps(dict(songs=reports,trace_ticks=108000,pcm_byte_exact=True,originals_unchanged=True),indent=2)+'\n')
print('Nine songs, 108000 trace ticks and PCM byte matched; originals intact')
