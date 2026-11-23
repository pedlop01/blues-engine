#!/usr/bin/env python3
"""Fox native inspection/preparation, private caching, gameplay/audio and failures."""
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
from validate_contract import loads,validate,validate_inspection,validate_session
MUSIC='386dbe1cad7a67e20ec3edb98bdb116c035e68cc762cd85762d2bedbb1215986'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--windows',action='store_true');a=p.parse_args()
    binary=a.bin_dir.resolve();originals=a.orig_games.resolve();suffix='.exe' if a.windows else ''
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
    def native(path):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip() if a.windows else str(path.resolve())
    def local(path):return Path(subprocess.check_output(['wslpath','-u',path],text=True).strip()) if a.windows else Path(path)
    def snapshot(root):return {str(x.relative_to(root)):(x.stat().st_size,x.stat().st_mtime_ns,hashlib.sha256(x.read_bytes()).hexdigest()) for x in root.rglob('*') if x.is_file()}
    before=snapshot(originals)
    with tempfile.TemporaryDirectory(prefix='fox ñ ',dir=ROOT/'build') as tmp:
        root=Path(tmp);user=root/'perfil ü'
        def invoke(name,args,code=0,variables=env):
            r=subprocess.run([str(binary/(name+suffix)),*args],env=variables,capture_output=True,text=True,encoding='utf-8',timeout=90)
            if r.returncode!=code:
                for log in user.rglob('stderr.log'):print(log,log.read_text(errors='replace')[-2000:],file=sys.stderr)
            assert r.returncode==code,(r.stdout,r.stderr)
            return loads(r.stdout)
        catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals),'--recursive']);validate(catalog)
        item=next(i for i in catalog['installations'] if i['game_id']=='fox' and i['edition_id']=='dos')
        base=['--user-dir',native(user),'--installation',item['id']]
        report=invoke('titus-session',base+['--inspect-only'],variables=dict(env,SDL_VIDEODRIVER='nonexistent-inspector-must-not-use-SDL'))
        folder=local(report['directory']);req=loads((folder/'request.json').read_text());inspected=loads((folder/'inspection.json').read_text());assert validate_inspection(req,inspected,0)
        assert inspected['status']=='preparation_required' and inspected['preparation_ids']==['fox-lzw-v1'] and inspected['save_index'] is None
        work=local(req['paths']['work_directory']['path']);assert not (work/'prepared').exists() and not (work/'preferences.json').exists()
        failed=invoke('titus-session',base+['--test-ms','150','--test-fault','prepare_fail'],1);assert not failed['ready']
        assert not list((work/'prepared').glob('*.fox-lzw-v1'))
        report=invoke('titus-session',base+['--test-ms','5500','--gameplay-probe']);assert report['state']=='done' and report['ready']
        folder=local(report['directory']);req=loads((folder/'request.json').read_text());events=[loads(x) for x in (folder/'events.jsonl').read_text().splitlines()];result=loads((folder/'result.json').read_text());assert validate_session(req,events,result,0)
        ready=next(e['body'] for e in events if e['event']=='ready');assert set(ready['features'])=={'return_to_library','native_pause','fullscreen'} and ready['save_slot_count']==0
        observation=loads((folder/'probe-observation.json').read_text());assert observation['frames']>=146 and observation['right']>40 and observation['left']>20 and observation['jump']>20 and observation['pause'],observation
        assert observation['samples']>10000 and observation['nonzero']>1000 and observation['peak']>1000 and observation['clipped']==0,observation
        shot=folder/'gameplay.bmp';assert shot.read_bytes()[:2]==b'BM';
        import struct
        assert struct.unpack_from('<ii',shot.read_bytes(),18)==(320,200)
        shutil.copyfile(shot,ROOT/'build'/f'fox-managed-{"windows" if a.windows else "linux"}.bmp')
        prepared=next((work/'prepared').glob('*.fox-lzw-v1'));assert hashlib.sha256((prepared/'music.bin').read_bytes()).hexdigest()==MUSIC
        assert not (prepared/'FOX.COM').exists() and not (prepared/'fox.com').exists()
        cache_before=snapshot(prepared)
        prefs=work/'preferences.json';assert loads(prefs.read_text())==dict(schema_version=1,fullscreen=False)
        # Inspection is read-only and now advertises a valid cache; cached data is reused.
        report=invoke('titus-session',base+['--inspect-only']);folder=local(report['directory']);inspected=loads((folder/'inspection.json').read_text());assert inspected['status']=='ok' and not inspected['preparation_ids'];assert snapshot(prepared)==cache_before
        # Preserve and reject future preferences, then recover without touching originals.
        future='{"schema_version":2,"fullscreen":false}';prefs.write_text(future)
        failed=invoke('titus-session',base+['--test-ms','150'],1);assert not failed['ready'] and prefs.read_text()==future
        prefs.write_text('{"schema_version":1,"fullscreen":false}')
        # Corrupt prepared audio is detected and rebuilt atomically into a new cache.
        (prepared/'music.bin').write_bytes(b'corrupt private cache')
        report=invoke('titus-session',base+['--test-ms','150']);assert report['ready'];assert hashlib.sha256((prepared/'music.bin').read_bytes()).hexdigest()==MUSIC
        assert list((work/'prepared').glob('*.invalid.*'))
        for fault in ['before_ready','missing_result']:
            failed=invoke('titus-session',base+['--test-ms','150','--test-fault',fault],1);assert failed['state']=='failed'
        for folder in (user/'cache/sessions').iterdir():
            req=loads((folder/'request.json').read_text());validate(req)
            if (folder/'inspection.json').exists():assert validate_inspection(req,loads((folder/'inspection.json').read_text()),0)
        if not a.windows:
            decoy=root/'private decoy';decoy.mkdir();(work/'prepared').rename(work/'prepared-backup');(work/'prepared').symlink_to(decoy,target_is_directory=True)
            escaped=invoke('titus-session',base+['--test-ms','150'],1);assert not escaped['ready'] and not list(decoy.iterdir())
            (work/'prepared').unlink();(work/'prepared-backup').rename(work/'prepared')
        profile=work.parent.parent;assert profile.parent.name=='fox' and not (profile/'saves').exists() and not (profile/'migration-receipt.json').exists()
        print(json.dumps(observation),flush=True)
    assert snapshot(originals)==before
    print(f'OK: native LZW/cache repair, pure inspection, gameplay/audio/pause/return and failures; {len(before)} originals unchanged')
if __name__=='__main__':main()
