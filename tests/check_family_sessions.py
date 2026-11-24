#!/usr/bin/env python3
"""Native Jukebox/P2 routes, truthful capabilities, per-game storage and PCM checks."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from validate_contract import loads,validate,validate_session

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir',type=Path,required=True)
    parser.add_argument('--orig-games',type=Path,required=True)
    parser.add_argument('--windows',action='store_true')
    args=parser.parse_args();binary=args.bin_dir.resolve();originals=args.orig_games.resolve()
    suffix='.exe' if args.windows else ''
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    if args.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
    def native(p):return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip() if args.windows else str(p.resolve())
    def local(s):return Path(subprocess.check_output(['wslpath','-u',s],text=True).strip()) if args.windows else Path(s)
    def snapshot(p):return {str(x.relative_to(p)):(x.stat().st_size,x.stat().st_mtime_ns,hashlib.sha256(x.read_bytes()).hexdigest()) for x in p.rglob('*') if x.is_file()}
    before=snapshot(originals)
    with tempfile.TemporaryDirectory(prefix='familia ñ ',dir=ROOT/'build') as tmp:
        root=Path(tmp);user=root/'perfil ü';legacy=root/'legacy';legacy.mkdir();(legacy/'preferences.ini').write_text('version=1\ninitial_lives_1=99\n')
        def invoke(name,arguments,code=0):
            r=subprocess.run([str(binary/(name+suffix)),*arguments],env=env,capture_output=True,text=True,encoding='utf-8',timeout=90)
            if r.returncode!=code:
                for log in user.rglob('stderr.log'):print(log,log.read_text(errors='replace')[-1500:],file=sys.stderr)
            assert r.returncode==code,(r.stdout,r.stderr)
            return loads(r.stdout)
        catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals),'--recursive'])
        validate(catalog);workspaces=[]
        for game in ['jukebox','prehistorik2']:
            installation=next(i for i in catalog['installations'] if i['game_id']==game)
            command=['--user-dir',native(user),'--installation',installation['id'],'--legacy-root',native(legacy)]
            inspected=invoke('titus-session',command+['--inspect-only']);folder=local(inspected['directory']);req=loads((folder/'request.json').read_text());validate(req);validate(loads((folder/'inspection.json').read_text()))
            profile=local(req['paths']['profile_directory']['path']);work=local(req['paths']['work_directory']['path']);workspaces.append(work)
            assert profile.parent.name==game and not (profile/'migration-receipt.json').exists()
            prefs={'schema_version':1,'fullscreen':False,'music_volume':73 if game=='jukebox' else 61,'effects_volume':100}
            prefs.update(action_button=2,jump_button=3);(work/'preferences.json').write_text(json.dumps(prefs))
            # Route uses full volume for the saturation assertion, then confirms persisted volumes.
            full=dict(prefs,music_volume=100);(work/'preferences.json').write_text(json.dumps(full))
            report=invoke('titus-session',command+['--test-ms','5500','--gameplay-probe','--test-fault','controller']);assert report['state']=='done' and report['ready']
            folder=local(report['directory']);req=loads((folder/'request.json').read_text());validate(req)
            events=[loads(x) for x in (folder/'events.jsonl').read_text().splitlines()];result=loads((folder/'result.json').read_text());assert validate_session(req,events,result,0)
            ready=next(e['body'] for e in events if e['event']=='ready')
            assert ready['save_slot_count']==8 and set(ready['features'])==({'return_to_library','native_pause','fullscreen','separate_volumes','controller_bindings','smooth_camera','dynamic_view','hud_modes','invulnerability','lives_options','level_select','rewind','save_slots','save_thumbnails'}|({'character_select'} if game=='jukebox' else set()))
            observation=loads((folder/'probe-observation.json').read_text())
            assert observation['gameplay'] and observation['pause'] and observation['frames']>=146
            assert observation['right']>40 and observation['left']>20 and observation['jump']>20,observation
            assert observation['samples']>10000 and observation['nonzero']>1000 and observation['effects']>0 and observation['peak']>1000,observation
            assert observation['clipped']==0 and observation['rewind'],observation
            shot=folder/'gameplay.bmp';assert shot.read_bytes()[:2]==b'BM'
            import shutil
            shutil.copyfile(shot,ROOT/'build'/f'{game}-managed-{"windows" if args.windows else "linux"}.bmp')
            saved=loads((work/'preferences.json').read_text());assert all(saved[k]==v for k,v in full.items());assert saved['camera']=='smooth'
            (work/'preferences.json').write_text(json.dumps(prefs))
            invoke('titus-session',command+['--test-ms','200'])
            saved=loads((work/'preferences.json').read_text());assert all(saved[k]==v for k,v in prefs.items())
            assert (work/'preferences.ini').exists() and (profile/'saves/slot01.bbs').exists()
            # Reopen a real saved frame from a new managed process, bypassing title/menu.
            inspected=invoke('titus-session',command+['--inspect-only'])
            index=inspected['inspection']['save_index'];assert index['slot_count']==8
            slot=next(x for x in index['slots'] if x['slot']==1);assert slot['status']=='ready' and slot['has_preview']
            details=loads((local(inspected['directory'])/'save-details.json').read_text())
            summary=next(x for x in details['slots'] if x['slot']==1)
            defaults=loads((work/'preferences.json').read_text())
            loaded=invoke('titus-session',command+['--load-slot','1','--test-ms','200'])
            after_load=loads((work/'preferences.json').read_text())
            assert all(after_load[k]==defaults[k] for k in ['camera','hud','view_step','invulnerable','infinite_lives'])
            restored=loads((local(loaded['directory'])/'restored-state.json').read_text())
            assert all(restored[k]==summary[k] for k in ['level','x','y','frame','camera','hud','view']), (restored,summary)
            assert (profile/'artwork-dos.bmp').read_bytes()[:2]==b'BM'
            print(game,json.dumps(observation),flush=True)
            # A future preferences version must survive untouched and fail before playing.
            future=json.dumps(dict(prefs,schema_version=2));(work/'preferences.json').write_text(future)
            failed=invoke('titus-session',command+['--test-ms','200'],1)
            assert failed['state']=='failed' and not failed['ready'] and (work/'preferences.json').read_text()==future
            (work/'preferences.json').write_text(json.dumps(prefs))
        assert workspaces[0]!=workspaces[1]
        assert (legacy/'preferences.ini').read_text()=='version=1\ninitial_lives_1=99\n'
    assert snapshot(originals)==before
    print(f'OK: two native routes, separate profiles/preferences, zero clipped PCM samples; {len(before)} originals unchanged')
if __name__=='__main__':main()
