#!/usr/bin/env python3
"""Bounded SDL library integration test; optional original data remains read-only.

Linux uses SDL dummy by default; --visible and --windows exercise a native window.
Windows mode invokes the PE directly through WSL, preserving Unicode arguments.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from validate_contract import loads, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--orig-games', type=Path)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--visible', action='store_true')
    parser.add_argument('--headless', action='store_true')
    args = parser.parse_args()

    def native(p):
        p = str(p.resolve())
        return subprocess.check_output(['wslpath', '-w', p], text=True).strip() if args.windows else p

    def snapshot():
        if not args.orig_games:
            return {}
        root = args.orig_games.resolve()
        return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns,
                hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}

    before = snapshot()
    with tempfile.TemporaryDirectory(prefix='biblioteca á ', dir=ROOT / 'build') as tmp:
        user = Path(tmp) / 'usuario ñ'
        capture = ROOT / 'build' / ('library-windows.bmp' if args.windows else 'library-linux.bmp')
        command = [str(args.binary.resolve()), '--user-dir', native(user), '--smoke-test', '--screenshot', native(capture)]
        if args.orig_games:
            command += ['--test-import', native(args.orig_games)]
        env = os.environ.copy()
        if (not args.windows and not args.visible) or args.headless:
            env['SDL_VIDEODRIVER'] = 'dummy'
        if args.windows and args.headless:
            env['WSLENV'] = ':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER']))
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', env=env, timeout=120)
        assert result.returncode == 0, (result.stdout, result.stderr)
        report = loads(result.stdout)
        assert report['fullscreen'] is False and report['ui_checks'] >= 19, report
        catalog = loads((user / 'config/catalog.json').read_text(encoding='utf-8'))
        validate(catalog)
        preferences = loads((user / 'config/library.json').read_text(encoding='utf-8'))
        assert preferences['selected_game'] == report['selected_game']
        assert preferences['fullscreen'] is False
        assert (preferences['window_width'], preferences['window_height']) == (1100, 720)
        if args.orig_games:
            assert (report['games'], report['installations']) == (5, 8), report
            assert report['ui_checks'] >= 30, report
            assert sum(len(i['locations']) for i in catalog['installations']) == 12
        else:
            assert (report['games'], report['installations']) == (0, 0), report
        assert capture.read_bytes()[:2] == b'BM'
        width, height = struct.unpack_from('<ii', capture.read_bytes(), 18)
        assert width >= 920 and abs(height) >= 640
    assert snapshot() == before, 'Original games changed during UI operations'
    print(json.dumps(report, ensure_ascii=False))
    print(f'Originals unchanged: {len(before)} files; catalog and UI preferences verified')


if __name__ == '__main__':
    main()
