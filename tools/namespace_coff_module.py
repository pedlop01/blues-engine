#!/usr/bin/env python3
"""Namespace a linked game module (ELF or MinGW COFF), including COMDAT references.

COFF has no ELF hidden-symbol visibility. Localising .refptr COMDATs can break
references from other objects, while leaving private ones shared binds engines
to each other's globals. Rename both private symbols and their refptr sections;
leave undefined system/libc references and the game entry point shared.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("object", type=Path)
parser.add_argument("entry")
parser.add_argument("--nm", required=True)
parser.add_argument("--objcopy", required=True)
args = parser.parse_args()
result = subprocess.run([args.nm, "-g", "--defined-only", "-P", str(args.object)],
                        check=True, capture_output=True, text=True)
defined = {line.split()[0] for line in result.stdout.splitlines() if line.strip()}
if args.entry not in defined:
    raise SystemExit(f"Missing module entry point: {args.entry}")
private = {name for name in defined if name != args.entry and not name.startswith(".")}
prefix = args.entry + "_"
mapping = {name: prefix + name for name in private}
sections = []
for name in private:
    ref = ".refptr." + name
    if ref in defined:
        mapping[ref] = ".refptr." + prefix + name
        mapping[".rdata$" + ref] = ".rdata$.refptr." + prefix + name
        sections += ["--rename-section", f".rdata${ref}=.rdata$.refptr.{prefix}{name}"]
with tempfile.TemporaryDirectory(prefix="titus-coff-") as directory:
    symbols = Path(directory) / "symbols"
    symbols.write_text("".join(f"{old} {new}\n" for old, new in sorted(mapping.items())))
    subprocess.run([args.objcopy, "--redefine-syms=" + str(symbols), *sections, str(args.object)], check=True)
