#!/usr/bin/env python3
"""Read-only TH-13 format probe. Emits metadata only; never executes game code.

LZSS token semantics follow Olduvai formats/lzss.cpp (GPL-3.0-or-later),
revision a69e9204fc311c8810d696a6b799000d1bcd1642. This strict research
reader rejects exhausted input rather than synthesizing trailing zero bytes.
SPDX-License-Identifier: GPL-3.0-or-later
"""
import argparse
import hashlib
import json
from pathlib import Path


def lzss(data):
    if len(data) < 4:
        raise ValueError('missing size')
    size = int.from_bytes(data[:4], 'big')
    if not 0 < size <= 1048576 or size > (len(data)-4)*40//11:
        raise ValueError('implausible size')
    pos = 32
    def bits(n):
        nonlocal pos
        if pos+n > len(data)*8:
            raise ValueError('truncated token')
        value = 0
        for _ in range(n):
            value = (value << 1) | ((data[pos//8] >> (7-pos%8)) & 1)
            pos += 1
        return value
    window = bytearray(256)
    out = bytearray()
    while len(out) < size:
        if bits(1) == 0:
            value = bits(8)
            window[len(out)%256] = value
            out.append(value)
        else:
            length, distance = bits(2)+2, bits(8)+1
            for _ in range(min(length, size-len(out))):
                value = window[(len(out)-distance)%256]
                window[len(out)%256] = value
                out.append(value)
    return bytes(out), len(data)*8-pos


def mat_structure(data):
    """Check record boundaries only; does not certify planar pixels/animation."""
    if len(data) < 2:
        raise ValueError('no sprite count')
    count, pos = int.from_bytes(data[:2], 'big'), 2
    if not 0 < count <= 4096:
        raise ValueError('invalid sprite count')
    for _ in range(count):
        if pos+6 > len(data):
            raise ValueError('truncated sprite header')
        w, h, packed = [int.from_bytes(data[pos+i:pos+i+2], 'big') for i in (0,2,4)]
        if not 0 < w <= 2048 or not 0 < h <= 2048:
            raise ValueError('invalid dimensions')
        pos += 6 + (packed or ((w+7)//8)*h*5)
        if pos > len(data):
            raise ValueError('truncated sprite pixels')
    return dict(sprite_count=count, trailing_bytes=len(data)-pos)


def inspect(folder, prehistorik=False):
    result = []
    for p in sorted(folder.rglob('*')):
        if not p.is_file() or p.name.endswith(':Zone.Identifier'):
            continue
        data = p.read_bytes()
        row = dict(path=p.relative_to(folder).as_posix(), size=len(data),
                   sha256=hashlib.sha256(data).hexdigest())
        if data[:4] == bytes.fromhex('000003f3'):
            row['format'] = 'Amiga HUNK_HEADER (not executed)'
        elif data[:4] == b'FORM' and data[8:12] == b'8SVX':
            row['format'] = 'IFF 8SVX'
        elif p.suffix.lower() == '.pat':
            row['format'] = 'PAT sequencer: decoder unverified'
        elif prehistorik and p.suffix.upper() == '.MAT' and p.name.lower() != 'char_set1.mat':
            try:
                decoded, remaining = lzss(data)
                row.update(format='LZSS decoded', decoded_size=len(decoded),
                           decoded_sha256=hashlib.sha256(decoded).hexdigest(),
                           remaining_bits=remaining)
                row['mat_records'] = mat_structure(decoded)
            except ValueError as e:
                row['probe_error'] = str(e)
        else:
            row['format'] = 'unverified/raw resource'
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('originals', type=Path)
    args = parser.parse_args()
    print(json.dumps(dict(fox=inspect(args.originals/'fox-amiga'),
                          prehistorik1=inspect(args.originals/'prehistorik2-amiga', True)), indent=2))


if __name__ == '__main__':
    main()
