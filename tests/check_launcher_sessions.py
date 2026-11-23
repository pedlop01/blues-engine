#!/usr/bin/env python3
"""Open DOS and Amiga via the actual SDL Play button and recover the frontend."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from validate_contract import loads, validate, validate_session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--orig-games', type=Path, required=True)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--family', action='store_true')
    args = parser.parse_args()
    binary = args.bin_dir.resolve()
    suffix = '.exe' if args.windows else ''
    def native(p):
        p = str(p.resolve())
        return subprocess.check_output(['wslpath','-w',p], text=True).strip() if args.windows else p
    def snapshot(root):
        return {str(p.relative_to(root)): (p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    originals = args.orig_games.resolve()
    before = snapshot(originals)
    with tempfile.TemporaryDirectory(prefix='library-session á ', dir=ROOT/'build') as directory:
        root = Path(directory);user = root/'usuario ñ';legacy = root/'legacy'
        (legacy/'saves').mkdir(parents=True)
        (legacy/'preferences.ini').write_text('version=1\ncamera=classic\nhud=fixed\nmusic_volume=42\nview_step=2\n')
        (legacy/'saves/slot01.bbs').write_bytes(b'legacy damaged save is retained')
        legacy_before = snapshot(legacy)
        env = os.environ.copy()
        env.update(SDL_VIDEODRIVER='windows' if args.windows and not args.headless else 'dummy',SDL_AUDIODRIVER='dummy',
                   BLUES_CONFIG_FILE=native(legacy/'preferences.ini'),BLUES_SAVE_DIR=native(legacy/'saves'))
        if args.windows:
            env['WSLENV'] = ':'.join(x for x in env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','BLUES_CONFIG_FILE','BLUES_SAVE_DIR'] if x)
        c = subprocess.run([str(binary/('titus-catalog'+suffix)),'--user-dir',native(user),'add',native(originals),'--recursive'],capture_output=True,text=True,encoding='utf-8',env=env,timeout=60)
        assert c.returncode==0,c.stderr
        capture = ROOT/'build'/('library-session-windows.bmp' if args.windows else 'library-session-linux.bmp')
        r = subprocess.run([str(binary/('titus-engine'+suffix)),'--user-dir',native(user),'--family-session-smoke-test' if args.family else '--session-smoke-test','--screenshot',native(capture)],capture_output=True,text=True,encoding='utf-8',env=env,timeout=210 if args.family else 90)
        if r.returncode:
            for p in user.rglob('stderr.log'):
                print(p,p.read_text(errors='replace')[-2000:],file=sys.stderr)
        assert r.returncode==0,(r.stdout,r.stderr)
        report = loads(r.stdout)
        expected=4 if args.family else 2
        assert report['sessions']==expected and report['session_ui_checks']==expected*2 and report['selected_edition']=='amiga'
        count=0
        for folder in (user/'cache/sessions').iterdir():
            req=loads((folder/'request.json').read_text());validate(req)
            if req['kind']=='launch':
                events=[loads(x) for x in (folder/'events.jsonl').read_text().splitlines()]
                result=loads((folder/'result.json').read_text())
                assert validate_session(req,events,result,0)
                count+=1
        assert count==expected
        profile=next((user/'data/profiles').glob('*/games/blues/blues.full.1991'))
        ini=(profile/'native/blues-family/preferences.ini').read_text()
        assert all(x in ini for x in ('camera=classic','hud=fixed','music_volume=42','view_step=2'))
        assert (profile/'saves/slot01.bbs').read_bytes()==(legacy/'saves/slot01.bbs').read_bytes()
        assert snapshot(legacy)==legacy_before
        assert capture.read_bytes()[:2]==b'BM'
        print(json.dumps(report,ensure_ascii=False))
    assert snapshot(originals)==before
    print(f'Originals and legacy preserved: {len(before)} game files; native window/selection restored after every game')


if __name__=='__main__':
    main()
