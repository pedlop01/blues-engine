#!/usr/bin/env python3
"""TH-12: repeated six-game sessions, shared defaults, isolation and recovery."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from validate_contract import loads,validate,validate_session

def snapshot(root):
 return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--windows',action='store_true');a=p.parse_args()
 binary=a.bin_dir.resolve();originals=a.orig_games.resolve();before=snapshot(originals);suffix='.exe' if a.windows else '';env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
 def native(p):return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip() if a.windows else str(p.resolve())
 def local(p):return Path(subprocess.check_output(['wslpath','-u',p],text=True).strip()) if a.windows else Path(p)
 operations=0
 with tempfile.TemporaryDirectory(prefix='TH12 expansión ñ ',dir=ROOT/'build') as temp:
  root=Path(temp);user=root/('configuración ü '+'larga '*22).rstrip();legacy=root/'legado aislado';legacy.mkdir()
  def invoke(tool,args,code=0,package=binary):
   nonlocal operations
   r=subprocess.run([str(package/(tool+suffix)),*args],env=env,capture_output=True,text=True,encoding='utf-8',timeout=100);operations+=1
   if r.returncode!=code:
    for log in user.rglob('stderr.log'):print(log,log.read_text(errors='replace')[-1500:],file=sys.stderr)
   assert r.returncode==code,(r.returncode,r.stdout,r.stderr)
   return loads(r.stdout) if r.stdout.strip() else r.stderr
  catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals),'--recursive']);validate(catalog)
  assert len(catalog['installations'])==8 and len({x['game_id'] for x in catalog['installations']})==5
  settings={'schema_version':1,'revision':0,'global':{'video':{'fullscreen':False},'audio':{'music_volume':37,'effects_volume':59},'input':{'action_button':2,'jump_button':3},'blues':{'camera':'classic','hud':'fixed'}},'profiles':{}}
  (user/'config/game-settings.json').write_text(json.dumps(settings))
  checks={};profiles={};channels=[]
  choices=[('blues','dos'),('jukebox','dos'),('prehistorik2','dos'),('fox','dos'),('prehistorik','dos'),('blues','amiga')]
  def run(game,edition,extra=(),code=0):
   item=next(x for x in catalog['installations'] if x['game_id']==game and x['edition_id']==edition)
   r=invoke('titus-session',['--user-dir',native(user),'--installation',item['id'],'--legacy-root',native(legacy),'--test-ms','150',*extra],code)
   if isinstance(r,dict) and (local(r['directory'])/'events.jsonl').exists():
    folder=local(r['directory']);request=loads((folder/'request.json').read_text());validate(request);result=loads((folder/'result.json').read_text());events=[loads(s) for s in (folder/'events.jsonl').read_text().splitlines()]
    assert validate_session(request,events,result,0 if code==0 else 1)==(code==0);channels.append((game,edition))
    if code==0:
     assert r['ready'] and result['ready'];actual=request['settings'];assert actual['video']=={'fullscreen':False}
     if game in ['blues','jukebox','prehistorik2']:
      assert actual['audio']=={'music_volume':37,'effects_volume':59} and actual['input']=={'action_button':2,'jump_button':3}
     else:assert set(actual)=={'video'},actual
     if game!='blues':assert 'blues' not in actual
    return r,request
   return r,None
  def check_other_profiles(game):
   for other,record in profiles.items():
    if other!=game:assert snapshot(record[0])==record[1],f'{game} changed {other} native data'
  for repeat in range(2):
   for game,edition in choices:
    check_other_profiles(game);r,request=run(game,edition);check_other_profiles(game)
    path=local(request['paths']['profile_directory']['path']);profiles[game]=(path,snapshot(path));assert not list(path.rglob('managed-settings-pending.json'))
    checks.setdefault(game+'_'+edition,{'restarts':0,'error_recovery':False})['restarts']+=1
    print('repeat',repeat+1,game,edition,'OK',flush=True)
  for game,edition in choices:
   check_other_profiles(game);failed,request=run(game,edition,['--test-fault','before_ready'],1);assert not failed['ready'];check_other_profiles(game)
   profiles[game]=(profiles[game][0],snapshot(profiles[game][0]));r,request=run(game,edition);check_other_profiles(game);assert r['ready']
   path=profiles[game][0];assert not list(path.rglob('managed-settings-pending.json'));profiles[game]=(path,snapshot(path));checks[game+'_'+edition]['error_recovery']=True
   print('recovery',game,edition,'OK',flush=True)
  for game in ['fox','prehistorik']:
   result,request=run(game,'amiga',code=1);assert isinstance(result,str) and 'disponible para jugar' in result
  # Constructor/preflight failure: a private package missing its installed runtime.
  package=root/'paquete incompleto';(package/'engines').mkdir(parents=True);shutil.copy2(binary/('titus-session'+suffix),package/('titus-session'+suffix));shutil.copy2(binary/'engines/registry.json',package/'engines/registry.json')
  if a.windows:shutil.copy2(binary/'SDL2.dll',package/'SDL2.dll')
  item=next(x for x in catalog['installations'] if x['game_id']=='blues' and x['edition_id']=='dos')
  failure=invoke('titus-session',['--user-dir',native(user),'--installation',item['id'],'--legacy-root',native(legacy),'--test-ms','150'],1,package);assert isinstance(failure,str) and 'Falta el motor' in failure
  # Explicit invalid slot selection must not silently start a new game.
  failure=invoke('titus-session',['--user-dir',native(user),'--installation',item['id'],'--load-slot','0','--test-ms','150'],1);assert isinstance(failure,str) and 'slot' in failure.lower()
  # An unavailable or changed source is tested only on an owned copy, never orig-games.
  own=root/'datos propios';own.mkdir();original=originals/'prehistorik2-dos';names={p.name.lower():p for p in original.iterdir() if p.is_file()}
  for name in ['pre1.exe','filesa.cur','filesb.cur','filesa.vga','filesb.vga']:shutil.copy2(names[name],own/name)
  isolated=root/'otra biblioteca';own_catalog=invoke('titus-catalog',['--user-dir',native(isolated),'add',native(own)]);own_id=own_catalog['installations'][0]['id'];(own/'pre1.exe').unlink()
  failure=invoke('titus-session',['--user-dir',native(isolated),'--installation',own_id,'--legacy-root',native(legacy),'--inspect-only'],1);assert isinstance(failure,str)
  shutil.copy2(names['pre1.exe'],own/'pre1.exe');restored=invoke('titus-session',['--user-dir',native(isolated),'--installation',own_id,'--legacy-root',native(legacy),'--inspect-only']);assert restored['state']=='done' and not restored['ready']
  assert len(channels)==24,(len(channels),channels)
  assert snapshot(originals)==before
  print(json.dumps({'games':checks,'protocol_sessions':len(channels),'operations':operations,'original_files_unchanged':len(before),'unsupported_amiga_rejected':2,'missing_runtime_rejected':True,'changed_source_recovered':True,'long_unicode_storage':True}),flush=True)
if __name__=='__main__':main()
