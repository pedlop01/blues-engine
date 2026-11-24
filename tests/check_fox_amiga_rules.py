#!/usr/bin/env python3
"""Verify native executable unpacking/rules against the independent private audit."""
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
    if output.exists() or output==root or root in output.parents:raise ValueError('Use new private output outside originals')
    def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();output.mkdir(parents=True)
    def run(data,name):
        options=['--datapath',str(data),'--output',str(output/name),'--rules-only','1']
        if args.windows:
            wrapper=Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'
            result=subprocess.run([sys.executable,str(wrapper),'--program',str(args.probe.resolve()),'--output-dir',str(output/'runner'),'--timeout','45','--',*options],capture_output=True,text=True,timeout=55)
            if not result.stdout:raise ValueError(result.stderr)
            wrapped=json.loads(result.stdout);return subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
        return subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=45)
    result=run(root,'valid')
    if result.returncode:raise ValueError(result.stderr)
    report=json.loads(result.stdout);gold=json.loads((reference/'report.json').read_text())['native_rules']
    assert not report['playable'] and report['native_section_sha256']==gold['section_sha256'] and report['rules_sha256']==gold['rules_sha256']
    if 'render_palettes' in gold:assert report['render_palettes']==gold['render_palettes'] and report['level_songs']==gold['level_songs']
    assert json.loads((output/'valid/rules.json').read_text())==json.loads((reference/'palette.json').read_text())
    main=next(p for p in root.iterdir() if p.name.lower()=='main').read_bytes()
    for name,raw in [('empty',b''),('truncated',main[:-1]),('modified',bytes([main[0]^1])+main[1:])]:
        private=output/(name+'-data');private.mkdir();(private/'main').write_bytes(raw)
        rejected=run(private,name);assert rejected.returncode and rejected.stderr and not (output/name/'report.json').exists(),name
    assert snapshot()==before
    final=dict(schema_version=1,playable=False,section_sha256=gold['section_sha256'],rules_sha256=gold['rules_sha256'],rejected_cases=3,originals_unchanged=True)
    (output/'report.json').write_text(json.dumps(final,indent=2)+'\n')
    print('Native sections and rules match isolated original unpacker; three invalid revisions rejected; originals intact')


if __name__=='__main__':main()
