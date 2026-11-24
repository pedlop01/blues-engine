#!/usr/bin/env python3
"""Managed Prehistorik Amiga sessions, options, native slots and failure recovery.

Private profiles only; original resources must remain byte-for-byte unchanged.
"""
import argparse,hashlib,json,os,shutil,subprocess,sys,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from validate_contract import loads,validate,validate_inspection,validate_session

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--datapath',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--windows',action='store_true');a=p.parse_args()
    binary,originals,out=a.bin_dir.resolve(),a.datapath.resolve(),a.output.resolve();suffix='.exe' if a.windows else ''
    if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build output')
    out.mkdir(parents=True)
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','TITUS_TEST_SCRIPT/p']))
    def native(path):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip() if a.windows else str(path.resolve())
    def local(path):return Path(subprocess.check_output(['wslpath','-u',path],text=True).strip()) if a.windows else Path(path)
    def snapshot(root):return {str(x.relative_to(root)):(x.stat().st_size,x.stat().st_mtime_ns,hashlib.sha256(x.read_bytes()).hexdigest()) for x in root.rglob('*') if x.is_file()}
    before=snapshot(originals);user=out/'perfil ñ';summary=[]
    def invoke(name,args,code=0,variables=None):
        r=subprocess.run([str(binary/(name+suffix)),*args],env=variables or env,capture_output=True,text=True,encoding='utf-8',timeout=90)
        if r.returncode!=code:
            for log in user.rglob('stderr.log'):print(str(log),log.read_text(errors='replace')[-2000:],file=sys.stderr)
        if r.returncode!=code:raise ValueError((name,r.returncode,r.stdout[-2000:],r.stderr[-2000:]))
        return loads(r.stdout)
    catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals)]);validate(catalog)
    item=next(i for i in catalog['installations'] if i['game_id']=='prehistorik' and i['edition_id']=='amiga');assert item['engine_id']=='pre1-amiga-native' and item['status']=='recognized'
    base=['--user-dir',native(user),'--installation',item['id']]
    inspected=invoke('titus-session',base+['--inspect-only'],variables=dict(env,SDL_VIDEODRIVER='invalid-inspection-must-not-open-video'))
    folder=local(inspected['directory']);request=loads((folder/'request.json').read_text());response=loads((folder/'inspection.json').read_text());assert validate_inspection(request,response,0)
    assert response['status']=='ok' and response['save_slot_count']==8 and not response['save_index']['slots'] and not response['preparation_ids']
    work=local(request['paths']['work_directory']['path']);assert not (work/'preferences.json').exists()
    script=out/'eight slots.json';steps=[{'ms':200,'key':'Escape'},{'ms':220,'key':'Down'},{'ms':240,'key':'Down'},{'ms':260,'key':'Return'}]
    for i in range(8):
        if i:steps.append({'ms':300+i*100-20,'key':'Right'})
        steps.append({'ms':300+i*100,'key':'Return'})
    script.write_text(json.dumps(steps));variables=dict(env,TITUS_TEST_SCRIPT=str(script))
    played=invoke('titus-session',base+['--test-ms','1800','--gameplay-probe'],variables=variables);assert played['ready'] and played['state']=='done'
    folder=local(played['directory']);request=loads((folder/'request.json').read_text());events=[loads(line) for line in (folder/'events.jsonl').read_text().splitlines()];result=loads((folder/'result.json').read_text());assert validate_session(request,events,result,0)
    required={'save_slots','save_thumbnails','rewind','dynamic_view','smooth_camera','hud_modes','invulnerability','lives_options','level_select','separate_volumes','controller_bindings'}
    assert required<=set(next(e['body']['features'] for e in events if e['event']=='ready'))
    report=invoke('titus-session',base+['--inspect-only']);folder=local(report['directory']);index=report['inspection']['save_index'];assert len(index['slots'])==8 and all(s['status']=='ready' and s['has_preview'] for s in index['slots']);validate(index)
    details=loads((folder/'save-details.json').read_text());assert len(details['slots'])==8 and all(len(s['preview_rgb'])==128*80*6 for s in details['slots'])
    summary.append('eight native slots, payload/preview hashes and library thumbnails')
    loaded=invoke('titus-session',base+['--load-slot','1','--test-ms','500']);assert loaded['ready'] and loaded['state']=='done'
    returned=invoke('titus-session',base+['--return-on-ready','--test-ms','5000']);assert returned['ready'] and returned['state']=='done';summary.append('library load and return command')
    controls=out/'controls.json';controls.write_text(json.dumps([{'ms':1100,'key':'P'},{'ms':1500,'key':'P'},{'ms':3000,'rewind':True},{'ms':3400,'rewind':False}]))
    observed=invoke('titus-session',base+['--test-ms','5200','--gameplay-probe'],variables=dict(env,TITUS_TEST_SCRIPT=str(controls)));assert observed['ready'];folder=local(observed['directory']);observation=loads((folder/'probe-observation.json').read_text())
    assert observation['right']>20 and observation['left']>20 and observation['jump']>20 and observation['pause'] and observation['rewind_steps']>0,observation
    assert observation['samples']>10000 and observation['nonzero']>1000 and observation['peak']>1000 and observation['clipped']==0,observation
    summary.append({'gameplay_audio_pause_rewind':observation})
    options=out/'options.json';options.write_text(json.dumps({'audio':{'music_volume':30,'effects_volume':60},'prehistorik':{'camera':'classic','hud':'fixed','invulnerable':True,'infinite_lives':True,'lives':8,'start_level':4,'view_step':3},'input':{'action_button':2,'jump_button':3}}))
    overridden=invoke('titus-session',base+['--settings',native(options),'--test-ms','500']);assert overridden['ready'];folder=local(overridden['directory']);request=loads((folder/'request.json').read_text());assert request['settings']['prehistorik']['start_level']==4 and request['settings']['audio']['music_volume']==30
    # A subsequent session recovers temporary overrides; changing native options
    # in the menu remains persistent via the shared settings store.
    prefs=loads((work/'preferences.json').read_text());assert prefs['camera']=='smooth' and prefs['music_volume']==100 and prefs['start_level']==1
    summary.append('edition options, bindings and temporary override recovery')
    future='{"schema_version":2,"fullscreen":false}';old=(work/'preferences.json').read_text();(work/'preferences.json').write_text(future)
    failed=invoke('titus-session',base+['--test-ms','150'],1);assert not failed['ready'] and (work/'preferences.json').read_text()==future;(work/'preferences.json').write_text(old)
    for fault in ['before_ready','missing_result']:
        failed=invoke('titus-session',base+['--test-ms','150','--test-fault',fault],1);assert failed['state']=='failed'
    healthy=invoke('titus-session',base+['--test-ms','200']);assert healthy['ready'];summary.append('future preferences, load failure, missing result and subsequent recovery')
    slot=work/'slot-8.json';raw=slot.read_bytes();slot.write_bytes(raw[:len(raw)//2]);seventh=work/'slot-7.json';other=json.loads(seventh.read_text());other['fingerprint']='0'*64;seventh.write_text(json.dumps(other))
    report=invoke('titus-session',base+['--inspect-only']);statuses={s['slot']:s['status'] for s in report['inspection']['save_index']['slots']};assert statuses[8]=='damaged' and statuses[7]=='incompatible' and all(statuses[n]=='ready' for n in range(1,7));assert slot.read_bytes()==raw[:len(raw)//2]
    summary.append('damaged and incompatible saves are preserved and isolated')
    assert snapshot(originals)==before
    result=dict(schema_version=1,platform='windows' if a.windows else 'linux',checks=summary,originals_unchanged=True,original_files=len(before));(out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
