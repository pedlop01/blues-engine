# Native Titus the Fox Amiga runtime

The native C++ simulation, Amiga resource readers, PAT/Paula audio player and
managed SDL2 adapter in this directory and platform/fox_amiga* are original
work for pedlop01/blues-engine, under GPL-3.0-or-later (see licenses/GPL-3.0.txt).
The native runtime does not link OpenTitus, Olduvai or an Amiga CPU emulator.

Titus developed the original game. Artwork, levels, music, samples and original
executables remain with their rights holders and are supplied by the user.
No commercial game data or extracted executable tables are distributed here.
The runtime reads a recognized, unchanged installation and extracts the bounded
rule tables it needs in memory. Research uses isolated original routines; its
Unicorn/amitools tools and private snapshots are not runtime dependencies.

SDL2 is dynamically linked under the zlib license. nlohmann/json is MIT licensed;
its notice is installed under share/licenses/titus-engine/nlohmann-json.
The surrounding library retains the original cyxx/blues attribution and licenses;
this notice does not assign a new blanket license to inherited code.

Corresponding sources and build recipes are in:
https://github.com/pedlop01/blues-engine
Final public release/source packaging remains task 18.
