#!/usr/bin/env python3
"""Replay object operations matched to isolated original Amiga routines."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('probe','datapath','reference','output'):parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--windows',action='store_true');args=parser.parse_args()
    root,reference,output=args.datapath.resolve(),args.reference.resolve(),args.output.resolve()
    if output.exists() or output==root or root in output.parents:raise ValueError('Use a new output outside originals')
    def snapshot():
        return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();output.mkdir(parents=True)
    def run(name,level,script):
        options=['--datapath',str(root),'--output',str(output/name),'--palette',str(reference/'palette.json'),'--level',str(level),'--objects',str(script)]
        if args.windows:
            wrapper=Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'
            result=subprocess.run([sys.executable,str(wrapper),'--program',str(args.probe.resolve()),'--output-dir',str(output/'runner'),'--timeout','45','--',*options],capture_output=True,text=True,timeout=55)
            if not result.stdout:raise ValueError(result.stderr)
            wrapped=json.loads(result.stdout);return subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
        return subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=45)
    cases=[]
    for case in json.loads((reference/'report.json').read_text())['objects']['cases']:
        if not case['name'].isidentifier():raise ValueError('Invalid case name')
        name=f"objects-{case['level']}-{case['name']}"
        result=run(name,case['level'],reference/(name+'.json'))
        if result.returncode:raise ValueError(name+': '+result.stderr)
        report=json.loads(result.stdout);assert not report['playable'] and not report['objects']['actors_simulated']
        assert report['objects']['sha256']==case['sha256']
        assert json.loads((output/name/'objects.json').read_text())==json.loads((reference/name/'objects.json').read_text()),name
        cases.append(case)
    fixture=dict(objects=[dict(slot=5,kind=3,x=210,y=415)],steps=[])
    invalids={
        'invalid_slot':dict(objects=[dict(slot=40,kind=3,x=210,y=415)],steps=[]),
        'invalid_kind':dict(objects=[dict(slot=5,kind=72,x=210,y=415)],steps=[]),
        'invalid_carried':dict(fixture,carried=40),
        'double_pick':dict(fixture,steps=[dict(action='pick',x=209,y=415,right=True)]*2),
        'attach_overflow':dict(fixture,carried=5,steps=[dict(action='attach',x=200,y=415,dx=2147483647)]),
        'unknown_action':dict(fixture,steps=[dict(action='invented')]),
    }
    for name,script in invalids.items():
        path=output/(name+'.json');path.write_text(json.dumps(script));result=run(name,1,path)
        assert result.returncode and result.stderr and not (output/name/'report.json').exists(),name
    assert before==snapshot()
    (output/'report.json').write_text(json.dumps(dict(schema_version=1,playable=False,actors_simulated=False,cases=cases,rejected_cases=len(invalids),originals_unchanged=True),indent=2)+'\n')
    print(f"{len(cases)} object cases, {sum(c['steps'] for c in cases)} operations matched; {len(invalids)} invalid cases rejected; originals intact")


if __name__=='__main__':main()
