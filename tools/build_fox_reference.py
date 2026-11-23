#!/usr/bin/env python3
"""Build isolated Fox reference binaries from pinned external source checkouts.

Does not download or bundle game data. SDKs are supplied explicitly; copied
sources/build products belong in an ignored build directory, never orig-games.
"""
import argparse
from pathlib import Path
import json
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
REVISIONS = {"opentitus": "6540e8f2867f99aff54ad1a710ae6379ecb9c590",
             "chocolate": "822664b4ff873d462370e9e96a9d91e6066c221d"}


def run(command, **kwargs):
    subprocess.run([str(x) for x in command], check=True, **kwargs)


def export_source(checkout, destination, revision, paths):
    actual = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if actual != revision:
        raise SystemExit(f"Wrong source revision for {checkout}: {actual}")
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / "source.tar"
    run(["git", "-C", checkout, "archive", "--output", archive, revision, *paths])
    with tarfile.open(archive) as stream:
        stream.extractall(destination, filter="data")
    archive.unlink()


def library_dir(prefix, name):
    candidates = [prefix / "lib", prefix / "lib64", *sorted((prefix / "lib").glob("*"))]
    for directory in candidates:
        if (directory / f"lib{name}.so").exists() or (directory / f"lib{name}.dll.a").exists():
            return directory
    raise SystemExit(f"Missing development library {name} under {prefix}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--opentitus", required=True, type=Path)
    parser.add_argument("--chocolate", required=True, type=Path)
    parser.add_argument("--sdl-prefix", required=True, type=Path)
    parser.add_argument("--mixer-prefix", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--windows", action="store_true")
    parser.add_argument("--sdl2-dll", type=Path)
    parser.add_argument("--asan", action="store_true", help="Native AddressSanitizer reference build")
    args = parser.parse_args()
    output = args.output.resolve()
    # Reference sources/data are deliberately kept under the project's ignored build tree.
    if not output.is_relative_to(ROOT / "build"):
        raise SystemExit("Choose an output directory inside the ignored project build/ tree")
    if args.windows and (not args.sdl2_dll or args.asan):
        raise SystemExit("Windows requires --sdl2-dll; --asan is a native-only option")
    sdl = args.sdl_prefix.resolve()
    mixer = (args.mixer_prefix or args.sdl_prefix).resolve()
    source, chocolate = output / "source", output / "chocolate-source"
    export_source(args.opentitus.resolve(), source, REVISIONS["opentitus"], ["opentitus.c", "include", "src", "opl", "gpl3.txt", "README"])
    export_source(args.chocolate.resolve(), chocolate, REVISIONS["chocolate"], ["opl", "COPYING"])
    # Do not retain the upstream binary OPL archives; all OPL code is rebuilt.
    for archive in (source / "opl").glob("*.a"):
        archive.unlink()
    run(["patch", "--batch", "-p1", "-i", ROOT / "patches/opentitus-reference.patch"], cwd=source)
    opl = chocolate / "opl"
    (opl / "config.h").write_text("/* Software SDL OPL only. */\n")
    path = opl / "opl.c"
    path.write_text(path.read_text().replace("#ifdef _WIN32\n", "#ifdef TITUS_ENABLE_HARDWARE_OPL\n"))
    sdl_lib, mixer_lib = library_dir(sdl, "SDL"), library_dir(mixer, "SDL_mixer")
    flags = ["-O2", "-g", "-fcommon", "-fsigned-char"]
    if args.asan:
        flags += ["-fsanitize=address", "-fno-omit-frame-pointer"]
    includes = [f"-I{p}" for p in (source, source / "include", source / "opl", opl,
                                  sdl / "include", sdl / "include/SDL", mixer / "include", mixer / "include/SDL")]
    objects = []
    for path in [opl / f"{stem}.c" for stem in ("opl", "opl_queue", "opl_sdl", "opl_timer")] + [source / "opl/dbopl.c"]:
        target = output / (path.stem + ".o")
        run([args.cc, "-std=gnu99", *flags, *includes, "-c", path, "-o", target])
        objects.append(target)
    libraries = [f"-L{sdl_lib}", f"-L{mixer_lib}"]
    if args.windows:
        libraries += ["-lmingw32", "-lSDLmain", "-static-libgcc"]
    else:
        libraries += [f"-Wl,-rpath,{sdl_lib}", f"-Wl,-rpath,{mixer_lib}", "-Wl,--disable-new-dtags"]
    libraries += ["-lSDL", "-lSDL_mixer", "-lm"]
    extension = ".exe" if args.windows else ""
    sources = sorted((source / "src").glob("*.c"))
    for name, main_source, wrappers in [
        ("opentitus-reference", source / "opentitus.c", []),
        ("fox-gameplay", ROOT / "tests/reference_fox_probe.c", ["move_player", "SDL_GetKeyState", "viewstatus", "Mix_HookMusic", "SDL_Flip"]),
    ]:
        run([args.cc, "-std=gnu89", *flags, *includes, main_source, *sources, *objects,
             *libraries, *["-Wl,--wrap=" + function for function in wrappers], "-o", output / (name + extension)])
    if args.windows:
        for path in [sdl / "bin/SDL.dll", mixer / "bin/SDL_mixer.dll", args.sdl2_dll.resolve()]:
            shutil.copyfile(path, output / path.name)
    (output / "build-info.json").write_text(json.dumps(dict(revisions=REVISIONS, windows=args.windows,
                                                          compiler=args.cc, asan=args.asan), indent=2) + "\n")
    print(f"Reference binaries built in {output}; game data/configuration must be supplied separately.")


if __name__ == "__main__":
    main()
