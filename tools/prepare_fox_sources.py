#!/usr/bin/env python3
"""Export pinned GPL OpenTitus/OPL sources and apply reviewable managed patches.

Build-time only. No data files, bundled OPL archives, downloads or host emulators.
"""
import argparse
from pathlib import Path
import json
import subprocess
from build_fox_reference import export_source,REVISIONS
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--opentitus',type=Path,required=True);p.add_argument('--chocolate',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    if not out.is_relative_to(ROOT/'build'):raise SystemExit('Use an ignored build/ destination')
    source=out/'opentitus';opl=out/'chocolate'
    export_source(a.opentitus.resolve(),source,REVISIONS['opentitus'],['opentitus.c','include','src','opl','gpl3.txt','README'])
    export_source(a.chocolate.resolve(),opl,REVISIONS['chocolate'],['opl','COPYING'])
    for f in (source/'opl').glob('*.a'):f.unlink()
    for patch in ['opentitus-reference.patch','opentitus-managed.patch']:
        subprocess.run(['patch','--batch','-p1','-i',str(ROOT/'patches'/patch)],cwd=source,check=True)
    (opl/'opl/config.h').write_text('/* Software SDL OPL only. */\n')
    f=opl/'opl/opl.c';f.write_text(f.read_text().replace('#ifdef _WIN32\n','#ifdef TITUS_ENABLE_HARDWARE_OPL\n'))
    (out/'revisions.json').write_text(json.dumps(REVISIONS,indent=2)+'\n')
    print(out)
if __name__=='__main__':main()
