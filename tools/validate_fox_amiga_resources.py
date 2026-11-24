#!/usr/bin/env python3
"""TH-14 reference check: emulate only pinned, isolated packer/decoder routines.

No Amiga OS, trainer, game loop, audio or hardware is executed. External amitools
and Unicorn are used only by this research tool, never by the native reader.
No executable bytes are embedded. Output must be outside the original directory.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

MAIN_SHA = '18ca46dd02a3ad3623b03d425f0e0ae489b5090498882aea6d2578161010bc45'


def snapshot(root):
    return {str(p.relative_to(root)): (p.stat().st_size, p.stat().st_mtime_ns,
            hashlib.sha256(p.read_bytes()).hexdigest())
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--datapath', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--amitools', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    args = parser.parse_args()
    root, output = args.datapath.resolve(), args.output.resolve()
    if output == root or root in output.parents or output.exists():
        raise ValueError('Use a new output directory outside original data')
    before = snapshot(root)
    raw = (root/'main').read_bytes()
    if hashlib.sha256(raw).hexdigest() != MAIN_SHA:
        raise ValueError('Executable revision not recognized: refuse to emulate')
    sys.path.insert(0, str(args.amitools.resolve()))
    from amitools.binfmt.hunk.BinFmtHunk import BinFmtHunk
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn.m68k_const import (UC_CPU_M68K_M68000, UC_M68K_REG_A1,
        UC_M68K_REG_A4, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_D6,
        UC_M68K_REG_PC)
    image = BinFmtHunk().load_image(str(root/'main'))
    if [s.get_size() for s in image.segments] != [80, 8, 3680, 9644, 74068, 496, 50324, 516]:
        raise ValueError('Unexpected Hunk layout')
    u = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
    u.ctl_set_cpu_model(UC_CPU_M68K_M68000)
    u.mem_map(0x10000, 0x300000)
    bases = [0x20000+i*0x30000 for i in range(8)]
    for base, segment in zip(bases, image.segments):
        if segment.get_data():
            u.mem_write(base, segment.get_data())
    helper, stop, stack = 0x10000, 0x10002, 0x2f0000
    u.mem_write(helper, b'\x4e\x75\x4e\x71')
    phase, payload, cursor, heap = 'packer', b'', 0, 0x180000

    def be(address):
        return int.from_bytes(u.mem_read(address, 4), 'big')

    def hook(cpu, address, size, user):
        nonlocal cursor, heap
        if address == stop:
            cpu.emu_stop()
            return
        if phase == 'packer':
            if address == helper:
                index = cpu.reg_read(UC_M68K_REG_D6)+1
                if index >= len(bases):
                    raise ValueError('Packer segment index out of bounds')
                cpu.reg_write(UC_M68K_REG_A1, bases[index])
            elif not bases[5] <= address < bases[5]+496:
                raise ValueError('Packer execution escaped its isolated segment')
            return
        if 0xe34d0 <= address < 0xe3cd6:
            return
        sp = cpu.reg_read(UC_M68K_REG_A7)
        if address == 0xf1d30:  # Read(handle, buffer, length)
            dest, requested = be(sp+8), be(sp+12)
            if requested > 512 or not 0x180000 <= dest < 0x280000-requested:
                raise ValueError('Reference read outside scratch buffers')
            data = payload[cursor:cursor+requested]
            if data:
                cpu.mem_write(dest, data)
            cursor += len(data)
            cpu.reg_write(UC_M68K_REG_D0, len(data))
        elif address == 0xf1dd4:  # AllocMem(length, flags)
            length = be(sp+4)
            if length > 65536 or heap+length >= 0x230000:
                raise ValueError('Reference allocation limit')
            cpu.mem_write(heap, bytes(length))
            cpu.reg_write(UC_M68K_REG_D0, heap)
            heap = (heap+length+3)&~3
        else:
            raise ValueError(f'Reference decoder escaped to {address:x}')
        # Return to caller directly: no OS implementation is entered.
        cpu.reg_write(UC_M68K_REG_PC, be(sp))
        cpu.reg_write(UC_M68K_REG_A7, sp+4)

    u.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        frame = stop.to_bytes(4, 'big')+b''.join(v.to_bytes(4, 'big') for v in arguments)
        u.mem_write(stack, frame)
        u.reg_write(UC_M68K_REG_A7, stack)
        u.emu_start(address, stop, count=15000000)
        if u.reg_read(UC_M68K_REG_PC) != stop:
            raise ValueError('Reference instruction budget exceeded')
        return u.reg_read(UC_M68K_REG_D0)

    u.reg_write(UC_M68K_REG_A4, helper)
    u.reg_write(UC_M68K_REG_D6, 4)
    call(bases[5])
    phase = 'decoder'
    data = bytes(u.mem_read(bases[3], 9644))
    words = lambda offset, count: [int.from_bytes(data[p:p+2], 'big')
                                  for p in range(offset, offset+count*2, 2)]
    palette = dict(main_sha256=MAIN_SHA, palette=words(0x2aa, 16),
                   level_color14=words(0x340, 16))
    output.mkdir(parents=True)
    palette_path = output/'palette.json'
    palette_path.write_text(json.dumps(palette, indent=2)+'\n')
    process = subprocess.run([str(args.probe.resolve()), '--datapath', str(root),
        '--output', str(output/'preview'), '--palette', str(palette_path)],
        capture_output=True, text=True, timeout=60)
    if process.returncode:
        raise ValueError(process.stderr)
    native = json.loads(process.stdout)
    report = dict(schema_version=1, main_sha256=MAIN_SHA, playable=False,
                  reference='isolated original 68000 resource decoders', resources=[])
    for row in native['resources']:
        packed = (root/row['name']).read_bytes()
        payload, cursor, heap = packed[6:], 0, 0x180000
        method, expected = packed[0], row['decoded_size']
        call(0xe3582 if method == 2 else 0xe3b98, (1,))
        decoded = bytearray()
        while len(decoded) < expected:
            count = min(512, expected-len(decoded))
            actual = call(0xe384c if method == 2 else 0xe3c90, (1, 0x240000, count)) & 0xffff
            if actual < count or actual > count+4096 or actual > expected-len(decoded):
                raise ValueError(f'Reference short output: {row["name"]}: {actual}/{count}')
            decoded.extend(u.mem_read(0x240000, actual))
        sha = hashlib.sha256(decoded).hexdigest()
        if sha != row['decoded_sha256']:
            raise ValueError(f'Native/reference mismatch: {row["name"]}')
        report['resources'].append(dict(row, reference_match=True))
    report.update(resources_verified=len(report['resources']), native_sprites=native['sprites'],
                  originals_unchanged=before == snapshot(root))
    if not report['originals_unchanged']:
        raise ValueError('Originals changed during validation')
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='resources'}, indent=2))


if __name__ == '__main__':
    main()
