#!/usr/bin/env python3
"""Load real DOS slots across Linux/Windows using private profiles from the session tests."""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--game',choices=['fox','prehistorik'],required=True);p.add_argument('--linux-bin',type=Path,required=True);p.add_argument('--windows-bin',type=Path,required=True);p.add_argument('--linux-profile',type=Path,required=True);p.add_argument('--windows-profile',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 users={'linux':a.linux_profile.resolve(),'windows':a.windows_profile.resolve()};bins={'linux':a.linux_bin.resolve(),'windows':a.windows_bin.resolve()};out=a.output.resolve()
 if out.exists()or not out.is_relative_to(ROOT/'build')or any(not u.is_relative_to(ROOT/'build')for u in users.values()):raise ValueError('Only isolated build profiles are supported')
 out.mkdir(parents=True);env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy');env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
 def native(path,platform):return subprocess.check_output(['wslpath','-w',str(path)],text=True).strip()if platform=='windows'else str(path)
 def invoke(platform,tool,args):
  suffix='.exe'if platform=='windows'else'';r=subprocess.run([str(bins[platform]/(tool+suffix)),*args],env=env,capture_output=True,text=True,timeout=90)
  if r.returncode:raise ValueError((platform,tool,r.returncode,r.stdout,r.stderr))
  return json.loads(r.stdout)
 works={platform:next(u.rglob('slot-1.json')).parent for platform,u in users.items()};results=[]
 for source,target in [('linux','windows'),('windows','linux')]:
  dest=works[target];saved={ext:(dest/('slot-6'+ext)).read_bytes()for ext in ['.json','.bmp']}
  try:
   for ext in saved:(dest/('slot-6'+ext)).write_bytes((works[source]/('slot-1'+ext)).read_bytes())
   catalog=invoke(target,'titus-catalog',['--user-dir',native(users[target],target),'list']);item=next(i for i in catalog['installations']if i['game_id']==a.game and i['edition_id']=='dos')
   result=invoke(target,'titus-session',['--user-dir',native(users[target],target),'--installation',item['id'],'--load-slot','6','--test-ms','250']);assert result['ready']and result['state']=='done',result
   results.append(dict(source=source,target=target,loaded=True))
  finally:
   for ext,data in saved.items():(dest/('slot-6'+ext)).write_bytes(data)
 result=dict(schema_version=1,game=a.game,slot_format=a.game+'.dos.json',checks=results);(out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
