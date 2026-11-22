
# Blues Engine — extended edition

**An extension of [cyxx/blues](https://github.com/cyxx/blues), the engine
reimplementation by [Gregory Montoir (cyxx)](https://github.com/cyxx) and its
contributors.** The original engine and reverse-engineering work are theirs;
this repository preserves the upstream history and adds a new set of features
focused on the first **Blues Brothers** game.

[Original project](https://github.com/cyxx/blues) ·
[This extension](https://github.com/pedlop01/blues-engine) ·
[Credits and lineage](CREDITS.md) ·
[Plan y seguimiento](PLAN.md)

## What this extension adds

| Feature | Current implementation |
|---|---|
| Native Windows x64 | Build from WSL with MinGW-w64, SDL2 and libmodplug. |
| Smoother motion | Camera, characters and objects presented at 30, 60 or 120 Hz while gameplay simulation remains at 30 Hz. |
| Dynamic framing | `-` reveals more of the map, `+` moves closer, and `0` restores the initial view; changes ease over about 300 ms. |
| Original DOS audio | Native playback of four AdLib songs and 18 effects, read from the user's supported `1.EXE`, with Nuked OPL3 chip emulation. |
| Live DOS / Amiga switch | `F6` changes graphics, palettes, HUD and native audio while retaining game state, map edits, rooms and framing. |
| Larger views | Dynamic sprite storage replaces the fixed sprite limit; camera bounds and defeated-enemy visibility follow the current view. |
| Regression tests | Tests cover DOS sequencer accuracy, PCM continuity, zoom, doors, defeated enemies and live presentation changes. |

Audio output is 44.1 kHz. Technical tests pass, and the Windows DOS audio
adjustment has been confirmed in gameplay.

## Ideas and roadmap

The next phase includes a centered in-game options menu, configurable controls
and volumes, invulnerability and life controls, level selection, eight save
slots with thumbnails, and a short rewind with a VHS-style visual effect.
Further work includes camera/HUD polish and Windows packaging.

**These are planned features, not features already available.** Progress,
dependencies and completion criteria are tracked in [PLAN.md](PLAN.md).

## What comes from upstream

The upstream engine already reimplements Blues Brothers, Blues Brothers:
Jukebox Adventure and Prehistorik 2. Its original improvements over the DOS
games include horizontal scrolling, configurable viewport size and game cheats.
The enhancements described above focus on the first Blues Brothers; the native
Windows build currently includes that engine only.

This extension starts from
[`d4b94bb`](https://github.com/cyxx/blues/commit/d4b94bbc15d04273b3aa7d04f320c9a60c5c1e0e).
See [CREDITS.md](CREDITS.md) for attribution and license information. Original
game data is supplied separately by the user and is not included here.

Screenshots inherited from the original project:

![Blues Brothers — upstream screenshot](blues1.png)
![Jukebox Adventure — upstream screenshot](bbja2.png)

## Requirements

### Blues Brothers

The data files of the Amiga or DOS version, full game or [demo](https://archive.org/details/TheBluesBrothers_1020), are required.

```
*.BIN, *.CK1, *.CK2, *.SQL, *.SQV, *.SQZ
```

For sounds and music, the Amiga version files or the [ExoticA](https://www.exotica.org.uk/wiki/The_Blues_Brothers) set need to be copied.

### Jukebox Adventure

The data files of the DOS version are required.

```
*.EAT, *.MOD
```

### Prehistorik 2

The data files of the DOS version, full game or [demo](http://cd.textfiles.com/ccbcurrsh1/demos/pre2.zip), are required.

```
*.SQZ, *.TRK
```

## Inherited engine features

Compared to the original DOS executables, the rewritten engines feature:

- horizontal scrolling
- configurable screen size
- game cheats: unlimited lifes and energy, no hit


## Running

By default, the executable loads the data files from the current directory.
This can be changed using command line switches.

```
Usage: blues [OPTIONS]...
  --datapath=PATH   Path to data files (default '.')
  --dos-datapath=PATH    DOS files for live mode switching
  --amiga-datapath=PATH  Amiga files for live mode switching
  --level=NUM       Start at level NUM
  --cheats=MASK     Cheats bitmask
  --startpos=XxY    Start at position (X,Y)
  --fullscreen      Enable fullscreen
  --scale=N         Graphics scaling factor (default 2)
  --filter=NAME     Graphics scaling filter (default 'nearest')
  --screensize=WxH  Graphics screen size (default 320x200)
  --scrollrate=HZ   Scroll presentation rate: 30, 60, or 120 (default 60)
  --sound=MODE      Blues Brothers audio: auto, dos, or amiga (default auto)
  --cga             Enable CGA colors
  --dosscroll       Enable DOS style screen scrolling
  --hybrid          Enable fuchsia color as in Hybrid crack
```

During Blues Brothers gameplay, `-` moves the camera farther out to reveal more
of the actual map, `+` moves it closer again, and `0` restores the initial
framing. There are eight steps out, up to three times the initial width/height,
limited by the level boundaries. The window size stays fixed; the larger map
area is fitted into it, so characters and tiles become smaller. The camera
keeps the player at the same relative position where map boundaries allow it.
With the default settings, the logical view ranges from 320x200 to 960x600.
Menus keep their original resolution; the chosen framing is reapplied when
entering the next level. `--screensize` sets the initial framing.
Changes of framing ease in and out over approximately 300 ms. Pressing another
zoom key, including reset, retargets the transition from the current view.
Bonus rooms use their original fixed 320x200 framing because they are separate
scenes stored below the main map. Leaving a room restores the selected outdoor
framing. Door transitions cancel any active zoom/scroll interpolation.

`--scrollrate=60` (default) keeps the game simulation at 30 Hz and presents camera, character and object motion at 60 Hz. On a 120 Hz display, each presented frame can be shown for exactly two refreshes; `--scrollrate=120` targets every refresh.

Actor interpolation tracks object slots independently of drawing order and uses
the center of their feet as an anchor when animation dimensions change. Doors,
resource switches and zoom changes discard interpolation across incompatible
views. Physics, collisions and animation timing retain their original 30 Hz
updates. Use `--scrollrate=30` for the original presentation cadence.

The renderer placement regression test needs no game assets:

```
make build/tests/check_sprite_interpolation
build/tests/check_sprite_interpolation
```

### Blues Brothers audio

`--sound=dos` plays the original DOS AdLib music and effects through a software
OPL chip. Keep your original `1.EXE` alongside the DOS data files. The supported
layout is the 1991 DOS release with a 0x2200-byte MZ header and data segment at
file offset 0x11e00, including the 196608-byte executable used for this port.
Other releases, compressed executables, and the demo are not yet supported by
this audio backend; an explicit DOS selection reports an error if the executable
is missing or its layout is unrecognized. The executable is read as a resource;
its machine code is not run by the game.

`--sound=amiga` uses the existing Amiga/ExoticA module and sample playback. It
needs the files listed above in the selected data directory. `--sound=auto`
(default) selects Amiga audio for Amiga graphics data and DOS audio for DOS data;
if the DOS executable is unavailable or unsupported it warns and tries the
Amiga audio files instead. This option applies to the first Blues Brothers game.

Example from the repository directory on Linux/WSL:

```
./blues --datapath=../orig-games/dos --sound=dos
```

The native DOS sequencer preserves the original PIT clock/divisor, per-song
timing, instrument programming, and shared rhythm channel for sound effects.
Its chip emulation is [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3), running
in OPL2 compatibility mode; source and LGPL license are in
`third_party/nuked-opl3`.

For the regression test, install Python's `unicorn` package in a virtual
environment and run:

```
python3 tests/check_dos_audio.py ../orig-games/dos/1.EXE
```

This compares 12000 timer ticks per song, with all 18 effects, against the DOS
interrupt routine executed by Unicorn. It also checks PCM output and rejection
of missing/truncated or unsupported executable layouts. Original game assets
are supplied by the caller and are not included with the test.

SDL output uses 44.1 kHz mono signed 16-bit audio with a 4096-sample buffer
(about 93 ms). This preserves the scheduling margin of the older 22.05 kHz
output while reducing low-rate resampling artifacts from the OPL chip.
`python3 tests/check_audio_stream.py /path/to/1.EXE` checks bit-identical PCM
across arbitrary callback block sizes, including effects, and checks for clipping.

### Switch DOS / Amiga while playing

Supply both resource directories, then press **F6** during gameplay to switch
the complete presentation: graphics, sprites, HUD and native audio.

```
./blues --dos-datapath=../orig-games/dos --amiga-datapath=../orig-games/amiga
```

The game starts with DOS resources by default. Add
`--datapath=../orig-games/amiga` to start in Amiga mode instead. Position, lives,
health, inventory, enemies, edited map tiles, bonus rooms and zoom are retained.
The current music track restarts in the other version's arrangement. F6 selects
native audio for the new mode even if `--sound` selected the initial soundtrack.
Switching is available in levels; menus use the currently selected version.

The resource sets must be compatible full releases; missing files or different
level trigger definitions reject the switch before replacing the active data.
The two releases' untouched map tiles are loaded independently, with current
gameplay edits applied over them. If an Amiga extraction contains Concert's
trigger definitions as `concert.tbl` and omits its map, that level uses the
paired DOS map with Amiga graphics.

### Blues Brothers Windows x64 build from WSL

With MinGW-w64, the SDL2 MinGW development package, and a static MinGW build of libmodplug available:

```
make -f Makefile.win SDL2_ROOT=/path/to/x86_64-w64-mingw32 MODPLUG_ROOT=/path/to/libmodplug
```

This builds the Blues Brothers engine only, creates `build/windows-x64/blues.exe`, and copies `SDL2.dll` alongside it. Run the executable on Windows with `--datapath` pointing to the game data files.

### Linux / WSL build

Install a C compiler, GNU Make, SDL2 development files and libmodplug development
files. On Ubuntu/Debian:

```
sudo apt install build-essential libsdl2-dev libmodplug-dev
make -j4
```

Game data directories in the examples are siblings of this repository. They
are local user data, not part of a fresh clone. Choose any other paths with
`--datapath`, `--dos-datapath` and `--amiga-datapath`.

`make clean` removes Linux objects, dependency files, the Linux executable and
the viewport test executable. `make -f Makefile.win clean` removes the Windows
build products. Normal build output is ignored by Git.

### Dynamic viewport regression test (Linux/WSL)

```
make build/tests/check_viewport
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/tests/check_viewport --datapath=../orig-games/dos --sound=dos --cheats=7 --level=0
```

Repeat with `--level=1` through `--level=5` for all DOS levels. This runs the
real game loop and SDL renderer, exercises eased zoom-out, zoom-in, reset and
reversal midway through a transition, checks
all four map corners, and verifies that resizing preserves player world
coordinates, window size, and the menu resolution. Set `BLUES_VIEW_TEST_OUTPUT`
to an existing directory to save BMP captures of the initial, intermediate and
wide views.

Set `BLUES_TEST_DOORS=1` with the same test command to exercise every door in the
selected level at normal, wide and intermediate zoom sizes. It checks room
camera bounds, outdoor framing restoration, repeated door input in one frame,
and several gameplay updates inside each room without losing a life.

Set `BLUES_TEST_ENEMY_DEATH=1` to test crate hits in both directions at normal,
wide and intermediate viewport sizes. Defeated enemies retain the original
DOS upward impulse, gravity and sideways knockback, and remain visible until
their rising/falling trajectory leaves the current view, rather than a fixed
200-pixel screen.

Set `BLUES_TEST_MODES=1` with both `--dos-datapath` and `--amiga-datapath` to
exercise repeated live switches. The test checks retained game state and map
edits, room/zoom coordinates, actor rendering, PCM output from both soundtracks,
matching player sprite/atlas dimensions after each switch, and safe rejection
when the alternate resources are missing. It can start in
either mode and run against each of the six levels.
