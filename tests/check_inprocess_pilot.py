#!/usr/bin/env python3
"""TH-11 actual Olduvai host/child experiment, using private audited resources."""
import argparse,hashlib,json,os,statistics,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def snapshot(root):
 return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
def main():
 p=argparse.ArgumentParser();p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--windows',action='store_true');p.add_argument('--report',type=Path,required=True);a=p.parse_args()
 binary=a.bin_dir.resolve();originals=a.orig_games.resolve();before=snapshot(originals);suffix='.exe' if a.windows else '';env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','TITUS_PILOT_SCENARIO']))
 def native(p):return subprocess.check_output(['wslpath','-w',str(p.resolve())],text=True).strip() if a.windows else str(p.resolve())
 def local(p):return Path(subprocess.check_output(['wslpath','-u',p],text=True).strip()) if a.windows else Path(p)
 with tempfile.TemporaryDirectory(prefix='TH11 piloto ñ ',dir=ROOT/'build') as temp:
  root=Path(temp);user=root/'perfil á'
  def invoke(tool,args):
   r=subprocess.run([str(binary/(tool+suffix)),*args],env=env,capture_output=True,text=True,encoding='utf-8',timeout=100);assert r.returncode==0,(r.stdout,r.stderr);return json.loads(r.stdout)
  catalog=invoke('titus-catalog',['--user-dir',native(user),'add',native(originals),'--recursive']);item=next(i for i in catalog['installations'] if i['game_id']=='prehistorik' and i['edition_id']=='dos')
  warm=invoke('titus-session',['--user-dir',native(user),'--installation',item['id'],'--test-ms','150']);assert warm['ready'];request=local(warm['directory'])/'request.json'
  req=json.loads(request.read_text());work=local(req['paths']['work_directory']['path']);prepared=work/'prepared';cache=snapshot(prepared)
  reports={}
  for mode in ['child','inline']:
   for scenario in ['sequence','audio','crash']:
    output=root/f'{mode}-{scenario}.json';variables=dict(env,TITUS_PILOT_SCENARIO=scenario)
    r=subprocess.run([str(binary/('titus-inprocess-pilot'+suffix)),'--mode',mode,'--request',native(request),'--output',native(output)],env=variables,capture_output=True,text=True,encoding='utf-8',timeout=210)
    (ROOT/'build'/f'th11-{mode}-{scenario}-{"windows" if a.windows else "linux"}.log').write_text(r.stdout+'\nSTDERR\n'+r.stderr)
    assert output.exists(),(mode,scenario,r.returncode,r.stderr)
    report=json.loads(output.read_text());report['host_exit_code']=r.returncode;reports[f'{mode}_{scenario}']=report
    if scenario=='crash' and mode=='inline':
     assert r.returncode==86 and not report['complete'] and report['host_before']['drawable'],report
    else:
     assert r.returncode==0 and report['complete'],(mode,scenario,r.returncode,report,r.stderr)
     for run in report['runs']:
      assert run['host_recovered'],run
      after=run['host_after']
      if mode=='child':assert not run['host_rebuilt'] and after['drawable'] and after['video'] and after['audio_subsystem'] and after['controller_subsystem'] and after['host_event_preserved'],run
      if mode=='child' and scenario=='audio':assert after['host_audio_playing'] and after['host_audio_callbacks']>0,run
      if run['fault']=='normal' and scenario=='sequence':
       assert run['exit_code']==0 and run['result']['ready'],run
       o=run['observation'];assert o['frames']>=146 and o['right']>40 and o['left']>20 and o['jump']>20 and o['pause'],o
       assert o['samples']>10000 and o['nonzero']>1000 and o['clipped']==0,o
       if mode=='inline':assert not after['drawable'] and not after['video'] and run['host_rebuilt'],run
      if run['fault']=='before_ready':assert run['exit_code']==1 and not run['result']['ready'] and after['drawable'],run
      if run['fault']=='pilot_throw':
       assert run['exit_code']==1 and run['result']['ready'],run
       if mode=='inline':assert run['native']['engine_window_leaked'],run
      if run['fault']=='pilot_crash':assert run['exit_code']==86 and run['result'] is None and after['drawable'],run
    print(mode,scenario,'OK',flush=True)
  summary={}
  for mode in ['child','inline']:
   normal=[x for x in reports[mode+'_sequence']['runs'] if x['fault']=='normal']
   summary[mode]={'samples':len(normal),'median_ready_ms':round(statistics.median(x['ready_ms'] for x in normal),2),'median_invocation_ms':round(statistics.median(x['invocation_ms'] for x in normal),2),'median_engine_cpu_ms':round(statistics.median(x['native']['cpu_ms'] for x in normal),2),'host_peak_rss_bytes':reports[mode+'_sequence']['host_peak_rss_bytes'],'max_engine_peak_rss_bytes':max(x['native']['peak_rss_bytes'] for x in normal),'host_rebuilds':sum(x['host_rebuilt'] for x in reports[mode+'_sequence']['runs'])}
  assert snapshot(prepared)==cache,'Pilot must not rewrite prepared original data'
  assert snapshot(originals)==before,'Original game files changed'
  result={'schema_version':1,'platform':'windows' if a.windows else 'linux','backend':{'video':'dummy','audio':'dummy'},'engine':'olduvai','engine_revision':'a69e9204fc311c8810d696a6b799000d1bcd1642','source_files_unchanged':len(before),'summary':summary,'experiments':reports}
  a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(summary),flush=True)
if __name__=='__main__':main()
