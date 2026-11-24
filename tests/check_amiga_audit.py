#!/usr/bin/env python3
"""Synthetic corruption/boundary tests; contains no commercial data."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('audit', Path(__file__).resolve().parents[1]/'tools/audit_amiga_data.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def stream(size, bits):
    bits += '0' * (-len(bits) % 8)
    return size.to_bytes(4, 'big') + int(bits, 2).to_bytes(len(bits)//8, 'big')


class AuditTests(unittest.TestCase):
    def test_literal_overlap_and_ring(self):
        data = stream(301, '0'+format(65,'08b') + ('1'+'11'+'00000000')*60)
        decoded, remaining = audit.lzss(data)
        self.assertEqual(decoded, b'A'*301)
        self.assertLess(remaining, 8)

    def test_zero_initialized_window(self):
        self.assertEqual(audit.lzss(stream(5, '1'+'11'+'11111111'))[0], bytes(5))

    def test_truncated_and_oversized(self):
        for data in (b'', bytes.fromhex('ffffffff'), (1).to_bytes(4,'big')+b'\0'):
            with self.assertRaises(ValueError):
                audit.lzss(data)

    def test_mat_boundaries(self):
        data = bytes.fromhex('0001000800010000')+bytes(5)
        self.assertEqual(audit.mat_structure(data), dict(sprite_count=1, trailing_bytes=0))
        with self.assertRaises(ValueError):
            audit.mat_structure(data[:-1])


if __name__ == '__main__':
    unittest.main()
