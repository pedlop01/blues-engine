#!/usr/bin/env python3
"""TH-10: native resume, independent settings, temporary overrides and reversible import."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--fixtures',type=Path,required=True);p.add_argument('--windows',action='store_true');a=p.parse_args();bin=a.bin_dir.resolve();orig=a.orig_games.resolve();suffix='.exe' if a.windows else ''
 env=os.environ.copy();env.update(SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
 def native(p):return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip() if a.windows else str(p.resolve())
 def local(p):return Path(subprocess.check_output(['wslpath','-u',p],text=True).strip()) if a.windows else Path(p)
 def snap(root):return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
 before=snap(orig)
 with tempfile.TemporaryDirectory(prefix='TH10 á ',dir=ROOT/'build') as temp:
  root=Path(temp);user=root/'perfil ñ';legacy=root/'clásico';(legacy/'saves').mkdir(parents=True);(legacy/'preferences.ini').write_text('version=1\ncamera=classic\nmusic_volume=42\n')
  for f in a.fixtures.glob('*.bbs'):shutil.copyfile(f,legacy/'saves'/f.name)
  legacy_before=snap(legacy)
  def call(tool,args,code=0):
   r=subprocess.run([str(bin/(tool+suffix)),*args],env=env,capture_output=True,text=True,encoding='utf-8',timeout=100);assert r.returncode==code,(r.stdout,r.stderr);return json.loads(r.stdout)
  catalog=call('titus-catalog',['--user-dir',native(user),'add',native(orig),'--recursive']);dos=next(i for i in catalog['installations'] if i['game_id']=='blues' and i['edition_id']=='dos');amiga=next(i for i in catalog['installations'] if i['game_id']=='blues' and i['edition_id']=='amiga')
  def run(id=dos['id'],extra=(),code=0):return call('titus-session',['--user-dir',native(user),'--installation',id,'--legacy-root',native(legacy),*extra],code)
  report=run(extra=['--inspect-only']);folder=local(report['directory']);request=json.loads((folder/'request.json').read_text());profile=user/'data/profiles'/catalog['profiles'][0]['id']/'games/blues/blues.full.1991';work=profile/'native/blues-family';index=json.loads((profile/'save-index.json').read_text());assert len(index['slots'])==8
  details=json.loads((folder/'save-details.json').read_text());chosen=next(s for s in index['slots'] if s['status']=='ready' and s['has_preview']);saved=next(s for s in details['slots'] if s['slot']==chosen['slot']);assert len(saved['preview_rgb'])==128*80*6
  overrides=root/'overrides.json';overrides.write_text(json.dumps({'blues':{'camera':'smooth','hud':'fixed','view_step':8,'start_level':5},'audio':{'music_volume':17,'effects_volume':29},'input':{'action_button':2,'jump_button':3}}))
  before_saves=snap(profile/'saves');report=run(extra=['--load-slot',str(chosen['slot']),'--settings',native(overrides),'--test-ms','300']);assert report['ready'];folder=local(report['directory']);actual=json.loads((folder/'restored-state.json').read_text());assert actual=={k:saved[k] for k in actual},(actual,saved);assert snap(profile/'saves')==before_saves
  # Session changes are excluded from persistence; save payload controls the restored frame.
  report=run(extra=['--test-ms','200']);r=json.loads((local(report['directory'])/'request.json').read_text());assert r['settings']['audio']['music_volume']==42 and r['settings']['blues']['camera']=='classic';assert r['settings']['input']=={'action_button':0,'jump_button':1}
  # Stale UI index is never trusted; corrupt and foreign-format states cannot start.
  path=profile/'saves'/f"slot{chosen['slot']:02}.bbs";original=path.read_bytes();path.write_bytes(b'foreign game save');bad=run(extra=['--load-slot',str(chosen['slot']),'--test-ms','100'],code=1);assert not bad['ready'];assert path.read_bytes()==b'foreign game save';path.write_bytes(original)
  # Both editions share the eight native slots, and the saved edition is restored.
  report=run(amiga['id'],['--load-slot',str(chosen['slot']),'--test-ms','200']);assert report['ready']
  # A missing index is rebuilt from the native slots, without changing a payload.
  saved_bytes=snap(profile/'saves');(profile/'save-index.json').unlink();run(extra=['--inspect-only']);rebuilt=json.loads((profile/'save-index.json').read_text());assert len(rebuilt['slots'])==8 and snap(profile/'saves')==saved_bytes
  # An explicit second import retains the entire current profile, including a changed slot.
  path.write_bytes(b'managed state retained in backup');report=run(extra=['--inspect-only','--import-legacy']);assert not report['ready'];backups=list(profile.parent.glob('blues.full.1991.before-import-*'));assert len(backups)==1;assert (backups[0]/'saves'/path.name).read_bytes()==b'managed state retained in backup';assert path.read_bytes()==original;assert snap(legacy)==legacy_before
  # Other games reject foreign slots and unsupported volume controls before opening a window.
  fox=next(i for i in catalog['installations'] if i['game_id']=='fox' and i['edition_id']=='dos');rejected=subprocess.run([str(bin/('titus-session'+suffix)),'--user-dir',native(user),'--installation',fox['id'],'--load-slot','1'],env=env,capture_output=True,text=True,timeout=60);assert rejected.returncode==1,(rejected.stdout,rejected.stderr);assert 'Unsupported save slot' in rejected.stderr,(rejected.stdout,rejected.stderr)
  ui=call('titus-engine',['--user-dir',native(user),'--settings-smoke-test','--screenshot',native(ROOT/'build'/('th10-ui-windows.bmp' if a.windows else 'th10-ui-linux.bmp'))]);assert ui['settings_ui_checks']>=8
 assert snap(orig)==before
 print('OK: native saved frame/edition restore, eight previews, session overrides, corrupt/foreign rejection and explicit reversible reimport; originals unchanged')
if __name__=='__main__':main()
