#!/usr/bin/env python3
"""Persist/reopen canonical native saves in separate processes and reject damaged files."""
import argparse,json,os,subprocess,tempfile,zlib,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--windows',action='store_true');p.add_argument('--sample',action='store_true');p.add_argument('--peer-bin-dir',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
reports=[]
with tempfile.TemporaryDirectory(prefix='family saves ñ ',dir=ROOT/'build') as tmp:
 storage=Path(tmp)
 for module,folder,stages in [('ja','blues2-dos',[1,16,31] if a.sample else range(1,32)),('p2','prehistorik2-dos',[0,5,9,15] if a.sample else range(16))]:
  for stage in stages:
   root=storage/f'{module}-{stage}';root.mkdir();env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
   def native(path,windows=a.windows):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip() if windows else str(path.resolve())
   def run(mode,peer=False):
    windows=(not a.windows) if peer else a.windows
    binaries=a.peer_bin_dir if peer else a.bin_dir
    call_env=dict(env,PF_SAVE_TEST=mode,BLUES_SAVE_DIR=native(root,windows))
    if windows:call_env['WSLENV']=':'.join(filter(None,call_env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER','BLUES_SAVE_DIR','PF_SAVE_TEST']))
    r=subprocess.run([str((binaries/f'check_{module}_presentation{".exe" if windows else ""}').resolve()),native(a.orig_games/folder,windows),str(stage)],env=call_env,capture_output=True,text=True,encoding='utf-8',timeout=60)
    assert r.returncode==0,(module,stage,mode,peer,r.stdout,r.stderr)
   run('write')
   if a.peer_bin_dir:run('read',True);run('write',True)
   run('read');original=(root/'slot01.bbs').read_bytes()
   for kind in ['truncated','crc','future','other_game','resources','payload']:
    data=bytearray(original)
    if kind=='truncated':data=data[:100]
    elif kind=='crc':data[-1]^=1
    elif kind=='future':struct.pack_into('<I',data,8,99)
    elif kind=='other_game':struct.pack_into('<I',data,12,2 if module=='ja' else 1)
    elif kind=='resources':data[24]^=1;struct.pack_into('<I',data,20,zlib.crc32(data[24:]))
    else:
     if module=='p2':continue
     # First field after the palette is JA's RNG initialization boolean.
     data[80+768]=255
     struct.pack_into('<I',data,20,zlib.crc32(data[24:]))
    (root/'slot01.bbs').write_bytes(data);run('reject')
   reports.append({'module':module,'stage':stage,'result':'pass'});print(module,stage,'PASS',flush=True)
(a.output/'report.json').write_text(json.dumps({'platform':'windows' if a.windows else 'linux','stages':reports,'cross_platform':bool(a.peer_bin_dir),'rejections':['truncation','crc','future_version','other_game','resources','invalid_boolean_JA']},indent=2)+'\n')
