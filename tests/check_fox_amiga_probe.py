#!/usr/bin/env python3
"""Generate all local Fox Amiga scenes and reject unsafe/broken requests.

Contains no game data. Outputs must live outside the supplied originals.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--datapath', type=Path, required=True)
    parser.add_argument('--palette', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--windows', action='store_true', help='Run the Windows probe through the WSL development harness')
    args = parser.parse_args()
    root, output = args.datapath.resolve(), args.output.resolve()
    if output.exists() or output == root or root in output.parents:
        raise ValueError('Choose a new output outside originals')
    def snapshot():
        return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns,
                hashlib.sha256(p.read_bytes()).hexdigest())
                for p in root.rglob('*') if p.is_file()}
    before = snapshot()
    def run(out, *extra):
        options=['--datapath',str(root),'--output',str(out),'--palette',str(args.palette.resolve()),*extra]
        if args.windows:
            wrapper=Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'
            result=subprocess.run([sys.executable,str(wrapper),'--program',str(args.probe.resolve()),
                '--output-dir',str(output/'runner'),'--timeout','45','--',*options],
                capture_output=True,text=True,timeout=55)
            if not result.stdout:
                raise ValueError(result.stderr)
            wrapped=json.loads(result.stdout)
            return subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
        return subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=45)
    output.mkdir(parents=True)
    scenes = []
    for level in list(range(1, 15))+[16]:
        process = run(output/f'{level:02}', '--level', str(level))
        if process.returncode:
            raise ValueError(process.stderr)
        result = json.loads(process.stdout)
        assert not result['playable'] and len(result['resources']) == 65
        assert result['sprite_total'] == 398 and len(result['sprites']) == 6
        state=result['level_state']
        assert len(state['collision_queries_sha256'])==64
        assert state['next_level']==(0 if level==16 else 16 if level==14 else level+1)
        images={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (output/f'{level:02}').glob('*.bmp')}
        assert len(images)==12 and {'scene.bmp','collisions.bmp'}<=images.keys()
        scenes.append(dict(level=level,viewport=result['level'],resources=65,sprites=398,
            initialization_hashes={k:v for k,v in state.items() if k.endswith('_sha256')},bmp_sha256=images))
    rejects = [run(root), run(output/'01'), run(output/'missing15', '--level', '15'),
               run(output/'bad-x', '--x', '4096'), run(output/'bad-y', '--y', '99999'),
               run(output/'bad-number', '--level', '1junk'), run(output/'negative', '--level', '-1')]
    assert all(p.returncode and p.stderr for p in rejects)
    assert snapshot() == before
    report = dict(schema_version=1, playable=False, scenes=scenes,
                  rejected_requests=len(rejects), originals_unchanged=True)
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print('15 initialized scenes and collision overlays; seven invalid requests rejected; originals intact')


if __name__ == '__main__':
    main()
