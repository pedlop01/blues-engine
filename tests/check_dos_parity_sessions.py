#!/usr/bin/env python3
"""PX-01..08 managed DOS regression: private profiles, eight slots, options and rewind."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from validate_contract import loads,validate,validate_inspection,validate_session

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--datapath',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--game',choices=['fox','prehistorik'],required=True);p.add_argument('--windows',action='store_true');a=p.parse_args()
 binary,originals,out=a.bin_dir.resolve(),a.datapath.resolve(),a.output.resolve();suffix='.exe' if a.windows else ''
 if out.exists() or not out.is_relative_to(ROOT/'build'):raise ValueError('Use a new private build output')
 out.mkdir(parents=True);env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','TITUS_TEST_SCRIPT/p']))
 def native(path):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip() if a.windows else str(path.resolve())
 def local(path):return Path(subprocess.check_output(['wslpath','-u',path],text=True).strip()) if a.windows else Path(path)
 def snapshot(root):return {str(x.relative_to(root)):(x.stat().st_size,x.stat().st_mtime_ns,hashlib.sha256(x.read_bytes()).hexdigest()) for x in root.rglob('*') if x.is_file()}
 before=snapshot(originals);user=out/'perfil ñ';summary=[]
 def invoke(name,args,code=0,variables=None):
  r=subprocess.run([str(binary/(name+suffix)),*args],env=variables or env,capture_output=True,text=True,encoding='utf-8',timeout=90)
  if r.returncode!=code:
   for log in user.rglob('stderr.log'):print(str(log),log.read_text(errors='replace')[-2000:],file=sys.stderr)
   raise ValueError((name,r.returncode,r.stdout[-2000:],r.stderr[-2000:]))
  return loads(r.stdout)
 catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals),'--recursive']);validate(catalog)
 item=next(i for i in catalog['installations'] if i['game_id']==a.game and i['edition_id']=='dos');assert item['status'] in {'recognized','needs_preparation'}
 base=['--user-dir',native(user),'--installation',item['id']]
 inspected=invoke('titus-session',base+['--inspect-only'],variables=dict(env,SDL_VIDEODRIVER='invalid-inspection-must-not-open-video'))
 folder=local(inspected['directory']);request=loads((folder/'request.json').read_text());response=loads((folder/'inspection.json').read_text());assert validate_inspection(request,response,0)
 assert response['status']=='preparation_required' and response['save_slot_count']==8 and response['save_index'] is None
 work=local(request['paths']['work_directory']['path']);assert not(work/'prepared').exists() and not(work/'preferences.json').exists()
 failed=invoke('titus-session',base+['--test-ms','150','--test-fault','prepare_fail'],1);assert not failed['ready']
 played=invoke('titus-session',base+['--test-ms','350']);assert played['ready']
 report=invoke('titus-session',base+['--inspect-only'],variables=dict(env,SDL_VIDEODRIVER='invalid-inspection-must-not-open-video'));assert report['inspection']['status']=='ok' and report['inspection']['save_index']['slots']==[]
 summary.append('read-only inspection, preparation fault and cache recovery')
 script=out/'eight slots.json';steps=[{'ms':200,'key':'Escape'},{'ms':240,'key':'Down'},{'ms':280,'key':'Down'},{'ms':320,'key':'Return'}]
 for i in range(8):
  if i:steps.append({'ms':400+i*180-80,'key':'Right'})
  steps.append({'ms':400+i*180,'key':'Return'})
 script.write_text(json.dumps(steps));variables=dict(env,TITUS_TEST_SCRIPT=str(script))
 played=invoke('titus-session',base+['--test-ms','2200','--gameplay-probe'],variables=variables);assert played['ready'] and played['state']=='done'
 folder=local(played['directory']);request=loads((folder/'request.json').read_text());events=[loads(line) for line in (folder/'events.jsonl').read_text().splitlines()];result=loads((folder/'result.json').read_text());assert validate_session(request,events,result,0)
 required={'save_slots','save_thumbnails','rewind','dynamic_view','smooth_camera','hud_modes','invulnerability','lives_options','level_select','separate_volumes','controller_bindings'}
 assert required<=set(next(e['body']['features'] for e in events if e['event']=='ready'))
 report=invoke('titus-session',base+['--inspect-only']);index=report['inspection']['save_index'];assert len(index['slots'])==8 and all(s['status']=='ready' and s['has_preview'] and s['format_id']==a.game+'.dos.json' for s in index['slots']),index
 folder=local(report['directory']);details=loads((folder/'save-details.json').read_text());assert len(details['slots'])==8 and all(len(s['preview_rgb'])==128*80*6 for s in details['slots'])
 for slot in range(1,9):
  resumed=invoke('titus-session',base+['--load-slot',str(slot),'--test-ms','180']);assert resumed['ready'] and resumed['state']=='done'
 summary.append('eight real slots, thumbnails, metadata and library load for each slot')
 controls=out/'rewind.json';controls.write_text(json.dumps([{'ms':300,'key':'-'},{'ms':500,'key':'Escape'},{'ms':700,'key':'Escape'},{'ms':1500,'rewind':True},{'ms':1900,'rewind':False}]))
 observed=invoke('titus-session',base+['--test-ms','5200','--gameplay-probe'],variables=dict(env,TITUS_TEST_SCRIPT=str(controls)));assert observed['ready'];folder=local(observed['directory']);observation=loads((folder/'probe-observation.json').read_text())
 assert abs(observation['logic_hz']-(18 if a.game=='prehistorik' else 1000/29))<1e-9,observation
 assert observation['right']>20 and observation['left']>20 and observation['jump']>20 and observation['pause'] and observation['rewind_steps']>0,observation
 assert observation['samples']>10000 and observation['nonzero']>1000 and observation['peak']>1000 and observation['clipped']==0,observation
 summary.append({'gameplay_audio_pause_rewind':observation})
 options=out/'options.json';options.write_text(json.dumps({'audio':{'music_volume':30,'effects_volume':60},a.game:{'camera':'classic','hud':'fixed','invulnerable':True,'infinite_lives':True,'lives':8,'start_level':4,'view_step':3},'input':{'action_button':2,'jump_button':3}}))
 overridden=invoke('titus-session',base+['--settings',native(options),'--test-ms','500']);assert overridden['ready'];folder=local(overridden['directory']);request=loads((folder/'request.json').read_text());assert request['settings'][a.game]['start_level']==4 and request['settings']['audio']['music_volume']==30
 prefs=loads((work/'preferences.json').read_text());assert prefs['camera']=='smooth' and prefs['music_volume']==100 and prefs['start_level']==1
 summary.append('edition options, bindings and temporary override recovery')
 # Keyboard bindings and presentation cadence are native preferences, not temporary launcher overrides.
 controls=out/'editable controls.json';steps=[{'ms':100,'key':'Escape'},{'ms':140,'key':'Down'},{'ms':180,'key':'Return'}]+[{'ms':220+i*40,'key':'Down'}for i in range(8)]+[{'ms':580,'key':'Right'},{'ms':640,'key':'Escape'}]+[{'ms':700+i*40,'key':'Down'}for i in range(4)]+[{'ms':900,'key':'Return'},{'ms':1000,'key':'Return'},{'ms':1100,'key':'a'},{'ms':1200,'key':'Escape'},{'ms':1300,'key':'Escape'},{'ms':1500,'audio_reset':True}]
 controls.write_text(json.dumps(steps));changed=invoke('titus-session',base+['--test-ms','2000'],variables=dict(env,TITUS_TEST_SCRIPT=str(controls)));assert changed['ready'];prefs=loads((work/'preferences.json').read_text());assert prefs['render_hz']==120 and prefs['keyboard'][0]==4,prefs
 silent=out/'silent.json';silent.write_text(json.dumps({'audio':{'music_volume':0,'effects_volume':0}}));played=invoke('titus-session',base+['--settings',native(silent),'--test-ms','300']);folder=local(played['directory']);quiet=loads((folder/'probe-observation.json').read_text());assert quiet['nonzero']==0 and quiet['peak']==0,quiet
 summary.append('editable keyboard, 120 FPS persistence, simulated device reopen and silent mix')
 future='{"schema_version":2,"fullscreen":false}';old=(work/'preferences.json').read_text();(work/'preferences.json').write_text(future)
 failed=invoke('titus-session',base+['--test-ms','150'],1);assert not failed['ready'] and (work/'preferences.json').read_text()==future;(work/'preferences.json').write_text(old)
 for fault in ['before_ready','missing_result']:
  failed=invoke('titus-session',base+['--test-ms','150','--test-fault',fault],1);assert failed['state']=='failed'
 healthy=invoke('titus-session',base+['--test-ms','200']);assert healthy['ready'];summary.append('future preferences, load failure, missing result and subsequent recovery')
 slot=work/'slot-8.json';raw=slot.read_bytes();slot.write_bytes(raw[:len(raw)//2]);seventh=work/'slot-7.json';other=json.loads(seventh.read_text());other['fingerprint']='0'*64;seventh.write_text(json.dumps(other))
 report=invoke('titus-session',base+['--inspect-only']);statuses={s['slot']:s['status'] for s in report['inspection']['save_index']['slots']};assert statuses[8]=='damaged' and statuses[7]=='incompatible' and all(statuses[n]=='ready' for n in range(1,7));assert slot.read_bytes()==raw[:len(raw)//2]
 summary.append('damaged and incompatible saves preserved and isolated')
 assert snapshot(originals)==before
 result=dict(schema_version=1,game=a.game,platform='windows' if a.windows else 'linux',checks=summary,originals_unchanged=True,original_files=len(before));(out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
