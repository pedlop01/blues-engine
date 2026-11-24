#!/usr/bin/env python3
"""All selectable original stages: real render/state/rewind production probes."""
import argparse,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True)
p.add_argument('--sample',action='store_true');p.add_argument('--output',type=Path,required=True);p.add_argument('--windows',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',ASAN_OPTIONS='detect_stack_use_after_return=0:detect_leaks=1',UBSAN_OPTIONS='halt_on_error=1')
if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
report={'platform':'windows' if a.windows else 'linux','stages':[]}
for module,folder,levels in [('ja','blues2-dos',range(1,32)),('p2','prehistorik2-dos',range(16))]:
    if a.sample:levels=[1,16,31] if module=='ja' else [0,5,9,15]
    path=str((a.orig_games/folder).resolve())
    if a.windows:path=subprocess.check_output(['wslpath','-w',path],text=True).strip()
    for level in levels:
        command=[str((a.bin_dir/('check_'+module+'_presentation'+('.exe' if a.windows else ''))).resolve()),path,str(level)]
        run=subprocess.run(command,env=env,capture_output=True,text=True,timeout=60)
        (a.output/f'{module}-{level:02}.log').write_text(run.stdout+run.stderr)
        if run.returncode:raise RuntimeError(f'{module} level {level}: {run.returncode}\n{run.stderr[-1800:]}')
        report['stages'].append({'module':module,'stage':level,'result':'pass'})
        print(f'{module} {level}: PASS',flush=True)
(a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n')
