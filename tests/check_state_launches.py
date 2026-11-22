#!/usr/bin/env python3
"""Full-state regression matrix and save portability across fresh processes."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
binary = os.environ.get('BLUES_TEST_BINARY', str(root / 'build/tests/check_viewport'))
with tempfile.TemporaryDirectory(prefix='blues-state-') as directory:
    saves = Path(directory) / 'partidas_á'
    saves.mkdir()
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', BLUES_CONFIG_FILE='', BLUES_SAVE_DIR=str(saves))

    def run(mode, level, options=(), settings=None):
        args = [binary, f'--datapath=../orig-games/{mode}', '--dos-datapath=../orig-games/dos', '--amiga-datapath=../orig-games/amiga', '--cheats=7', f'--level={level}'] + list(options)
        result = subprocess.run(args, cwd=root, env=dict(env, **(settings or {})), capture_output=True, text=True, timeout=90)
        if result.returncode:
            raise AssertionError(f'{mode}/{level} {settings}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        print(mode, level, result.stdout.strip().splitlines()[-1], flush=True)

    for mode in ('dos', 'amiga'):
        for level in range(6):
            run(mode, level, settings={'BLUES_TEST_STATE': '1'})
        for scenario in ('room', 'two', 'dead'):
            settings = {'BLUES_TEST_STATE': '1'}
            settings['BLUES_TEST_STATE_ROOM' if scenario == 'room' else 'BLUES_TEST_STATE_TWO'] = '1'
            if scenario == 'dead':
                settings['BLUES_TEST_STATE_DEAD'] = '1'
            options = ['--camera=classic', '--hud=fixed']
            level = 0 if scenario == 'room' else 1
            run(mode, level, options, settings)
            # Fresh address space, another initial mode and level, same future.
            run(mode, level, options, dict(settings, BLUES_TEST_STATE_WRITE='1'))
            run('amiga' if mode == 'dos' else 'dos', 3, settings={'BLUES_TEST_STATE': '1', 'BLUES_TEST_STATE_READ': '1'})
        if mode == 'dos':
            run(mode, 1, ['--dosscroll', '--startpos=57x47'], {'BLUES_TEST_STATE': '1'})
            run(mode, 1, ['--screensize=640x360'], {'BLUES_TEST_STATE': '1', 'BLUES_TEST_STATE_WRITE': '1'})
            run('amiga', 2, settings={'BLUES_TEST_STATE': '1', 'BLUES_TEST_STATE_READ': '1'})
        (saves / 'slot01.bbs').unlink(missing_ok=True)
        run(mode, 1, settings={'BLUES_TEST_STATE_MENU': '1'})
print('state: six levels/releases, rooms, zoom, carried objects, two players, corruption, cross-mode/level/process restore and menu passed')
