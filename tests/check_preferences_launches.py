#!/usr/bin/env python3
"""Real game process restarts, CLI precedence and persistent menu selections."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
args = [os.environ.get('BLUES_TEST_BINARY', str(root / 'build/tests/check_viewport')), '--dos-datapath=../orig-games/dos', '--amiga-datapath=../orig-games/amiga']
with tempfile.TemporaryDirectory(prefix='blues-prefs-') as directory:
    config = Path(directory) / 'preferencias_á.ini'
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', BLUES_CONFIG_FILE=str(config))

    def run(settings, extra=()):
        result = subprocess.run(args + list(extra), cwd=root, env=dict(env, **settings), text=True, capture_output=True, timeout=60)
        if result.returncode:
            raise AssertionError(f'{settings}: {result.returncode}\n{result.stdout}\n{result.stderr}')
        print(result.stdout.strip())

    run({'BLUES_TEST_OPTIONS': '1'}, ['--cheats=0', '--level=1'])
    text = config.read_text()
    assert 'camera=classic' in text and 'hud=fixed' in text and 'mode=amiga' in text
    assert 'key_4=120' in text and 'button_1=2' in text
    run({'BLUES_TEST_PREFS_EXPECT': 'options'})
    run({'BLUES_TEST_PREFS_EXPECT': 'override'}, ['--camera=smooth', '--hud=classic', '--scrollrate=30'])
    # Restore the fixture, then change assists through the actual game/menu path.
    config.write_text(text)
    run({'BLUES_TEST_LEVELS': '1'})
    run({'BLUES_TEST_PREFS_EXPECT': 'assists', 'BLUES_TEST_PREFS_LOSE_LIFE': '1'})
    run({'BLUES_TEST_PREFS_EXPECT': 'assists'})
    assert 'initial_lives_1=17' in config.read_text()
    run({'BLUES_TEST_RESET_PREFS': '1'})
    run({'BLUES_TEST_PREFS_EXPECT': 'reset'})
print('preferences: separate processes, Unicode path, saved presentation/controls/assists and CLI precedence passed')
