#!/usr/bin/env python3
"""Replay actor records and combined simulation/render across host builds."""
import argparse,hashlib,json,subprocess,sys
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('probe','datapath','enemies','platforms','game','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--windows',action='store_true');args=p.parse_args();root=args.datapath.resolve();out=args.output.resolve()
    if out.exists() or out==root or root in out.parents:raise ValueError('Use new output outside originals')
    def snapshot():return {str(p.relative_to(root)):(p.stat().st_size,p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}
    before=snapshot();out.mkdir(parents=True);cases=[]
    for level in list(range(1,15))+[16]:
        for mode,ticks,reference,filename in [('enemy',600,args.enemies/f'enemy-motion-{level}','enemy-motion.json'),('platform',600,args.platforms/f'level-{level}','platform-motion.json'),('game',1200,args.game/str(level),'game-motion.json')]:
            folder=out/f'{mode}-{level}';options=['--datapath',str(root),'--output',str(folder),'--level',str(level),'--'+mode+'-ticks',str(ticks)]
            if args.windows:
                result=subprocess.run([sys.executable,str(Path(__file__).resolve().parents[1]/'tools/run_windows_probe.py'),'--program',str(args.probe.resolve()),'--output-dir',str(out/'runner'),'--timeout','60','--',*options],capture_output=True,text=True,timeout=75)
                if not result.stdout:raise ValueError(result.stderr)
                wrapped=json.loads(result.stdout);result=subprocess.CompletedProcess(result.args,wrapped['exit'],wrapped['stdout'],wrapped['stderr'])
            else:result=subprocess.run([str(args.probe.resolve()),*options],capture_output=True,text=True,timeout=60)
            if result.returncode:raise ValueError((mode,level,result.stderr))
            assert json.loads((folder/filename).read_text())==json.loads((reference/filename).read_text()),(mode,level)
            cases.append(dict(mode=mode,level=level,ticks=ticks,matched=True))
        print(f'Actors and game level {level} matched',flush=True)
    assert before==snapshot()
    (out/'report.json').write_text(json.dumps(dict(schema_version=1,playable=False,cases=cases,originals_unchanged=True),indent=2)+'\n')
if __name__=='__main__':main()
