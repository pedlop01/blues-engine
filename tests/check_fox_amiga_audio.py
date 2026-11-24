#!/usr/bin/env python3
"""Replay PAT interrupt and PCM cases against a private original-verified reference."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('probe','datapath','reference','output'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--windows',action='store_true')
    args=parser.parse_args()
    root,output=args.datapath.resolve(),args.output.resolve()
    if output.exists() or output==root or root in output.parents:raise ValueError('Use a new output outside originals')
    def snapshot():
        return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();output.mkdir(parents=True);reference=args.reference.resolve()
    golden=json.loads((reference/'report.json').read_text())['audio']
    assert golden['sequencer_only'];cases=[]
    for case in golden['cases']:
        name=case['name']
        if Path(name).name!=name or not name.lower().endswith('.pat'):raise ValueError('Invalid PAT name')
        options=['--datapath',str(root),'--output',str(output/name),'--audio',name,'--audio-ticks',str(case['ticks'])]
        if args.windows:
            wrapper=Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'
            result=subprocess.run([sys.executable,str(wrapper),'--program',str(args.probe.resolve()),'--output-dir',str(output/'runner'),'--timeout','45','--',*options],capture_output=True,text=True,timeout=55)
            if not result.stdout:raise ValueError(result.stderr)
            wrapped=json.loads(result.stdout)
            result=subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
        else:result=subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=45)
        if result.returncode:raise ValueError(name+': '+result.stderr)
        report=json.loads(result.stdout);assert not report['playable']
        actual=report['audio'];assert all(actual[k]==case[k] for k in actual),(name,actual,case)
        assert actual['nonzero'] and actual['peak']<32767
        assert json.loads((output/name/'audio.json').read_text())==json.loads((reference/('audio-'+name)/'audio.json').read_text()),(name,'trace')
        assert (output/name/'audio.wav').read_bytes()==(reference/('audio-'+name)/'audio.wav').read_bytes(),(name,'PCM')
        cases.append(actual)
    assert before==snapshot()
    (output/'report.json').write_text(json.dumps(dict(schema_version=1,playable=False,sequencer_only=True,cases=cases,originals_unchanged=True),indent=2)+'\n')
    print(f"{len(cases)} PAT songs, {sum(c['ticks'] for c in cases)} replay ticks and PCM matched; originals intact")


if __name__=='__main__':main()
