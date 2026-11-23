#!/usr/bin/env python3
"""Extract reference OPL data from the verified, user-supplied FOX.COM.

Requires Unicorn (audit dependency, not a game runtime dependency). Emulates
only the bounded decompressor, stopping before relocation/original game code.
The source executable is never launched as a host process or modified.
"""
import argparse
import hashlib
from pathlib import Path
import json
import struct

SOURCE_SHA = "a47ab6e4f8016db90e727df2eef5450edf1e16ecb6c90f5cb31ad214fef65f7b"
IMAGE_SHA = "f398540b9bd65ef64fcd7898285cafb30f95e456d8b3d7a5123d2337c0c0df41"
MUSIC_SHA = "386dbe1cad7a67e20ec3edb98bdb116c035e68cc762cd85762d2bedbb1215986"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path, help="Private cache file outside the original data directory")
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    if output.is_relative_to(source.parent):
        raise SystemExit("Choose a cache destination outside the original data directory")
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA:
        raise SystemExit("Unrecognised FOX.COM; only the verified packed variant is accepted")
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_16, UC_HOOK_CODE, UC_HOOK_INTR
    from unicorn.x86_const import UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS, UC_X86_REG_IP, UC_X86_REG_SP

    machine = Uc(UC_ARCH_X86, UC_MODE_16)
    machine.mem_map(0, 0x200000)
    base, segment = 0x10000, 0x1000
    machine.mem_write(base + 0x100, data)
    for register in (UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_ES, UC_X86_REG_SS):
        machine.reg_write(register, segment)
    machine.reg_write(UC_X86_REG_IP, 0x100)
    machine.reg_write(UC_X86_REG_SP, 0xFFFE)
    result = []

    def instruction(cpu, address, size, context):
        if address != base + 0x32A:
            return
        # Here the original MZ is reconstructed but no relocations were applied.
        image_segment = struct.unpack("<H", cpu.mem_read(base + 0x3EA, 2))[0]
        image = bytes(cpu.mem_read(image_segment * 16, 63985))
        if hashlib.sha256(image).hexdigest() == IMAGE_SHA:
            result.append(image[28677:28677 + 18749])
        cpu.emu_stop()

    def interrupt(cpu, number, context):
        # The recognised decompressor needs no DOS or BIOS services.
        cpu.emu_stop()

    machine.hook_add(UC_HOOK_CODE, instruction)
    machine.hook_add(UC_HOOK_INTR, interrupt)
    machine.emu_start(base + 0x100, 0x100000, timeout=20000000, count=3000000)
    if len(result) != 1 or hashlib.sha256(result[0]).hexdigest() != MUSIC_SHA:
        raise SystemExit("Decompression did not reach the expected image/audio signatures")
    if hashlib.sha256(source.read_bytes()).hexdigest() != SOURCE_SHA:
        raise SystemExit("Source changed during extraction")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result[0])
    print(json.dumps(dict(source_sha256=SOURCE_SHA, decoded_image_sha256=IMAGE_SHA,
                          audio_sha256=MUSIC_SHA, audio_bytes=len(result[0]), originals_unchanged=True), indent=2))


if __name__ == "__main__":
    main()
