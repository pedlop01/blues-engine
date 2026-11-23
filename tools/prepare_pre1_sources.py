#!/usr/bin/env python3
"""Export pinned Olduvai GPL sources and apply isolated managed source patches."""
import argparse
from pathlib import Path
import json
import subprocess
from build_fox_reference import export_source
ROOT=Path(__file__).resolve().parents[1]
REV='a69e9204fc311c8810d696a6b799000d1bcd1642'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--olduvai',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
    if not out.is_relative_to(ROOT/'build'):raise SystemExit('Use an ignored build/ destination')
    export_source(a.olduvai.resolve(),out,REV,['CMakeLists.txt','src','third_party','cmake','assets','packaging','tests','tools','LICENSE','LEGAL.md','THIRD-PARTY-NOTICES.md','README.md','docs','scripts'])
    for patch in ['olduvai-mingw-loader.patch','olduvai-managed.patch']:
        subprocess.run(['patch','--batch','-p1','-i',str(ROOT/'patches'/patch)],cwd=out,check=True)
    f=out/'CMakeLists.txt';s=f.read_text().replace('${CMAKE_SOURCE_DIR}','${CMAKE_CURRENT_SOURCE_DIR}').replace('${CMAKE_BINARY_DIR}','${CMAKE_CURRENT_BINARY_DIR}');(out/'CMakeLists.upstream.txt').write_text(s)
    prefix=s[:s.index('# ── app ─')]
    begin=s.index('add_custom_command(\n    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/menu_model_data.cpp')
    end=s.index('# olduvai_tests compiles menu_model.cpp',begin)
    # Keep the pinned library recipes and generated menu/icon, without CLI/tests/install policy.
    f.write_text(prefix+'\n# Titus managed library-only embedding.\n'+s[begin:end])
    (out/'titus-source.json').write_text(json.dumps(dict(engine='olduvai',revision=REV,embedding_paths_adjusted=True),indent=2)+'\n')
    print(out)
if __name__=='__main__':main()
