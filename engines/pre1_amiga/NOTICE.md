# Native Prehistorik Amiga runtime

The isolated simulation fork in logic/ derives from Olduvai by Krzysztof
Sokolowski, GPL-3.0-or-later, pinned revision
`a69e9204fc311c8810d696a6b799000d1bcd1642`. See logic/UPSTREAM.json and
https://github.com/ksokolowski/olduvai (upstream URL is recorded in UPSTREAM.json).
The Amiga adaptation, bounded Hunk/DAT/DUR/MAT/PC1 readers, PAT/Paula audio,
renderer and managed SDL adapter are GPL-3.0-or-later as well.

Titus owns the original game; all artwork, levels, music, samples and original
executables are supplied by the user. No original resource or extracted table
is distributed. The native runtime reads tables from a recognized Hunk without
executing 68k code. Research validators and their private snapshots are not
runtime dependencies. The DOS Olduvai adapter remains separate.

SDL2 is dynamically linked under the zlib license. nlohmann/json is MIT licensed
(see the package's share/licenses/titus-engine directory). Original cyxx/blues
attribution remains in the surrounding project.

Corresponding sources and build recipes:
https://github.com/pedlop01/blues-engine
Public source/release packaging remains task 18.
