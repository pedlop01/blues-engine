# Managed OpenTitus runtime

OpenTitus, Copyright (C) 2008–2011 The OpenTitus team, including Eirik Stople,
reimplements Titus the Fox / Moktar. Upstream:
https://github.com/stople/OpenTitus
Revision: `6540e8f2867f99aff54ad1a710ae6379ecb9c590`.
License: GNU GPL version 3 or later. Its full text is copied into the build package.

The OPL interface is from Chocolate Doom 1.6.0,
revision `822664b4ff873d462370e9e96a9d91e6066c221d`:
https://github.com/chocolate-doom/chocolate-doom
Copyright notices remain in the exported sources; GNU GPL version 2 or later.
The software DBOPL from OpenTitus retains the DOSBox Team copyright (2002–2010)
and GPL version 2 or later, including OpenTitus's percussion changes.
Hardware OPL backends and precompiled archives are excluded from this build.

SDL 1.2 compatibility: libsdl-org/sdl12-compat release-1.2.68,
revision `35ef746cf1a4fde8c442dc92fe9b18c3f90d7855`, zlib license:
https://github.com/libsdl-org/sdl12-compat
SDL_mixer 1.2.12, zlib license; full notices accompany this directory.
SDL2 remains dynamically linked, with the same version as the frontend package.

The bootstrap, runtime and managed source patch here are GPL-3.0-or-later.
They are modified versions/adaptations, not unmodified upstream releases.
The frontend does not link this runtime's engine, SDL 1.2 or OPL code.
No new blanket license is assigned to inherited cyxx/blues code.

Sources are exported from pinned checkouts with `tools/prepare_fox_sources.py`.
The source exports, GPL texts, revisions, source patches, CMake recipe and bridge
are required inputs to reproduce this local development binary. Keep them when
preparing a redistribution/source bundle in task 18; this is not a final release
package or an offer to ship binaries without corresponding sources/notices.
No commercial OpenTitus/bin assets, music.bin or original game data are included.
Audio is prepared only from the user's verified FOX.COM, into a private profile.
