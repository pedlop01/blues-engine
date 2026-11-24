#!/usr/bin/env python3
"""Build current Linux/Windows packages from scratch; build/ contains deliveries only.

SDKs reside in $XDG_CACHE_HOME/titus-engine/deps. Compiler output and logs go
outside the repository; successful temporary compilations are removed.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CACHE = Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache')) / 'titus-engine'
STATE = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'titus-engine'


def build(platform, args):
    destination = ROOT / 'build' / platform
    if destination.exists() and not args.replace:
        raise SystemExit(f'{destination} exists; use --replace to rebuild this delivery.')
    if destination.is_symlink():
        raise SystemExit(f'Refusing to replace a symlink: {destination}')
    work_root = CACHE / 'work'
    work_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix=platform + '-', dir=work_root))
    staging = work / 'package'
    log_dir = STATE / 'build-logs'
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / (platform + '.log')
    deps = args.deps_dir.resolve()
    options = [
        '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON',
        '-DTITUS_BUILD_LAUNCHER=ON', '-DTITUS_BUILD_BLUES_RUNTIME=ON',
        '-DTITUS_BUILD_FOX_RUNTIME=ON', '-DTITUS_BUILD_PRE1_RUNTIME=ON',
        '-DTITUS_BUILD_FOX_AMIGA_RUNTIME=ON', '-DTITUS_BUILD_PRE1_AMIGA_RUNTIME=ON',
        '-DTITUS_BUILD_DOS_PARITY=ON', '-DTITUS_BUILD_DOS_PROBES=OFF',
        '-DTITUS_BUILD_FOX_AMIGA_PROBE=OFF', '-DTITUS_BUILD_PRE1_AMIGA_PROBE=OFF',
        '-DTITUS_BUILD_INPROCESS_PILOT=OFF',
    ]
    if platform == 'windows':
        sdl = deps / 'SDL2-2.30.12/x86_64-w64-mingw32'
        ttf = deps / 'SDL2_ttf-2.22.0/x86_64-w64-mingw32'
        required = [sdl / 'lib/cmake/SDL2/sdl2-config.cmake',
                    ttf / 'include/SDL2/SDL_ttf.h', ttf / 'bin/SDL2_ttf.dll',
                    deps / 'libmodplug-windows/lib/libmodplug.a']
        if any(not p.is_file() for p in required):
            shutil.rmtree(work)
            raise SystemExit(f'Missing cached Windows SDK/dependencies in {deps}')
        options += [
            '-DCMAKE_SYSTEM_NAME=Windows',
            '-DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc-posix',
            '-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++-posix',
            f'-DSDL2_DIR={sdl / "lib/cmake/SDL2"}',
            f'-DTITUS_TTF_INCLUDE={ttf / "include/SDL2"}',
            f'-DTITUS_TTF_LIBRARY={ttf / "lib/libSDL2_ttf.dll.a"}',
            f'-DTITUS_TTF_DLL={ttf / "bin/SDL2_ttf.dll"}',
            f'-DTITUS_MODPLUG_ROOT={deps / "libmodplug-windows"}',
        ]
    elif (deps / 'include/SDL2/SDL_ttf.h').exists():
        options += [f'-DTITUS_TTF_INCLUDE={deps / "include/SDL2"}']
    succeeded = False
    try:
        with log_path.open('w') as log:
            def run(command):
                print(f'{platform}: {command[0]} {command[1]}', flush=True)
                log.write('\n$ ' + ' '.join(map(str, command)) + '\n')
                log.flush()
                subprocess.run(list(map(str, command)), cwd=ROOT,
                               stdout=log, stderr=subprocess.STDOUT, check=True)
            run(['cmake', '-S', ROOT, '-B', work / 'compile', *options])
            run(['cmake', '--build', work / 'compile', '--parallel', args.jobs])
            if platform == 'windows':
                # The contract vector argument must be a native Windows path under WSL.
                vector = subprocess.check_output(['wslpath', '-w', str(ROOT / 'contracts/fingerprint-v1-vector.json')], text=True).strip()
                run([work / 'compile/check_catalog.exe', vector])
                run(['ctest', '--test-dir', work / 'compile', '--output-on-failure', '-E', '^catalog$'])
            else:
                run(['ctest', '--test-dir', work / 'compile', '--output-on-failure'])
            run(['cmake', '--install', work / 'compile', '--prefix', staging, '--strip'])
            if platform == 'windows':
                for dll in staging.rglob('*.dll'):
                    run(['x86_64-w64-mingw32-strip', '--strip-unneeded', dll])
            images = deps / 'artwork'
            if images.exists():
                for image in images.glob('*.bmp'):
                    shutil.copy2(image, staging / 'bin/assets/covers' / image.name)
            license = deps / 'libmodplug-windows/COPYING'
            if license.exists():
                target = staging / 'share/licenses/titus-engine/libmodplug-PUBLIC-DOMAIN.txt'
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(license, target)
            shutil.copy2(ROOT / 'CREDITS.md', staging / 'CREDITS.md')
            (staging / 'README.txt').write_text(
                'Titus Engine\n\nAbrir bin/titus-engine' + ('.exe' if platform == 'windows' else '') +
                '. Mantener sus carpetas y dependencias.\n'
                'Incluye los cinco motores y las opciones actuales.\n'
                'Los perfiles y guardados se conservan fuera de esta carpeta.\n', encoding='utf-8')
            registry = json.loads((staging / 'bin/engines/registry.json').read_text())
            assert len(registry['engines']) == 5
            executable = staging / 'bin' / ('titus-engine.exe' if platform == 'windows' else 'titus-engine')
            assert executable.is_file()
            (ROOT / 'build').mkdir(exist_ok=True)
            if destination.exists():
                shutil.rmtree(destination)
            # Work/cache may live on another filesystem.
            shutil.move(str(staging), destination)
        succeeded = True
        print(f'{platform}: ready at {destination}; log: {log_path}', flush=True)
    finally:
        if succeeded and not args.keep_work:
            shutil.rmtree(work)
        else:
            print(f'Compiler work preserved: {work}; log: {log_path}', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['linux', 'windows', 'all'], default='all')
    parser.add_argument('--deps-dir', type=Path, default=CACHE / 'deps')
    parser.add_argument('--jobs', type=int, default=3)
    parser.add_argument('--replace', action='store_true')
    parser.add_argument('--keep-work', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    for platform in (['linux', 'windows'] if args.platform == 'all' else [args.platform]):
        build(platform, args)


if __name__ == '__main__':
    main()
