#!/usr/bin/env python3
"""Compare native AdLib writes with the actual DOS IRQ routine in Unicorn.

Requires a C compiler, Python unicorn, and the user's original DOS 1.EXE.
No original game data is copied into the repository.
Usage: python3 tests/check_dos_audio.py /path/to/1.EXE [--ticks 12000]
"""
import argparse
import ctypes as C
from pathlib import Path
import struct
import subprocess
import tempfile

from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_CODE, UC_HOOK_INSN
from unicorn.x86_const import (
    UC_X86_REG_CS, UC_X86_REG_IP, UC_X86_REG_DS, UC_X86_REG_ES,
    UC_X86_REG_SS, UC_X86_REG_SP, UC_X86_REG_AX, UC_X86_REG_BX,
    UC_X86_REG_CX, UC_X86_REG_EFLAGS, UC_X86_INS_OUT,
)

ROOT = Path(__file__).resolve().parents[1]
DATA = 0x11E00
CODE = 0x10A20


class Original:
    def __init__(self, exe, song):
        self.uc = u = Uc(UC_ARCH_X86, UC_MODE_16)
        u.mem_map(0, 0x100000)
        u.mem_write(0, exe)
        # The image is placed at its file offset; relocate segment references.
        count, table = struct.unpack_from('<H', exe, 6)[0], struct.unpack_from('<H', exe, 24)[0]
        for n in range(count):
            off, seg = struct.unpack_from('<HH', exe, table + n * 4)
            p = 0x2200 + seg * 16 + off
            value = (struct.unpack_from('<H', exe, p)[0] + 0x220) & 0xffff
            u.mem_write(p, struct.pack('<H', value))
        self.writes = []
        self.index = 0
        u.hook_add(UC_HOOK_INSN, self.out, None, 1, 0, UC_X86_INS_OUT)
        u.hook_add(UC_HOOK_CODE, self.code)
        # BIOS IRQ continuation: a harmless return address in our harness.
        u.mem_write(CODE + 0x63D, struct.pack('<HH', 0xFF00, CODE >> 4))
        self.exe = exe
        self.music(song)

    def music(self, song):
        u, exe = self.uc, self.exe
        seq, instruments, divider = struct.unpack_from('<HHB', exe, DATA + 0x82C3 + song * 8)
        for reg, value in ((UC_X86_REG_DS, DATA >> 4), (UC_X86_REG_ES, DATA >> 4),
                           (UC_X86_REG_AX, seq), (UC_X86_REG_BX, instruments),
                           (UC_X86_REG_CX, divider)):
            u.reg_write(reg, value)
        self.run(0x10B63, irq=False)

    def code(self, u, address, size, _):
        if address == CODE + 0xFF00:
            u.emu_stop()

    def out(self, u, port, size, value, _):
        if port == 0x388:
            self.index = value
        elif port == 0x389:
            self.writes.append((self.index << 8) | value)

    def run(self, address, irq=True):
        u = self.uc
        u.reg_write(UC_X86_REG_CS, CODE >> 4)
        u.reg_write(UC_X86_REG_IP, address - CODE)
        u.reg_write(UC_X86_REG_SS, 0x8000)
        u.reg_write(UC_X86_REG_SP, 0xFF00)
        u.reg_write(UC_X86_REG_EFLAGS, 2)
        if irq:
            u.mem_write(0x8FF00, struct.pack('<HHH', 0xFF00, CODE >> 4, 2))
        else:
            u.mem_write(0x8FF00, struct.pack('<H', 0xFF00))
        u.emu_start(address, 0xFFFFF, count=1000000)
        assert u.reg_read(UC_X86_REG_IP) == 0xFF00, 'DOS routine did not return'

    def tick(self):
        self.writes = []
        self.run(0x10F27)
        return self.writes

    def sound(self, number):
        pointer = struct.unpack('<H', self.uc.mem_read(DATA + 0x82E5 + number * 2, 2))[0]
        self.uc.mem_write(DATA + 0x82E3, struct.pack('<H', pointer + 1))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    parser.add_argument('--ticks', type=int, default=12000)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    with tempfile.TemporaryDirectory(prefix='blues-audio-test-') as tmp:
        library = Path(tmp) / 'audio.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-o', str(library),
                        str(ROOT / 'tests/dos_audio_harness.c'),
                        str(ROOT / 'third_party/nuked-opl3/opl3.c')], check=True)
        lib = C.CDLL(str(library))
        lib.dos_audio_create.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
        lib.dos_audio_create.restype = C.c_void_p
        lib.dos_audio_music.argtypes = [C.c_void_p, C.c_int]
        lib.dos_audio_sound.argtypes = [C.c_void_p, C.c_int]
        lib.dos_audio_destroy.argtypes = [C.c_void_p]
        lib.test_tick.argtypes = [C.c_void_p, C.POINTER(C.c_uint16)]
        lib.test_tick.restype = C.c_uint
        lib.test_failed.argtypes = [C.c_void_p]
        lib.dos_audio_render.argtypes = [C.c_void_p, C.POINTER(C.c_int16), C.c_int]
        assert not lib.dos_audio_create(exe, 42, 22050), 'truncated EXE accepted'
        damaged = bytearray(exe)
        damaged[0x112C1] ^= 1
        assert not lib.dos_audio_create(bytes(damaged), len(damaged), 22050)
        output = (C.c_uint16 * 16384)()
        for song in range(4):
            a = lib.dos_audio_create(exe, len(exe), 22050)
            assert a, 'unsupported executable'
            lib.dos_audio_music(a, song)
            dos = Original(exe, song)
            writes = 0
            for tick in range(args.ticks):
                # Exercise every effect against the music, repeatedly.
                if tick % 137 == 30:
                    sound = (tick // 137) % 18
                    dos.sound(sound)
                    lib.dos_audio_sound(a, sound)
                expected = dos.tick()
                n = lib.test_tick(a, output)
                actual = list(output[:n])
                assert not lib.test_failed(a), f'native decoder failed: song {song}, tick {tick}'
                assert actual == expected, (
                    f'song {song}, tick {tick}:\n'
                    f'expected {[hex(x) for x in expected]}\n'
                    f'actual   {[hex(x) for x in actual]}')
                writes += n
            lib.dos_audio_destroy(a)
            # Check the actual OPL renderer, not only the sequencer's trace.
            a = lib.dos_audio_create(exe, len(exe), 22050)
            lib.dos_audio_music(a, song)
            samples = (C.c_int16 * 44100)()
            lib.dos_audio_render(a, samples, len(samples))
            assert not lib.test_failed(a)
            assert max(samples) - min(samples) > 100, f'silent song {song}'
            lib.dos_audio_destroy(a)
            print(f'song {song}: {args.ticks} ticks, {writes} OPL writes identical; PCM nonzero')

        a = lib.dos_audio_create(exe, len(exe), 22050)
        dos = Original(exe, 0)
        for song in (0, 1, 2, 3, 0):
            lib.dos_audio_music(a, song)
            dos.music(song)
            for tick in range(200):
                if tick == 10:
                    dos.sound(8)
                    lib.dos_audio_sound(a, 8)
                expected = dos.tick()
                n = lib.test_tick(a, output)
                assert not lib.test_failed(a)
                assert list(output[:n]) == expected, f'song switch {song}, tick {tick}'
        lib.dos_audio_destroy(a)
        print('song transitions: 1000 ticks with shared chip state identical')


if __name__ == '__main__':
    main()
