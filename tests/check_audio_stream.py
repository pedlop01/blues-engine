#!/usr/bin/env python3
"""Check native DOS PCM continuity across arbitrary SDL callback boundaries.

Uses the production renderer and the user's original executable. No Unicorn
dependency; compiles a temporary shared library with the host C compiler.
"""
import argparse
import ctypes as C
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    args = parser.parse_args()
    exe = args.exe.read_bytes()
    with tempfile.TemporaryDirectory(prefix='blues-audio-stream-') as tmp:
        library = Path(tmp) / 'audio.so'
        subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-I', str(ROOT),
                        str(ROOT / 'dos_audio.c'),
                        str(ROOT / 'third_party/nuked-opl3/opl3.c'),
                        '-o', str(library)], check=True)
        lib = C.CDLL(str(library))
        lib.dos_audio_create.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
        lib.dos_audio_create.restype = C.c_void_p
        lib.dos_audio_music.argtypes = [C.c_void_p, C.c_int]
        lib.dos_audio_sound.argtypes = [C.c_void_p, C.c_int]
        lib.dos_audio_render.argtypes = [C.c_void_p, C.POINTER(C.c_int16), C.c_int]
        lib.dos_audio_destroy.argtypes = [C.c_void_p]

        def render(song, rate, fragmented):
            audio = lib.dos_audio_create(exe, len(exe), rate)
            assert audio
            lib.dos_audio_music(audio, song)
            sizes = [1, 17, 511, 2048, 4096, 321]
            output = bytearray()
            index = 0
            for effect, duration in [(None, rate * 2), (5, rate // 3), (11, rate * 2)]:
                if effect is not None:
                    lib.dos_audio_sound(audio, effect)
                while duration:
                    count = min(duration, sizes[index % len(sizes)]) if fragmented else duration
                    block = (C.c_int16 * count)()
                    lib.dos_audio_render(audio, block, count)
                    output.extend(bytes(block))
                    duration -= count
                    index += 1
            lib.dos_audio_destroy(audio)
            return output

        for rate in (22050, 44100):
            for song in range(4):
                whole = render(song, rate, False)
                chunked = render(song, rate, True)
                assert whole == chunked, f'callback discontinuity: song {song}, rate {rate}'
                samples = (C.c_int16 * (len(whole) // 2)).from_buffer_copy(whole)
                peak = max(abs(v) for v in samples)
                clipped = sum(v in (-32768, 32767) for v in samples)
                assert peak > 100, f'silent song {song}'
                assert clipped == 0, f'PCM clipping in song {song}'
                print(f'{rate}Hz song {song}: identical PCM across callback sizes, peak {peak}, no clipping')


if __name__ == '__main__':
    main()
