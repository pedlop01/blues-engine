# Managed Olduvai runtime

Prehistorik DOS uses Olduvai by Krzysztof Sokołowski (2026):
https://github.com/ksokolowski/olduvai
Pinned revision: a69e9204fc311c8810d696a6b799000d1bcd1642 (0.9.11).
Olduvai source and our bridge/patches are GPL-3.0-or-later. Linked xBRZ is
GPL-3.0-only, making the combined binary GPL version 3. Preserve upstream
THIRD-PARTY-NOTICES and all source copyright declarations.

Nuked OPL3 by Nuke.YKT (2013–2020) is LGPL-2.1-or-later, conveyed with
Olduvai under GPL3 using LGPL section 3. stb is MIT/public domain. SDL2 is
linked dynamically under its zlib license. RtMidi, MT32emu and FluidSynth
bundled implementations are disabled in this build; baseline audio uses OPL.

The restricted DIET decoder adapts Deark modules/diet.c by Jason Summers
(2023), MIT license, revision e294a6e8348bd93f9a9305bb5f67d0042db4b791:
https://github.com/jsummers/deark
Its license accompanies this runtime. Deark is not required at runtime.

Sources are exported by tools/prepare_pre1_sources.py from the pinned checkout;
patches, bridge and CMake recipe are in this repository. The build export,
licenses and corresponding sources must accompany any redistribution. Final
release/source packaging remains task 18. No commercial game data is included.
No blanket license is assigned to inherited cyxx code. The frontend does not
link Olduvai; it supervises a separate native process.
