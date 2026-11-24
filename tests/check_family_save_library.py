#!/usr/bin/env python3
"""Create native slots then load both games through actual launcher controls."""
import argparse,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--windows',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def native(path):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip() if a.windows else str(path.resolve())
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
user=a.output.resolve()/'profile'
def invoke(name,args,timeout=90):
 r=subprocess.run([str((a.bin_dir/(name+('.exe' if a.windows else ''))).resolve()),'--user-dir',native(user),*args],env=env,capture_output=True,text=True,encoding='utf-8',timeout=timeout)
 if r.returncode:
  for log in user.rglob('stderr.log'):print(str(log),log.read_text(errors='replace')[-2000:])
 assert r.returncode==0,(r.stdout,r.stderr)
 return json.loads(r.stdout)
c=invoke('titus-catalog',['add',native(a.orig_games),'--recursive'])
reports={}
for game in ['blues','jukebox','prehistorik2']:
 i=next(x for x in c['installations'] if x['game_id']==game and x['edition_id']=='dos')
 cmd=['--installation',i['id']]
 reports[game]=invoke('titus-session',cmd+['--test-ms','5500','--gameplay-probe'] if game!='blues' else cmd+['--test-ms','200'])
reports['library']=invoke('titus-engine',['--family-save-smoke-test','--screenshot',native(a.output/'library.bmp')],210)
assert reports['library']['loaded_games']==2
# Both launched requests must be load_slot actions and report restored metadata.
restored=list((user/'cache/sessions').glob('*/restored-state.json'));assert len(restored)==2
reports['restored']=[json.loads(x.read_text()) for x in restored]
(a.output/'report.json').write_text(json.dumps(reports,indent=2)+'\n');print(json.dumps(reports['library']))
