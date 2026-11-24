#!/usr/bin/env python3
"""Reproduce actor flicker, backdrop jitter and corrupted death/reload paths.

Runs production rendering and 32 native death/respawn routes using the user's
original data. Test-only time/input hooks do not replace the game simulation.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin-dir',required=True,type=Path)
p.add_argument('--datapath',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('--windows',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
source=str(a.datapath.resolve())
if a.windows:source=subprocess.check_output(['wslpath','-w',source],text=True).strip()
program=(a.bin_dir/('check_ja_presentation'+('.exe' if a.windows else ''))).resolve()
report={'platform':'windows' if a.windows else 'linux','cases':[],'death_routes':32}
for case in ['sprites','resize','backdrop','music','death','scroll','camera','motion']:
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',PF_REGRESSION=case,
             ASAN_OPTIONS='detect_stack_use_after_return=0:detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1')
    if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','PF_REGRESSION']))
    r=subprocess.run([str(program),source],env=env,capture_output=True,text=True,timeout=240)
    (a.output/(case+'.log')).write_text(r.stdout+r.stderr)
    if r.returncode or 'runtime error:' in r.stderr or 'ERROR: AddressSanitizer' in r.stderr:
        raise RuntimeError(f'{case}: {r.returncode}\n{r.stderr[-2000:]}')
    assert 'PASS: Jukebox lifecycle regression' in r.stdout
    report['cases'].append({'case':case,'result':'pass'})
    print(case+': PASS',flush=True)
(a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
