#!/usr/bin/env python3
"""Eight atomic state/preview slots and independent-process restore."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
binary = os.environ.get('BLUES_TEST_BINARY', str(root / 'build/tests/check_viewport'))
with tempfile.TemporaryDirectory(prefix='blues-slots-') as directory:
    saves = Path(directory) / 'partidas_á'
    saves.mkdir()
    base = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', BLUES_CONFIG_FILE='', BLUES_SAVE_DIR=str(saves), BLUES_TEST_SLOTS='1')
    def run(mode, level, hud, read=False, two=False, odd=False):
        env = dict(base)
        if read:
            env['BLUES_TEST_SLOTS_READ'] = '1'
        if two:
            env['BLUES_TEST_SLOTS_TWO'] = '1'
        if odd:
            env['BLUES_TEST_SLOTS_ODD_SIZE'] = '1'
        args = [binary, f'--datapath=../orig-games/{mode}', '--dos-datapath=../orig-games/dos', '--amiga-datapath=../orig-games/amiga', f'--level={level}', f'--hud={hud}']
        result = subprocess.run(args, cwd=root, env=env, capture_output=True, text=True, timeout=90)
        if result.returncode:
            raise AssertionError(f'{mode}/{level}/{hud}, read={read}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        print(mode, level, hud, result.stdout.strip().splitlines()[-1], flush=True)
    for mode in ('dos', 'amiga'):
        for level in (0, 2, 5):
            hud = 'fixed' if level == 2 else 'classic'
            run(mode, level, hud, two=level == 2)
            assert len(list(saves.glob('slot??.bbs'))) == 8
            assert not list(saves.glob('*.tmp.*'))
            run('amiga' if mode == 'dos' else 'dos', (level + 1) % 6, 'classic', read=True)
    run('dos', 1, 'fixed', odd=True)
    run('amiga', 3, 'classic', read=True, odd=True)
print('slots: eight independent files, scene thumbnails, metadata, legacy migration, compatibility states and fresh-process restore passed')
