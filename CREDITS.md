# Credits and project lineage

## Original engine

This repository extends **[blues](https://github.com/cyxx/blues)** by
**[Gregory Montoir (cyxx)](https://github.com/cyxx)** and its contributors.
The original engine reimplements Blues Brothers, Blues Brothers: Jukebox
Adventure and Prehistorik 2, with SDL2 and PSP backends.

The engine architecture, reverse engineering, original game logic, resource
decoders and inherited platform support come from that project. This extension
builds on that work; it does not claim authorship of the original engine.

- Upstream repository: <https://github.com/cyxx/blues>
- Starting revision:
  [`d4b94bbc15d04273b3aa7d04f320c9a60c5c1e0e`](https://github.com/cyxx/blues/commit/d4b94bbc15d04273b3aa7d04f320c9a60c5c1e0e)
- Original README at that revision:
  [upstream documentation](https://github.com/cyxx/blues/blob/d4b94bbc15d04273b3aa7d04f320c9a60c5c1e0e/README.md)
- The upstream Git history and its author information are retained.
- The existing screenshots in the repository were inherited from upstream.

## This extension

Maintained by [pedlop01](https://github.com/pedlop01) at
[pedlop01/blues-engine](https://github.com/pedlop01/blues-engine).

The current work focuses on the first Blues Brothers game: a native Windows
build, smoother camera presentation, dynamic framing, DOS AdLib music/effects,
live DOS/Amiga presentation switching and regression tests. See
[README.md](README.md) for implemented features and [PLAN.md](PLAN.md) for the
planned in-game menu, saves, rewind and other improvements.

## Original games and data

The original games were developed by Titus. Game artwork, music, names and
other original assets remain the property of their respective rights holders.
Users supply their own game data; this repository does not bundle DOS/Amiga
game data or the original executables.

## Bundled dependency

**[Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3)** by Nuke.YKT provides OPL
chip emulation for the new DOS audio backend. Its unmodified source, pinned
revision information and LGPL 2.1-or-later license are included in
[`third_party/nuked-opl3`](third_party/nuked-opl3).

SDL2 and libmodplug are external build dependencies and retain their own
licenses. The compiled SDL2 DLL and other build products are not committed.

## License information

The inherited upstream snapshot does not include a top-level license file.
This extension does not assign a new blanket license to that inherited code.
Original authorship and history are preserved, and the bundled Nuked OPL3
sources retain their explicit license and notices.
