#!/usr/bin/env python3
"""Exercise the real C++ CLI and v1 schema. Original data is optional and read-only."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from validate_contract import loads, validate


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--windows', action='store_true', help='Execute a Windows binary through WSL interop')
    parser.add_argument('--orig-games', type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    checks = 0

    def path(p):
        p = str(p.resolve())
        return subprocess.check_output(['wslpath', '-w', p], text=True).strip() if args.windows else p

    def invoke(user, *operands, error=None):
        nonlocal checks
        command = [str(binary), '--user-dir', path(user)]
        command.extend(path(x) if isinstance(x, Path) else x for x in operands)
        result = subprocess.run(command, text=True, encoding='utf-8', capture_output=True, timeout=60)
        checks += 1
        if error:
            assert result.returncode != 0, result.stdout
            doc = loads(result.stderr)
            assert doc['error'] == error, doc
            return doc
        assert result.returncode == 0, result.stderr
        doc = loads(result.stdout)
        if isinstance(doc, dict) and doc.get('kind') == 'catalog':
            validate(doc)
        return doc

    def snapshot(folder):
        return {str(p.relative_to(folder)): (p.stat().st_size, p.stat().st_mtime_ns,
                hashlib.sha256(p.read_bytes()).hexdigest())
                for p in folder.rglob('*') if p.is_file()}

    with tempfile.TemporaryDirectory(prefix='catálogo TH-04 ', dir=build) as tmp:
        root = Path(tmp)
        user = root / 'usuario á'
        invoke(user, 'list', error='E_CATALOG_NOT_FOUND')
        initial = invoke(user, 'init')
        assert initial['revision'] == 1 and not initial['installations']
        assert invoke(user, 'init') == initial
        catalog_file = user / 'config/catalog.json'
        raw = catalog_file.read_bytes()
        # Failed/stale writes must not alter the current file.
        invoke(user, '--expected-revision', '0', 'init', error='E_REVISION')
        assert catalog_file.read_bytes() == raw
        invalid = dict(initial, schema_version=2, future_extension=True)
        catalog_file.write_text(json.dumps(invalid), encoding='utf-8')
        future = catalog_file.read_bytes()
        invoke(user, 'init', error='E_SCHEMA_VERSION')
        assert catalog_file.read_bytes() == future
        catalog_file.write_bytes(raw)
        # A second process cannot write while another client holds the lock.
        if not args.windows:
            import fcntl
            with (user / 'config/catalog.lock').open('r+') as lock:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                invoke(user, 'init', error='E_LOCKED')
        unknown = root / 'desconocido.adf'
        unknown.write_bytes(b'DOS\0unknown image')
        scan = invoke(user, 'scan', unknown)
        assert scan[0]['installation'] is None and scan[0]['diagnostics']['reason'] == 'unknown_adf'
        invoke(user, 'add', unknown, error='E_NO_MATCH')
        assert catalog_file.read_bytes() == raw
        if args.orig_games:
            originals = args.orig_games.resolve()
            before = snapshot(originals)
            c = invoke(user, 'add', originals, '--recursive')
            profiles = json.loads((ROOT / 'core/data_profiles.json').read_text())
            for i in c['installations']:
                p = next(p for p in profiles if (p['game_id'], p['edition_id']) == (i['game_id'], i['edition_id']))
                manifest = {key: p[key] for key in ('game_id', 'edition_id', 'variant_id', 'files')}
                manifest['version'] = 1
                canonical = json.dumps(manifest, sort_keys=True, separators=(',', ':')).encode('utf-8')
                assert i['fingerprint'] == hashlib.sha256(canonical).hexdigest()
            assert len(c['installations']) == 8
            assert sum(len(i['locations']) for i in c['installations']) == 12
            assert all(i['status'] in {'recognized', 'needs_preparation', 'missing_engine'} for i in c['installations'])
            assert invoke(user, 'add', originals, '--recursive') == c
            blues = next(i for i in c['installations'] if i['game_id'] == 'blues' and i['edition_id'] == 'amiga')
            c = invoke(user, 'recheck', blues['id'])
            assert next(i for i in c['installations'] if i['id'] == blues['id'])['status'] == 'recognized'
            # Add ADF first, then extraction; recheck must prefer an available directory.
            alternate = root / 'usuario ADF'
            image_first = invoke(alternate, 'add', originals / 'blues.adf')
            assert image_first['installations'][0]['status'] == 'needs_preparation'
            image_and_dir = invoke(alternate, 'add', originals / 'blues-amiga')
            check = invoke(alternate, 'recheck', image_first['installations'][0]['id'])
            assert check['installations'][0]['status'] == 'recognized'
            assert image_and_dir['installations'][0]['id'] == check['installations'][0]['id']
            # A copied original is moved outside the catalog: only relink its reference.
            copied = root / 'copia DOS á'
            shutil.copytree(originals / 'prehistorik-dos', copied)
            copied_catalog = invoke(root / 'usuario movido', 'add', copied)
            old = copied_catalog['installations'][0]
            moved = root / 'movido con ñ'
            copied.rename(moved)
            updated = invoke(root / 'usuario movido', 'relocate', old['id'], moved)['installations'][0]
            for key in ('id', 'fingerprint', 'save_compatibility_id'):
                assert old[key] == updated[key]
            invoke(root / 'usuario movido', 'remove', old['id'])
            assert moved.is_dir() and len(list(moved.iterdir())) > 0
            # Never create catalog files beneath original data.
            invoke(originals / 'NO-DEBE-CREARSE', 'add', originals, error='E_SOURCE_OVERLAP')
            assert not (originals / 'NO-DEBE-CREARSE').exists()
            assert snapshot(originals) == before
            print(f'Originals unchanged: {len(before)} files; 8 identities / 12 locations')
    print(f'OK: {checks} CLI operations + schema, paths and persistence checks')


if __name__ == '__main__':
    main()
