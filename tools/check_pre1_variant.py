#!/usr/bin/env python3
"""Reproduce the TH-01 PRE1 audit without modifying or executing DOS files.

External Deark/Olduvai binaries are supplied explicitly. Generated resources
stay in a private temporary directory; only signatures/diagnostics are printed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import os
import subprocess
import tempfile

PACKED_SHA = "b03ce6913d07a4c5b66dfc4843d185ffdff6561e9bf91f3333b096016690406f"
DECODED_SHA = "5cea262f9610657af8b55c10b277ec30ffe99df35294d5f8c9f520a30e029c96"
RESOURCE_SHA = {
    "FILESA.CUR": "eeb2e85ab2440ef91d54c808bd93fa6e6962b48d422beed1c06e32d684b962ec",
    "FILESB.CUR": "088806e36c4f836862989c5679a68c98b922cdac31c923d4e189cce37dcff098",
    "FILESA.VGA": "ec0b7a025ff1ef2391afc3675d3a4079f93b9df780d2bdf9715a1d2d5987b0fa",
    "FILESB.VGA": "6bfebd59c28e01878ce7575652a5e2a1f9fc0a9b6d3cf68039b4070ecb463d1a",
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resolve(directory, name):
    matches = [p for p in directory.iterdir() if p.is_file() and p.name.casefold() == name.casefold()]
    if len(matches) != 1:
        raise ValueError(f"Expected exactly one {name}; found {len(matches)}")
    return matches[0]


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, timeout=40, **kwargs)
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}): {command[0]}\n{result.stdout}\n{result.stderr}")
    return result.stdout + result.stderr


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data_dir", type=Path)
    parser.add_argument("--deark", required=True, type=Path)
    parser.add_argument("--olduvai", type=Path, help="Optionally check levels 1–7, 30 frames each (audio off)")
    args = parser.parse_args()
    directory = args.data_dir.resolve()
    original = resolve(directory, "PRE1.EXE")
    if digest(original) != PACKED_SHA:
        raise ValueError("Unrecognised PRE1.EXE: this audit recipe only accepts the verified variant")
    data = [resolve(directory, name) for name in RESOURCE_SHA]
    for path in data:
        if digest(path) != RESOURCE_SHA[path.name.upper()]:
            raise ValueError(f"Unrecognised resource variant: {path.name}")
    before = {p.name: digest(p) for p in [original, *data]}
    report = {"packed_sha256": PACKED_SHA, "levels": []}
    with tempfile.TemporaryDirectory(prefix="titus-pre1-audit-") as temporary:
        staging = Path(temporary)
        log = run([str(args.deark.resolve()), "-m", "diet", "-d", "-od", str(staging), str(original)])
        decoded = staging / "output.000.exe"
        if not decoded.is_file() or digest(decoded) != DECODED_SHA:
            raise ValueError(f"Unexpected decompression result\n{log}")
        if "crc (reported): 0x2f77" not in log or "crc (calculated): 0x2f77" not in log:
            raise ValueError(f"DIET checksum not confirmed\n{log}")
        report.update(decoded_sha256=DECODED_SHA, decoded_bytes=decoded.stat().st_size,
                      format="DIET EXE v1.02–1.20", crc16="2f77")
        (staging / "HISTORIK.EXE").symlink_to(decoded)
        for p in data:
            (staging / p.name.upper()).symlink_to(p)
        if args.olduvai:
            env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                       XDG_CONFIG_HOME=str(staging / "config"),
                       XDG_DATA_HOME=str(staging / "data"), XDG_CACHE_HOME=str(staging / "cache"))
            for level in range(1, 8):
                shot = staging / f"level{level}.bmp"
                log = run([str(args.olduvai.resolve()), "--game-dir", str(staging), "--play",
                           "--level", str(level), "--display-mode", "cpu", "--play-frames", "30",
                           "--play-shot", str(shot), "--sound-card", "off"], env=env, cwd=staging)
                errors = ("EXE tables not installed", "failed to parse game files", "could not load level data")
                if any(message in log for message in errors) or f"game: level {level} " not in log:
                    raise ValueError(f"Level {level} failed to load despite exit code 0\n{log}")
                if not shot.is_file() or shot.read_bytes()[:2] != b"BM":
                    raise ValueError(f"Level {level} did not produce the requested screenshot\n{log}")
                report["levels"].append({"level": level, "frames_requested": 30, "capture": True})
    if before != {p.name: digest(p) for p in [original, *data]}:
        raise ValueError("Original resource signatures changed during the audit")
    report["originals_unchanged"] = True
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.TimeoutExpired) as error:
        raise SystemExit(str(error))
