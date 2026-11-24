#!/usr/bin/env python3
"""Preview or remove obsolete local build artifacts, preserving current deliveries and state.

Only ignored outputs are candidates. No tracked files or orig-games are removed.
Use --apply to execute the printed plan. Saved states/configurations are copied
and verified before any obsolete directory is removed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
KEEP = {'linux', 'windows'}
STATE = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'titus-engine'
BACKUP = STATE / 'workspace-backups' / ('cleanup-' + hashlib.sha256(str(ROOT).encode()).hexdigest()[:12])

STATE_NAMES = {'preferences.ini', 'preferences.json', 'catalog.json',
               'game-settings.json', 'library.json'}


def regular_files(path):
    if path.is_symlink():
        return
    if path.is_file():
        yield path
        return
    for directory, dirs, files in os.walk(path, followlinks=False):
        dirs[:] = [d for d in dirs if not (Path(directory) / d).is_symlink()]
        for name in files:
            file = Path(directory) / name
            if not file.is_symlink():
                yield file


def size(path):
    return sum(p.stat().st_size for p in regular_files(path))


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def remove(path):
    if path.is_symlink() or path.is_file():
        path.unlink()
    else:
        shutil.rmtree(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    if not BUILD.exists():
        print('No build directory; nothing to clean.')
        return
    if BUILD.is_symlink():
        raise SystemExit('Refusing to clean a symlinked build directory.')
    tracked = set(subprocess.check_output(
        ['git', 'ls-files', '-z'], cwd=ROOT).decode().split('\0'))
    if any(p.startswith('build/') for p in tracked):
        raise SystemExit('Tracked files found in build/; review manually.')

    keep = set(KEEP)
    targets = [p for p in sorted(BUILD.iterdir()) if p.name not in keep]
    # Stale Make outputs outside build/, including Python bytecode caches.
    outside = []
    for directory, dirs, files in os.walk(ROOT, followlinks=False):
        dirs[:] = [d for d in dirs if d not in {'.git', 'build', 'orig-games', 'game-data', 'saves', 'captures', '.venv', 'venv'}
                   and not (Path(directory) / d).is_symlink()]
        for name in list(dirs):
            if name == '__pycache__':
                candidate = Path(directory) / name
                if not any(p.startswith(candidate.relative_to(ROOT).as_posix() + '/') for p in tracked):
                    outside.append(candidate)
                dirs.remove(name)
        for name in files:
            file = Path(directory) / name
            if (file.suffix in {'.o', '.d'} or file in {ROOT / 'blues', ROOT / 'blues.exe'}) and file.relative_to(ROOT).as_posix() not in tracked:
                outside.append(file)
    for p in targets + outside:
        result = subprocess.run(['git', 'check-ignore', '-q', str(p)], cwd=ROOT)
        if result.returncode:
            raise SystemExit(f'Refusing to remove a non-ignored path: {p}')
    planned = sum(size(p) for p in targets + outside)
    print(json.dumps({'apply': args.apply, 'obsolete_paths': len(targets + outside),
                      'candidate_bytes': planned, 'keep': sorted(keep)}, indent=2))
    if not args.apply:
        return

    before = size(BUILD) + sum(size(p) for p in outside)
    archive = BACKUP / 'retained-state'
    archive.mkdir(parents=True, exist_ok=True)
    manifest = archive / 'manifest.json'
    records = json.loads(manifest.read_text()) if manifest.exists() else []
    known = {(r['path'], r['sha256']) for r in records}
    objects = archive / 'objects'
    objects.mkdir(exist_ok=True)
    validation = BACKUP / 'retained-validation'
    configs = BACKUP / 'retained-configs'
    validation.mkdir(exist_ok=True)
    configs.mkdir(exist_ok=True)
    # Finish every preservation step before beginning deletion.
    for target in targets:
        for file in regular_files(target):
            relative = file.relative_to(BUILD)
            if (file.name in STATE_NAMES or file.suffix == '.bbs'
                    or re.fullmatch(r'slot-[1-8]\.json', file.name)):
                sha = digest(file)
                destination = objects / sha
                if not destination.exists():
                    shutil.copy2(file, destination)
                if digest(destination) != sha:
                    raise SystemExit(f'Backup verification failed: {file}')
                key = (relative.as_posix(), sha)
                if key not in known:
                    records.append({'path': key[0], 'sha256': sha,
                                    'bytes': file.stat().st_size,
                                    'mtime_ns': file.stat().st_mtime_ns})
                    known.add(key)
            if (file.name == 'report.json' or (file.parent == BUILD and file.suffix == '.json')
                    or (file.parent == BUILD and file.name.startswith(('pf6-', 'library-r6-')) and file.suffix == '.log')):
                dest = validation / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(file, dest)
            if file.name == 'CMakeCache.txt':
                dest = configs / relative
                dest.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(file, dest)
    manifest.write_text(json.dumps(records, indent=2, ensure_ascii=False) + '\n')
    errors = []
    for target in targets + outside:
        try:
            remove(target)
        except OSError as error:
            errors.append({'path': str(target.relative_to(ROOT)), 'error': str(error)})
    after = size(BUILD)
    report = {'before_bytes': before, 'after_bytes': after,
              'reclaimed_bytes': before - after, 'removed_paths': len(targets + outside) - len(errors),
              'preserved_state_records': len(records), 'errors': errors}
    (BACKUP / 'cleanup-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))
    if errors:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
