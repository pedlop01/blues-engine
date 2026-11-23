#!/usr/bin/env python3
"""Run a bounded Windows reference check from WSL with private preferences.

Use `--program EXE --output-dir build/... [--cwd DIR] [--env KEY=VALUE] -- ARGS`.
Absolute Linux paths in arguments/environment values are translated with wslpath.
This is a development utility, not the future cross-platform game launcher.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def windows_path(path):
    return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()


def translate(value):
    if value.startswith("/"):
        return windows_path(Path(value).resolve())
    if "=" in value:
        key, content = value.split("=", 1)
        if content.startswith("/"):
            return key + "=" + windows_path(Path(content).resolve())
    return value


def quote(value):
    if any(character in value for character in ('"', '\r', '\n')):
        raise ValueError("Invalid batch argument")
    return '"' + value.replace("%", "%%") + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--program", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--cwd", type=Path)
    parser.add_argument("--env", action="append", default=[])
    parser.add_argument("--timeout", default=35, type=int)
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if not output.is_relative_to(ROOT / "build"):
        raise SystemExit("Output must be inside the ignored project build/ tree")
    output.mkdir(parents=True, exist_ok=True)
    environment = dict(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                       BLUES_CONFIG_FILE="", BLUES_SAVE_DIR="",
                       XDG_CONFIG_HOME=windows_path(output / "config"),
                       XDG_CACHE_HOME=windows_path(output / "cache"),
                       XDG_DATA_HOME=windows_path(output / "data"))
    for pair in args.env:
        key, value = pair.split("=", 1)
        if not key.replace("_", "").isalnum():
            raise ValueError("Invalid environment key")
        environment[key] = translate(value)
    arguments = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    command = " ".join(quote(value) for value in [windows_path(args.program.resolve()), *map(translate, arguments)])
    lines = ["@echo off", "setlocal", *["set " + quote(key + "=" + value) for key, value in environment.items()],
             "pushd " + quote(windows_path((args.cwd or args.program.parent).resolve())),
             "if errorlevel 1 exit /b 2", command,
             'set "probe_status=%errorlevel%"', "popd", "exit /b %probe_status%"]
    with tempfile.TemporaryDirectory(prefix="windows-probe-", dir=output) as directory:
        batch = Path(directory) / "run.cmd"
        batch.write_bytes(("\r\n".join(lines) + "\r\n").encode("utf-8"))
        result = subprocess.run(["/mnt/c/Windows/System32/cmd.exe", "/d", "/c", windows_path(batch)],
                                cwd="/mnt/c", capture_output=True, timeout=args.timeout)
    stdout, stderr = result.stdout.decode(errors="replace"), result.stderr.decode(errors="replace")
    print(json.dumps(dict(program=args.program.name, exit=result.returncode, stdout=stdout, stderr=stderr), indent=2))
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
