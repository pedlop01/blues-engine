#!/usr/bin/env python3
"""Replay terrain-only player cases already matched to isolated original ticks."""
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
    if output.exists() or output==root or root in output.parents:
        raise ValueError('Use a new output outside originals')
    def snapshot():
        return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,
            hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();output.mkdir(parents=True)
    reference=args.reference.resolve()
    golden=json.loads((reference/'report.json').read_text())['motion']
    assert golden['terrain_only']
    def run(name,script):
        options=['--datapath',str(root),'--output',str(output/name),
                 '--palette',str(reference/'palette.json'),'--motion',str(script)]
        if args.windows:
            wrapper=Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'
            result=subprocess.run([sys.executable,str(wrapper),'--program',str(args.probe.resolve()),
                '--output-dir',str(output/'runner'),'--timeout','45','--',*options],
                capture_output=True,text=True,timeout=55)
            if not result.stdout:raise ValueError(result.stderr)
            wrapped=json.loads(result.stdout)
            return subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
        return subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=45)
    cases=[]
    for case in golden['cases']:
        name=case['name']
        if not name.isidentifier():raise ValueError('Invalid case name')
        result=run(name,reference/(name+'.json'))
        if result.returncode:raise ValueError(name+': '+result.stderr)
        report=json.loads(result.stdout)
        assert not report['playable'] and report['motion']['terrain_only']
        assert report['motion']['sha256']==case['sha256'] and report['motion']['steps']==case['ticks']
        native=json.loads((output/name/'motion.json').read_text())
        original=json.loads((reference/('motion-'+name)/'motion.json').read_text())
        assert native==original,(name,'motion frame mismatch')
        cases.append(dict(name=name,ticks=case['ticks'],sha256=case['sha256']))
    for name,initial in (('unsupported_actor',dict(state=6)),('outside_map',dict(x=0))):
        script=output/(name+'.json');script.write_text(json.dumps(dict(initial=initial,steps=[{}])))
        result=run(name,script)
        assert result.returncode and result.stderr and not (output/name/'report.json').exists()
    assert before==snapshot()
    report=dict(schema_version=1,playable=False,terrain_only=True,cases=cases,
                rejected_unsupported_cases=2,originals_unchanged=True)
    (output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"{len(cases)} terrain-only cases, {sum(c['ticks'] for c in cases)} ticks matched; originals intact")


if __name__=='__main__':main()
