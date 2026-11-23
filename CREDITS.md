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
chip emulation for the new DOS audio backend. Its source, pinned
revision information and LGPL 2.1-or-later license are included in
[`third_party/nuked-opl3`](third_party/nuked-opl3).
A small blues-engine extension adds per-channel output gains for independent
music/effect volumes; operators and the original register stream are preserved.

SDL2 and libmodplug are external build dependencies and retain their own
licenses. The compiled SDL2 DLL and other build products are not committed.

## Audited engines and tools for the expansion

The expansion selects [OpenTitus](https://github.com/stople/OpenTitus) (Eirik
Stople and contributors, GPL-3.0-or-later) for Fox DOS and
[Olduvai](https://github.com/ksokolowski/olduvai) (Krzysztof Sokołowski,
GPL-3.0-or-later source) for Prehistorik DOS. They have been tested externally;
their sources, binaries and bundled game resources have not been imported.
Olduvai's notices distinguish its source license from the GPL-3.0 combined
binary because of xBRZ. Their dependency notices must be retained if integrated.

[Deark](https://github.com/jsummers/deark), by Jason Summers (MIT-style license),
was used externally to unpack the user's DIET-compressed executable for audit.
[amitools](https://github.com/cnvogelg/amitools), by Christian Vogelgsang and
contributors (GPL-2.0-or-later), was used externally for read-only ADF comparison.
Neither is bundled. Unicorn 2.1.4 is an external preparation tool for the
verified Fox COM decompressor; it is not a game runtime dependency.
Chocolate Doom 1.6.0 OPL (Simon Howard and contributors, GPL-2.0-or-later),
with OpenTitus DBOPL (DOSBox Team, GPL-2.0-or-later), is rebuilt externally for
Fox. SDL 1.2 compatibility and SDL_mixer reference SDKs also remain external.
The two patches under `patches/` retain GPL-3.0-or-later for their respective
external engine changes; they do not assign a license to the inherited base.
Our audit scripts contain no original game data or copies
of these tools. Pinned revisions, preparation and distribution constraints are
recorded in [the audit](docs/ENGINE_AUDIT.md) and
[identification rules](docs/GAME_DETECTION_RULES.md). Builds and reference
validation are described in [TH-02](docs/ENGINE_REFERENCE_TESTS.md).

## Catalog core dependency

[nlohmann/json 3.11.3](https://github.com/nlohmann/json/tree/v3.11.3), by Niels
Lohmann and contributors, is bundled for the C++17 catalog core under the
[MIT license](third_party/nlohmann-json/LICENSE.MIT). Its pinned header and
provenance are under `third_party/nlohmann-json`. The core adds no SDL or
original game resources; SHA-256 and catalog/detection code are part of this
extension. The inherited game implementation remains credited above.

## Global library frontend

The SDL2 library frontend uses [SDL_ttf 2.22.0](https://github.com/libsdl-org/SDL_ttf),
by Sam Lantinga and contributors (zlib license), with FreeType and HarfBuzz.
Portions of this software are copyright © 2013 The FreeType Project
(www.freetype.org). All rights reserved. Dependency SDKs are external and their
notices must accompany binary distribution.

[DejaVu Sans 2.37](https://dejavu-fonts.github.io/) is bundled unchanged for
library text, with the [Bitstream Vera license and DejaVu public-domain notices](assets/fonts/LICENSE-DejaVu.txt).
Its source/checksum are recorded in [assets/fonts/README.md](assets/fonts/README.md).
No original game fonts or artwork are used by the frontend. Native game HUDs
retain their resource sets and authorship. See the [frontend guide](docs/LIBRARY_FRONTEND.md)
for pinned dependencies and validation.

The managed Blues runtime in `engines/blues-family/` calls the same inherited
Blues code and the extension above; it is not a replacement implementation of
the original game. Its process/profile bridge is documented in
[BLUES_RUNTIME.md](docs/BLUES_RUNTIME.md). Original game resources remain external.

## License information

The inherited upstream snapshot does not include a top-level license file.
This extension does not assign a new blanket license to that inherited code.
Original authorship and history are preserved, and the bundled Nuked OPL3
sources retain their explicit license and notices.
