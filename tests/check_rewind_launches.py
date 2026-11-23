#!/usr/bin/env python3
"""Production timeline/game-loop and VHS regressions with real release data."""
import os
from blues_test_paths import data_path
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
binary = os.environ.get('BLUES_TEST_BINARY', str(root / 'build/tests/check_viewport'))
base = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
            BLUES_CONFIG_FILE='', BLUES_SAVE_DIR='', BLUES_TEST_REWIND='1')
for mode in ('dos', 'amiga'):
    cases = [(level, level % 2 == 1, False, False) for level in range(6)]
    cases += [(0, True, True, False), (1, True, False, True)]
    for level, fixed, room, two in cases:
        env = dict(base)
        if room:
            env['BLUES_TEST_REWIND_ROOM'] = '1'
        if two:
            env['BLUES_TEST_REWIND_TWO'] = '1'
            env['BLUES_TEST_REWIND_ODD_SIZE'] = '1'
        args = [binary, '--datapath='+str(data_path(mode)),
                '--dos-datapath='+str(data_path('dos')), '--amiga-datapath='+str(data_path('amiga')),
                '--cheats=7', f'--level={level}', f'--scrollrate={(30, 60, 120)[level % 3]}',
                f'--hud={"fixed" if fixed else "classic"}',
                f'--camera={"classic" if fixed else "smooth"}']
        result = subprocess.run(args, cwd=root, env=env, text=True, capture_output=True, timeout=90)
        if result.returncode:
            raise AssertionError(f'{mode}/{level}, room={room}, two={two}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        print(mode, level, 'fixed' if fixed else 'classic', f'room={room}, two={two}', result.stdout.strip().splitlines()[-1], flush=True)
print('rewind/VHS: all six DOS/Amiga levels, both HUD/cameras, rooms, zoom, two players, irregular window, exact scene framing and production R/LB controls passed')
