#!/usr/bin/env python3
"""TH-12 classic Blues visual/state regressions, alongside the managed binaries."""
import argparse,hashlib,json,os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def snapshot(root):return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--orig-games',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);a=parser.parse_args();originals=a.orig_games.resolve();before=snapshot(originals);out=a.output.resolve();assert out.is_relative_to(ROOT/'build');out.mkdir(parents=True,exist_ok=True)
 env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',BLUES_CONFIG_FILE='',BLUES_SAVE_DIR='');results=[]
 def call(name,command,variables=env):
  print(name,'START',flush=True)
  with (out/(name+'.log')).open('w') as log:r=subprocess.run([str(x) for x in command],cwd=ROOT,env=variables,stdout=log,stderr=subprocess.STDOUT,timeout=600)
  assert r.returncode==0,(name,(out/(name+'.log')).read_text(errors='replace')[-4000:]);results.append(name);print(name,'OK',flush=True)
 targets=['check_viewport','check_intermission','check_sprite_interpolation','check_pause_menu','check_preferences','check_mixer_volumes']
 call('build',['make','-j2',*['build/tests/'+x for x in targets]])
 for name in targets[2:]:call(name,[ROOT/'build/tests'/name])
 for mode in ['dos','amiga']:
  source=originals/('blues-'+mode)
  for width in ['', 'wide']:
   call('intermission-'+mode+'-'+(width or 'normal'),[ROOT/'build/tests/check_intermission',source,*([width] if width else [])])
  for camera in ['classic','smooth']:
   for hud in ['classic','fixed']:
    args=[ROOT/'build/tests/check_viewport','--datapath='+str(source),'--dos-datapath='+str(originals/'blues-dos'),'--amiga-datapath='+str(originals/'blues-amiga'),'--camera='+camera,'--hud='+hud,'--scrollrate=60']
    call('zoom-'+mode+'-'+camera+'-'+hud,args)
  args=[ROOT/'build/tests/check_viewport','--datapath='+str(source),'--dos-datapath='+str(originals/'blues-dos'),'--amiga-datapath='+str(originals/'blues-amiga'),'--scrollrate=60']
  call('camera-'+mode,args,dict(env,BLUES_TEST_CAMERA='1'))
 for name in ['check_scroll_launches.py','check_amiga_layers.py','check_state_launches.py','check_rewind_launches.py']:call(name,[sys.executable,ROOT/'tests'/name,'--orig-games',originals])
 assert snapshot(originals)==before
 (out/'report.json').write_text(json.dumps({'schema_version':1,'platform':'linux','cases_passed':results,'original_files_unchanged':len(before),'status':'passed'},indent=2)+'\n');print('Classic Blues baseline passed; originals unchanged',flush=True)
if __name__=='__main__':main()
