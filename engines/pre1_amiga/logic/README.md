# Prehistorik Amiga simulation kernel

Selected pure C++ systems/types from Olduvai by Krzysztof Sokołowski (2026),
GPL-3.0-or-later, pinned revision a69e9204fc311c8810d696a6b799000d1bcd1642:
https://github.com/ksokolowski/olduvai
Original file hashes are recorded in UPSTREAM.json. Original copyright and
license declarations are retained. No renderer, SDL host, audio backend, DOS
executable reader implementation, assets or extracted tables are vendored here.

This is an isolated Amiga adaptation; it does not modify the DOS runtime or its
exported source tree. Changes are reviewed against isolated Amiga original
routines. The native Hunk/DAT/DUR readers supply level and actor data. The DOS
MonsterTables::from_exe loader is removed. Amiga-specific differences and
validation are documented in docs/PREHISTORIK_AMIGA_RUNTIME.md at repo root.
The prior DOS comments/function labels preserve provenance, not a claim that
those values or routines are authoritative for the Amiga edition.
