#!/usr/bin/env python3
"""Pixel regressions for guarded scrolling and screen-fixed Amiga copper."""
import os
from blues_test_paths import data_path
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
binary = os.environ.get('BLUES_TEST_BINARY', str(root / 'build/tests/check_viewport'))
base = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
            BLUES_CONFIG_FILE='', BLUES_SAVE_DIR='', BLUES_TEST_SCROLL_EDGES='1')
for mode in ('amiga', 'dos'):
    for level in range(6):
        for hud in ('classic', 'fixed'):
            args = [binary, '--datapath='+str(data_path(mode)),
                    '--dos-datapath='+str(data_path('dos')), '--amiga-datapath='+str(data_path('amiga')),
                    f'--level={level}', f'--hud={hud}', '--scrollrate=60']
            result = subprocess.run(args, cwd=root, env=base, capture_output=True, text=True, timeout=90)
            if result.returncode:
                raise AssertionError(f'{mode}/{level}/{hud}: {result.returncode}\n{result.stdout}\n{result.stderr}')
            print(mode, level, hud, result.stdout.strip().splitlines()[-1], flush=True)
print('scroll: six DOS/Amiga stages, both native HUD layouts, 1x/60 Hz, all borders, diagonals, reversals and stationary copper colors passed')
