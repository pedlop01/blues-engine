#!/usr/bin/env python3
"""Replay native campaign/state/render fixtures; optionally compare another build.

These are fixtures, not an input-only complete campaign playthrough.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
for name in ('probe', 'datapath', 'output'):
    p.add_argument('--'+name, type=Path, required=True)
p.add_argument('--reference', type=Path)
p.add_argument('--windows', action='store_true')
a = p.parse_args()
root, out = a.datapath.resolve(), a.output.resolve()
if out.exists() or not out.is_relative_to(ROOT/'build'):
    raise ValueError('Use a new private build output')
def snapshot():
    return {str(f.relative_to(root)): (f.stat().st_size, f.stat().st_mtime_ns,
            hashlib.sha256(f.read_bytes()).hexdigest()) for f in root.rglob('*') if f.is_file()}
before = snapshot()
out.mkdir(parents=True)
args = [str(root)]
if a.windows:
    command = [sys.executable, str(ROOT/'tools/run_windows_probe.py'), '--program',
               str(a.probe.resolve()), '--output-dir', str(out/'runner'), '--timeout', '180', '--', *args]
else:
    command = [str(a.probe.resolve()), *args]
r = subprocess.run(command, capture_output=True, text=True, timeout=190)
if r.returncode:
    raise ValueError((r.returncode, r.stderr, r.stdout))
report = json.loads(r.stdout)
if a.windows:
    assert report['exit'] == 0, report
    report = json.loads(report['stdout'])
assert len(report['levels']) == 7 and len(report['exit_fixtures']) == 7
assert len(report['render_hashes']) == 42
assert report['neighbour_fixtures'] == 5
assert report['room_fixtures'] == 55 and report['trunk_save_and_crossing']
if a.reference:
    assert report == json.loads(a.reference.read_text()), 'Cross-build state/render divergence'
assert before == snapshot(), 'Original resources changed'
(out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
print('Seven levels, saves/continuation, replay, 55 rooms, seven exits/finale and 42 render hashes passed; originals intact')
