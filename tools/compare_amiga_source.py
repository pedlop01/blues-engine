#!/usr/bin/env python3
"""Compare an extracted folder with an ADF, using an external amitools checkout.

Read-only audit; no image contents or extracted resources are written to disk.
Folder files, including subdirectories, are compared; extra disk files are allowed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("folder", type=Path)
    parser.add_argument("--amitools", required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.amitools.resolve()))
    from amitools.fs.blkdev.ADFBlockDevice import ADFBlockDevice
    from amitools.fs.ADFSVolume import ADFSVolume

    image_sha = hashlib.sha256(args.image.read_bytes()).hexdigest()
    block = ADFBlockDevice(str(args.image), read_only=True)
    block.open()
    volume = ADFSVolume(block)
    matched, different, absent = [], [], []
    try:
        volume.open()
        nodes = {}

        def visit(directory, prefix=""):
            for node in directory.get_entries():
                name = prefix + str(node.name.get_name())
                if node.is_dir():
                    visit(node, name + "/")
                elif node.is_file():
                    key = name.casefold()
                    if key in nodes:
                        raise ValueError(f"Ambiguous case-insensitive image path: {name}")
                    nodes[key] = node

        visit(volume.root_dir)
        for path in sorted(args.folder.rglob("*")):
            if not path.is_file() or path.name.endswith(":Zone.Identifier"):
                continue
            name = path.relative_to(args.folder).as_posix()
            node = nodes.get(name.casefold())
            if node is None or not node.is_file():
                absent.append(name)
            elif node.get_file_data() == path.read_bytes():
                matched.append(name)
            else:
                different.append(name)
        report = dict(volume=str(volume.name), image_sha256=image_sha,
                      matched_count=len(matched), matched=matched, different=different, absent=absent)
    finally:
        volume.close()
        block.close()
    if hashlib.sha256(args.image.read_bytes()).hexdigest() != image_sha:
        raise ValueError("Image signature changed during audit")
    report["image_unchanged"] = True
    print(json.dumps(report, indent=2))
    return bool(different or absent or not matched)


if __name__ == "__main__":
    sys.exit(main())
