#!/usr/bin/env python3
"""Repeatable TH-12 automatic validation; manual campaign/hardware gates stay explicit."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
ROOT=Path(__file__).resolve().parents[1]

def snapshot(root):
 return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--orig-games',type=Path,required=True);p.add_argument('--platform',choices=['linux','windows'],required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--import-fixtures',type=Path);p.add_argument('--resume',action='store_true');a=p.parse_args()
 output=a.output.resolve();assert output.is_relative_to(ROOT/'build'),'Use an ignored build/ output';output.mkdir(parents=True,exist_ok=True)
 binary=a.bin_dir.resolve();originals=a.orig_games.resolve();before=snapshot(originals);windows=a.platform=='windows';suffix='.exe' if windows else '';platform_args=['--windows'] if windows else [];fixtures=output/'fixtures'
 executables=['titus-catalog','titus-engine','titus-session','engines/blues-family/blues-family-runtime','engines/opentitus/opentitus-runtime','engines/olduvai/olduvai-runtime']
 def binary_hashes():return {name+suffix:hashlib.sha256((binary/(name+suffix)).read_bytes()).hexdigest() for name in executables}
 initial_binaries=binary_hashes()
 report={'schema_version':1,'task':'TH-12','platform':a.platform,'binaries_sha256':initial_binaries,'source_worktree_modified':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True)),'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'video_backend':'dummy','audio_backend':'dummy','automatic_status':'running','manual_status':'pending','original_files':len(before),'cases':[]}
 file=output/'report.json'
 previous={}
 if a.resume:
  saved=json.loads(file.read_text());assert saved['platform']==a.platform and saved['source_revision']==report['source_revision'],'Resume requires the same platform/base revision'
  if 'binaries_sha256' in saved:assert saved['binaries_sha256']==initial_binaries,'Executables changed; run a fresh validation'
  else:
   # Compatibility with the initial TH-12 report: bind its already-passed prefix
   # only when every executable predates the first case. Record the distinction.
   first=saved['cases'][0];first_start=(output/(first['id']+'.log')).stat().st_mtime-first['seconds']
   assert all((binary/(name+suffix)).stat().st_mtime<=first_start for name in executables),'Cannot bind the earlier results to these executables'
   report['binary_fingerprint_recorded_on_resume']=True
  previous={case['id']:case for case in saved['cases'] if case['status']=='passed'}
 def write():file.write_text(json.dumps(report,indent=2)+'\n')
 def test(name,command,timeout=600):
  log=output/(name+'.log')
  if name in previous:
   entry=dict(previous[name]);assert log.exists() and hashlib.sha256(log.read_bytes()).hexdigest()==entry['log_sha256'],'Previous evidence changed'
   entry['reused_verified_result']=True;report['cases'].append(entry);write();print(a.platform,name,'REUSED VERIFIED RESULT',flush=True);return
  start=time.monotonic();print(a.platform,name,'START',flush=True)
  with log.open('w') as stream:
   proc=subprocess.Popen([str(x) for x in command],cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT)
   while True:
    try:code=proc.wait(timeout=30);break
    except subprocess.TimeoutExpired:
     print(a.platform,name,'RUNNING',round(time.monotonic()-start),'s',flush=True)
     if time.monotonic()-start>timeout:
      proc.kill();code=proc.wait();break
  entry={'id':name,'status':'passed' if code==0 else 'failed','exit_code':code,'seconds':round(time.monotonic()-start,2),'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()};report['cases'].append(entry);write()
  print(a.platform,name,entry['status'],entry['seconds'],'s',flush=True)
  if code:print(log.read_text(errors='replace')[-6000:],flush=True);raise RuntimeError(name+' failed')
 def script(name,args=()):return [sys.executable,ROOT/'tests'/name,'--bin-dir',binary,'--orig-games',originals,*platform_args,*args]
 try:
  write();test('cpp-contracts-settings-process',['ctest','--test-dir',binary,'--output-on-failure'])
  test('catalog-cli',[sys.executable,ROOT/'tests/check_catalog_cli.py','--binary',binary/('titus-catalog'+suffix),*platform_args])
  if not windows:test('json-contracts',[sys.executable,ROOT/'tests/check_engine_contracts.py'])
  options=['--export-fixtures',fixtures]
  if a.import_fixtures:options+=['--import-fixtures',a.import_fixtures.resolve()]
  test('blues-migration-slots-camera-rewind',script('check_blues_sessions.py',options))
  test('unified-settings-native-resume',script('check_unified_sessions.py',['--fixtures',fixtures]))
  test('family-keyboard-controller-pcm',script('check_family_sessions.py'))
  test('fox-preparation-gameplay-recovery',script('check_fox_sessions.py'))
  test('prehistorik-preparation-gameplay-recovery',script('check_pre1_sessions.py'))
  test('six-game-restarts-isolation-recovery',script('check_expansion_sessions.py'),timeout=900)
  test('library-management-keyboard-controller',[sys.executable,ROOT/'tests/check_launcher.py','--binary',binary/('titus-engine'+suffix),'--orig-games',originals,'--headless',*platform_args])
  test('library-six-play-returns',script('check_launcher_sessions.py',['--pre1','--headless']))
  report['binaries_unchanged']=binary_hashes()==initial_binaries
  if not report['binaries_unchanged']:raise RuntimeError('Executables changed during validation')
  report['originals_unchanged']=snapshot(originals)==before
  if not report['originals_unchanged']:raise RuntimeError('Original resources changed')
  report['automatic_status']='passed';write();print(a.platform,'AUTOMATIC MATRIX PASSED; campaigns/listening/physical controllers pending',flush=True)
 except Exception as error:
  report['automatic_status']='failed';report['error']=str(error);report['originals_unchanged']=snapshot(originals)==before;write();raise
if __name__=='__main__':main()
