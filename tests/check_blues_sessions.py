#!/usr/bin/env python3
"""Real managed Blues sessions, migration, protocol failures and orphan-safe locks.

Requires the user's audited data. Nothing in orig-games or legacy sources is
modified; profiles and receipts live in an isolated temporary build directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from validate_contract import loads, validate, validate_session


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', type=Path, required=True)
    parser.add_argument('--orig-games', type=Path, required=True)
    parser.add_argument('--windows', action='store_true')
    parser.add_argument('--export-fixtures', type=Path)
    parser.add_argument('--import-fixtures', type=Path)
    args = parser.parse_args()
    binary = args.bin_dir.resolve()
    suffix = '.exe' if args.windows else ''
    environment = os.environ.copy()
    environment.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    if args.windows:
        # WSLENV forwards these explicit strings to native Windows, with no shell.
        forwarded = environment.get('WSLENV', '').split(':')
        environment['WSLENV'] = ':'.join(x for x in forwarded + ['SDL_VIDEODRIVER', 'SDL_AUDIODRIVER'] if x)

    def native(p):
        p = str(p.resolve())
        return subprocess.check_output(['wslpath', '-w', p], text=True).strip() if args.windows else p

    def local(text):
        return Path(subprocess.check_output(['wslpath', '-u', text], text=True).strip()) if args.windows else Path(text)

    def snapshot(root):
        return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns,
                hashlib.sha256(p.read_bytes()).hexdigest()) for p in root.rglob('*') if p.is_file()}

    originals = args.orig_games.resolve()
    before = snapshot(originals)
    operations = 0
    with tempfile.TemporaryDirectory(prefix='sesiones á ', dir=ROOT / 'build') as temporary:
        root = Path(temporary)
        user = root / 'usuario ñ'
        legacy = root / 'legado ü'
        (legacy / 'saves').mkdir(parents=True)
        (legacy / 'preferences.ini').write_text('version=1\ncamera=classic\nhud=fixed\nmusic_volume=42\neffects_volume=61\ninitial_lives_1=7\n', encoding='utf-8')
        (legacy / 'saves/slot01.bbs').write_bytes(b'broken old save, keep exactly')
        initial_legacy = snapshot(legacy)

        def invoke(exe, operands, code=0):
            nonlocal operations
            r = subprocess.run([str(binary / (exe + suffix)), *operands], env=environment,
                               capture_output=True, text=True, encoding='utf-8', timeout=60)
            operations += 1
            if r.returncode != code:
                for log in root.rglob('stderr.log'):
                    print(str(log), log.read_text(encoding='utf-8', errors='replace')[-3000:], file=sys.stderr)
            assert r.returncode == code, (exe, r.returncode, r.stdout, r.stderr)
            return loads(r.stdout) if r.stdout.strip() else r.stderr

        catalog = invoke('titus-catalog', ['--user-dir', native(user), 'add', native(originals), '--recursive'])
        validate(catalog)
        dos, amiga = [next(i['id'] for i in catalog['installations'] if i['game_id'] == 'blues' and i['edition_id'] == edition) for edition in ('dos', 'amiga')]

        def run(installation=dos, extras=(), code=0, storage=user):
            return invoke('titus-session', ['--user-dir', native(storage), '--installation', installation,
                          '--legacy-root', native(legacy), *extras], code)

        def validate_channels(storage=user, skip_bad=False):
            valid = 0
            for folder in (storage / 'cache/sessions').iterdir():
                request = loads((folder / 'request.json').read_text())
                validate(request)
                if request['kind'] == 'inspect_request':
                    inspection = loads((folder / 'inspection.json').read_text())
                    validate(inspection)
                    assert inspection['inspection_id'] == request['inspection_id']
                else:
                    result_path = folder / 'result.json'
                    if not result_path.exists() and skip_bad:
                        continue
                    events = [loads(line) for line in (folder / 'events.jsonl').read_text().splitlines()]
                    result = loads(result_path.read_text())
                    commands = [loads(line) for line in (folder / 'commands.jsonl').read_text().splitlines()]
                    assert validate_session(request, events, result, 0, commands)
                    valid += 1
            return valid

        inspection = run(extras=['--inspect-only'])
        assert inspection['ready'] is False
        profile = next((user / 'data/profiles').glob('*/games/blues/blues.full.1991'))
        receipt_bytes = (profile / 'migration-receipt.json').read_bytes()
        receipt = loads(receipt_bytes.decode('utf-8'))
        assert receipt['state'] == 'complete'
        assert (profile / 'saves/slot01.bbs').read_bytes() == (legacy / 'saves/slot01.bbs').read_bytes()
        assert (profile / 'legacy-initial/native/blues-family/preferences.ini').read_bytes() == (legacy / 'preferences.ini').read_bytes()
        bad = next(f for f in receipt['files'] if f['file'] == 'saves/slot01.bbs')
        assert bad['validation'] == 'damaged'
        result = run(extras=['--test-ms', '250'])
        assert result['ready'] and result['state'] == 'done'
        ini = (profile / 'native/blues-family/preferences.ini').read_text()
        assert 'camera=classic' in ini and 'hud=fixed' in ini and 'music_volume=42' in ini and 'initial_lives_1=7' in ini
        assert snapshot(legacy) == initial_legacy, 'Legacy files changed during import or play'
        # Later classic progress must not silently replace managed progress.
        (legacy / 'preferences.ini').write_text('version=1\nmusic_volume=13\n')
        result = run(amiga, ['--test-ms', '250'])
        assert result['ready'] and 'music_volume=42' in (profile / 'native/blues-family/preferences.ini').read_text()
        assert (profile / 'migration-receipt.json').read_bytes() == receipt_bytes
        result = run(dos, ['--test-ms', '2100', '--gameplay-probe'])
        observation = loads((local(result['directory']) / 'probe-observation.json').read_text())
        assert observation['rewind'] and observation['amiga'] and observation['hud'] == 0 and observation['view_step'] == 1, observation
        assert len(observation['slots']) == 8 and all(s['status'] == 'ready' and s['has_preview'] for s in observation['slots'])
        events = [loads(x) for x in (local(result['directory']) / 'events.jsonl').read_text().splitlines()]
        assert [e['event'] for e in events].count('ready') == 1
        assert 'return_cancelled' in [e['event'] for e in events]
        fixtures = root / 'fixtures'
        shutil.copytree(profile / 'saves', fixtures)
        if args.import_fixtures:
            shutil.rmtree(fixtures)
            shutil.copytree(args.import_fixtures, fixtures)
        if args.export_fixtures:
            args.export_fixtures.mkdir(parents=True, exist_ok=True)
            for p in fixtures.glob('*.bbs'):
                shutil.copy2(p, args.export_fixtures / p.name)
        # A fresh profile imports both original BBS v1 and v2 without re-encoding.
        for p in fixtures.glob('*.bbs'):
            shutil.copy2(p, legacy / 'saves' / p.name)
        v1 = bytearray((legacy / 'saves/slot02.bbs').read_bytes()[:-128*80*3-8])
        struct.pack_into('<III', v1, 8, 1, len(v1)-24, zlib.crc32(v1[24:]))
        (legacy / 'saves/slot02.bbs').write_bytes(v1)
        future = bytearray((legacy / 'saves/slot03.bbs').read_bytes())
        struct.pack_into('<I', future, 8, 99)
        (legacy / 'saves/slot03.bbs').write_bytes(future)
        damaged = bytearray((legacy / 'saves/slot04.bbs').read_bytes());damaged[99] ^= 1
        (legacy / 'saves/slot04.bbs').write_bytes(damaged)
        second = root / 'segundo perfil'
        c2 = invoke('titus-catalog', ['--user-dir', native(second), 'add', native(originals), '--recursive'])
        second_id = next(i['id'] for i in c2['installations'] if i['game_id'] == 'blues' and i['edition_id'] == 'dos')
        (second / 'data/profiles' / c2['profiles'][0]['id'] / 'games/blues/blues.full.1991').mkdir(parents=True)
        result = run(second_id, ['--inspect-only'], storage=second)
        info = loads((local(result['directory']) / 'inspection.json').read_text())
        validate(info)
        slots = {s['slot']: s for s in info['save_index']['slots']}
        assert slots[1]['status'] == slots[2]['status'] == 'ready'
        assert slots[1]['has_preview'] and slots[2]['format_version'] == 1 and not slots[2]['has_preview']
        assert slots[3]['status'] == 'incompatible' and slots[4]['status'] == 'damaged'
        second_profile = next((second / 'data/profiles').glob('*/games/blues/blues.full.1991'))
        for p in (legacy / 'saves').glob('*.bbs'):
            assert p.read_bytes() == (second_profile / 'saves' / p.name).read_bytes()
        # The native runtime must retain the lock after its parent exits abruptly.
        orphan = run(dos, ['--test-ms', '6000', '--test-orphan'])
        assert orphan['state'] == 'orphaned'
        busy = run(extras=['--inspect-only'], code=1)
        assert 'uso' in busy or 'busy' in busy
        folder = local(orphan['directory']);req = loads((folder / 'request.json').read_text())
        with (folder / 'commands.jsonl').open('a', encoding='utf-8') as stream:
            stream.write(json.dumps(dict(schema_version=1, kind='command', session_id=req['session_id'], seq=1, command='return_to_library'))+'\n')
        deadline = time.monotonic()+10
        while not (folder / 'result.json').exists():
            assert time.monotonic() < deadline
            time.sleep(0.03)
        time.sleep(0.2)
        run(extras=['--inspect-only'])
        clean_count = validate_channels()
        # Exit zero and a window are never enough to declare successful loading.
        for fault in ('before_ready', 'missing_result', 'failed_zero', 'wrong_session'):
            failure = run(extras=['--test-ms', '150', '--test-fault', fault], code=1)
            assert failure['state'] == 'failed'
            if fault == 'before_ready':
                assert failure['ready'] is False
        rotated = run(extras=['--test-ms', '150', '--test-fault', 'log_flood'])
        log = local(rotated['directory']) / 'stdout.log'
        assert log.stat().st_size < 1024*1024 and 'log rotated' in log.read_text()
        # The original legacy inputs are untouched except for our deliberate fixture edits.
        assert (legacy / 'saves/slot01.bbs').read_bytes() == (fixtures / 'slot01.bbs').read_bytes()
        assert before == snapshot(originals), 'Original game resources changed'
        validate(loads((binary / 'engines/registry.json').read_text()))
        print(f'OK: {operations} native session operations, {clean_count} validated game sessions; BBS v1/v2, eight thumbnails, F6/HUD/zoom/rewind, migration and orphan lock')
        print(f'Originals unchanged: {len(before)} files')


if __name__ == '__main__':
    main()
