#!/usr/bin/env python3
"""Assert exact simulation equality at 30/60/120 presentation FPS for both DOS games."""
import argparse,concurrent.futures,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir',type=Path,required=True);p.add_argument('--fox-data',type=Path,required=True);p.add_argument('--pre1-data',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--windows',action='store_true');a=p.parse_args()
 out=a.output.resolve()
 if out.exists()or not out.is_relative_to(ROOT/'build'):raise ValueError('Use new private build output')
 out.mkdir(parents=True);env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
 if a.windows:env['WSLENV']=':'.join(filter(None,env.get('WSLENV','').split(':')+['SDL_VIDEODRIVER','SDL_AUDIODRIVER']))
 def native(path):return subprocess.check_output(['wslpath','-w',str(path.resolve())],text=True).strip()if a.windows else str(path.resolve())
 suffix='.exe'if a.windows else '';programs={'fox':a.bin_dir/'engines/opentitus'/('opentitus-runtime'+suffix),'prehistorik':a.bin_dir/'engines/olduvai'/('olduvai-runtime'+suffix)};data={'fox':a.fox_data,'prehistorik':a.pre1_data}
 def run(job):
  game,hz=job;work=out/f'{game}-{hz}';work.mkdir()
  r=subprocess.run([str(programs[game].resolve()),'--datapath',native(data[game]),'--workdir',native(work),'--render-hz',str(hz),'--probe-ticks','180'],env=env,capture_output=True,text=True,timeout=60)
  if r.returncode:raise ValueError((job,r.returncode,r.stderr))
  d=json.loads(r.stdout);assert d['ticks']==180 and d['render_hz']==hz,d;return dict(game=game,presentation_fps=hz,observation=d)
 with concurrent.futures.ThreadPoolExecutor(max_workers=3)as pool:results=list(pool.map(run,[(g,hz)for g in programs for hz in [30,60,120]]))
 for game in programs:assert len({d['observation']['state_sha256']for d in results if d['game']==game})==1,results
 result=dict(schema_version=1,platform='windows'if a.windows else'linux',simulation_independent_of_render_fps=True,cases=results);(out/'report.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
