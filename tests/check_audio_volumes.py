#!/usr/bin/env python3
"""DOS bus separation, mute continuity and unity PCM fidelity (user assets required)."""
import argparse
import ctypes as C
import hashlib
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REFERENCE_EXE = '769ee254b154a9444205524ec4b28a8147a372ac5b2cee2e25fcd83aa90d0add'
# PCM recorded from ebd906c's unmodified Nuked OPL3 and renderer, at 44.1kHz.
UNITY = [
    '8b86b985849e4f857d72f6a296b0a400a42f16820d27ba45f6f227b36af234cd',
    'eb648b084d6273d6787a957b81107cf5eb744ed6086bb8f044b9dc45d9b5944f',
    'e53e464b5f6be0dc57e9432efba23d1f74ca1a3f7aa11462bb008f031495d5ab',
    'e0459249409b39c4fd58b8d07aa19ebba5276024f9bdfffe39ed4c984a1aa271',
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('exe', type=Path)
    exe = parser.parse_args().exe.read_bytes()
    with tempfile.TemporaryDirectory(prefix='blues-volumes-') as tmp:
        library = Path(tmp) / 'audio.so'
        subprocess.run(['cc','-shared','-fPIC','-O2','-I',str(ROOT),str(ROOT/'dos_audio.c'),
                        str(ROOT/'third_party/nuked-opl3/opl3.c'),'-o',str(library)],check=True)
        lib = C.CDLL(str(library))
        lib.dos_audio_create.argtypes=[C.c_void_p,C.c_size_t,C.c_int]
        lib.dos_audio_create.restype=C.c_void_p
        lib.dos_audio_music.argtypes=[C.c_void_p,C.c_int]
        lib.dos_audio_sound.argtypes=[C.c_void_p,C.c_int]
        lib.dos_audio_set_volumes.argtypes=[C.c_void_p,C.c_int,C.c_int]
        lib.dos_audio_render.argtypes=[C.c_void_p,C.POINTER(C.c_int16),C.c_int]
        lib.dos_audio_destroy.argtypes=[C.c_void_p]

        def create(song, music=100, effects=100):
            a=lib.dos_audio_create(exe,len(exe),44100)
            assert a
            lib.dos_audio_set_volumes(a,music,effects)
            lib.dos_audio_music(a,song)
            return a

        def render(a, count, chunks=False):
            output=bytearray()
            sizes=[1,17,2048,511,4096,29]
            i=0
            while count:
                n=min(count,sizes[i%len(sizes)]) if chunks else count
                b=(C.c_int16*n)()
                lib.dos_audio_render(a,b,n)
                output.extend(bytes(b))
                count-=n
                i+=1
            return bytes(output)

        for song in range(4):
            a=create(song)
            unity=render(a,44100)
            lib.dos_audio_sound(a,11)
            unity+=render(a,22050)
            lib.dos_audio_sound(a,5)
            unity+=render(a,22050)
            lib.dos_audio_destroy(a)
            if hashlib.sha256(exe).hexdigest()==REFERENCE_EXE:
                assert hashlib.sha256(unity).hexdigest()==UNITY[song], f'changed default PCM song {song}'
            a=create(song,0,100)
            assert not any(render(a,44100)), 'music leaked into effect-only bus'
            lib.dos_audio_sound(a,11)
            assert any(render(a,22050)), 'muting music silenced effects'
            lib.dos_audio_set_volumes(a,0,0)
            render(a,2048)
            assert not any(render(a,4096)), 'mute leaked samples'
            lib.dos_audio_destroy(a)
            a=create(song,100,0)
            assert any(render(a,44100)), 'muting effects silenced music'
            lib.dos_audio_destroy(a)
            # Gain changes must not change synthesis/sequence progress while muted.
            a,b=create(song),create(song,0,0)
            for audio in (a,b):
                render(audio,44100)
                lib.dos_audio_sound(audio,11)
                render(audio,22050)
            lib.dos_audio_set_volumes(b,100,100)
            reference,resumed=render(a,4096),render(b,4096)
            assert reference[2048*2:]==resumed[2048*2:], 'mute altered synthesis/timeline'
            lib.dos_audio_destroy(a)
            lib.dos_audio_destroy(b)
            def transitions(chunks):
                a=create(song)
                result=bytearray()
                for music,effects,fx in [(100,100,None),(30,70,5),(0,100,11),(80,0,7),(0,0,None),(100,100,None)]:
                    lib.dos_audio_set_volumes(a,music,effects)
                    if fx is not None: lib.dos_audio_sound(a,fx)
                    result.extend(render(a,10000,chunks))
                lib.dos_audio_destroy(a)
                return result
            assert transitions(False)==transitions(True), 'gain discontinuity across callbacks'
            print(f'song {song}: unity fidelity, independent mute, timeline continuity and chunked ramps passed')
        if hashlib.sha256(exe).hexdigest()!=REFERENCE_EXE:
            print('Unity golden PCM comparison skipped: different executable; other assertions passed.')


if __name__=='__main__':
    main()
