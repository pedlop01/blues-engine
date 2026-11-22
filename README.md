
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
| Camera choice | Classic following or smooth following with directional anticipation and a vertical dead zone; select with `--camera=classic` or `--camera=smooth` (default). |
| Dynamic framing | `-` reveals more of the map, `+` moves closer, and `0` restores the initial view; changes ease over about 300 ms. |
| Original DOS audio | Native playback of four AdLib songs and 18 effects, read from the user's supported `1.EXE`, with Nuked OPL3 chip emulation. |
| Live DOS / Amiga switch | `F6` changes graphics, palettes, HUD and native audio while retaining game state, map edits, rooms and framing. |
| HUD choice | Classic HUD scales with the map (default); fixed HUD keeps its display size and reserves separate bands without covering the map. `F7` switches live. Both preserve DOS/Amiga artwork and layout. |
| In-game pause menu | `Esc`, `P` or controller Start opens a centered menu with Resume, Options, Controls and confirmed Quit. Simulation, zoom and audio pause; menu inputs are consumed before returning to play. |
| Separate audio levels | Options → Sound provides independent 0–100% music/effect levels and mute, with short gain ramps in DOS and Amiga. |
| Gameplay assists | Options → Assists offers invulnerability, independent infinite lives and finite counts from 1–99 per active player. Active protections are labeled in the original HUD’s empty margin. |
| Larger views | Dynamic sprite storage replaces the fixed sprite limit; camera bounds and defeated-enemy visibility follow the current view. |
| Regression tests | Tests cover DOS sequencer accuracy, PCM continuity, zoom, doors, defeated enemies and live presentation changes. |

Audio output is 44.1 kHz. Technical tests pass, and the Windows DOS audio
adjustment has been confirmed in gameplay.

## Ideas and roadmap

The next phase adds persistent preferences,
level selection, eight save
slots with thumbnails, and a short rewind with a VHS-style visual effect.
Further work includes Windows packaging.

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
  --hud=MODE       Blues Brothers HUD: classic or fixed (default classic)
  --camera=MODE    Blues Brothers camera: classic or smooth (default smooth)
  --dosscroll       Select classic camera with DOS style screen scrolling
  --hybrid          Enable fuchsia color as in Hybrid crack
```

During Blues Brothers gameplay, `-` moves the camera farther out to reveal more
of the actual map, `+` moves it closer again, and `0` restores the initial
framing. There are eight steps out, up to three times the initial width/height,
limited by the level boundaries. The window size stays fixed; the larger map
area is fitted into it, so characters and tiles become smaller. The camera
keeps the player at the same relative position where map boundaries allow it.
With the default settings, the logical view ranges from 320x200 to 960x600.
The default `--hud=classic` retains the earlier presentation: HUD and world
scale together during zoom. `--hud=fixed` keeps the indicators at their native
display size and fits the complete world view between the HUD bands, without
cropping or stretching it. Fixed mode scales the world area independently of
the reserved bands, so zoom retains the map's aspect ratio. If level bounds or
custom view dimensions change that ratio, the world is fitted with margins.
Press **F7** in a level to alternate between the two modes, including while
zooming. The selected mode is retained across DOS/Amiga switches and levels;
selection is also available under Options in the pause menu.
Both modes preserve each release's bars, digits, portraits, icons and layout,
including two-player indicators.
Menus keep their original resolution; the chosen framing is reapplied when
entering the next level. `--screensize` sets the initial framing.
Changes of framing ease in and out over approximately 300 ms. Pressing another
zoom key, including reset, retargets the transition from the current view.
Bonus rooms use their original fixed 320x200 framing because they are separate
scenes stored below the main map. Leaving a room restores the selected outdoor
framing. Door transitions cancel any active zoom/scroll interpolation.

`--scrollrate=60` (default) keeps the game simulation at 30 Hz and presents camera, character and object motion at 60 Hz. On a 120 Hz display, each presented frame can be shown for exactly two refreshes; `--scrollrate=120` targets every refresh.

`--camera=smooth` follows gradually, leaves more room ahead while walking and
changes anticipation gently when turning. Stopping retains the current look-ahead;
a vertical dead zone avoids chasing small jumps. Bonus rooms keep their fixed
camera, and doors and viewport changes reset the following history.
`--camera=classic` restores the previous camera behavior. `--dosscroll` selects
the classic camera's DOS scrolling variant (a later `--camera` overrides the
camera selection). Both cameras retain the selected presentation frequency.
Options in the pause menu also exposes the classic/smooth selector.

Camera regression tests cover all six levels with the supplied assets:

```
make build/tests/check_viewport
BLUES_TEST_CAMERA=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=0
```

Repeat with `--level=1` through `--level=5` for the other levels.

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

### In-game pause menu

During a Blues Brothers level, release **Esc** or **P**, or controller **Start**,
to open the centered pause menu. Use Up/Down, the controller D-pad or left stick
to navigate, and Enter/Space or controller A to select. Esc/P/Start closes the
main pause menu; controller B goes back or resumes. Controls groups keyboard/controller configuration, a binding summary and
restore-defaults in one place. Quit opens a confirmation that defaults to cancel.
Closing the window still exits directly. Outside levels, Esc retains its
original exit behavior.

The menu freezes the current scene, simulation, camera/zoom transition and audio.
It redraws correctly after resizing the window and works with either DOS/Amiga
resources, HUD mode and presentation rate. Held menu controls must be released
before they can act in gameplay, preventing accidental movement, jumps or
resource/HUD switches on resume.

The menu retains a retro bitmap font with a dark shadow, gold selection stripe
and a separate color for the confirmed Quit action. The Controls page groups
actions and bindings in columns. It uses whole-pixel scaling when the output
fits at least 320x200, keeping glyph strokes uniform at fractional window sizes;
smaller outputs fit the dialog proportionally. Its size is independent of map
zoom, and it stays centered in windowed and fullscreen modes.

The menu foundation and visual polish (tasks 06–07) are validated. General
settings are implemented in task 08.
The existing F6/F7 and zoom shortcuts remain available during gameplay.

```
make build/tests/check_pause_menu build/tests/check_viewport
build/tests/check_pause_menu
BLUES_TEST_MENU=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=0
```

The asset-free test exercises menu navigation, quit confirmation, controller and
joystick events, focus changes, held input, audio pause/resume and centered
layout at several window sizes. The game integration test opens the actual
modal menu during zoom, spends time in its Controls page, and checks that game
state, objects, map and viewport remain unchanged before resuming.

### General options and controls

Open **Options** from the pause menu. Up/Down selects a row; Left/Right changes
its value, and Enter/A also cycles a setting. The page provides DOS/Amiga,
classic/smooth camera, classic/fixed HUD, window/fullscreen mode, 30/60/120 Hz
presentation and all nine framing positions (normal plus eight steps out).
DOS/Amiga selection requires both compatible resource directories; unavailable
switches remain disabled with an explanation.

Changes are prepared while the simulation and audio remain paused, then applied
when you return to **Resume**. Fullscreen toggles immediately for preview.
Changing resource mode retains the existing F6 behavior, including restarting
the current track in the other release's native arrangement. FPS changes affect
presentation; simulation still runs at 30 Hz.

**Controls** in the main pause menu offers keyboard and SDL game-controller configuration,
plus a restore-defaults action. Select an action and press Enter/A, then release
the new key or button. Esc cancels capture; controller Start also cancels button
capture. The six keyboard actions are movement in four directions, action and
jump; controller action/jump buttons can be reassigned, while movement keeps its
stick/D-pad controls. Assigning an already-used control swaps the two bindings.
Enter remains an additional gameplay action key, and right Shift aliases left
Shift when the default jump binding is used.

Menu navigation remains on arrows, Enter/Space, Esc/P and controller A/B/Start
regardless of gameplay bindings. Menu/presentation shortcuts and player-select
digits are reserved during keyboard capture. Controls shows the prepared
bindings, and held capture/confirmation inputs are consumed on resume.
Settings and bindings remain active through levels during this run; saving
preferences across launches is task 12.

```
BLUES_TEST_OPTIONS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=0
```

This exercises the real menu, prepares every setting, reassigns keyboard and
controller actions, checks that the world stays frozen in the menu, and verifies
that the settings are applied after resuming. Repeat across levels, initial
resource releases and HUD modes for the full matrix.

### Music and effect volumes

**Options → Sound** has independent Music and Effects levels. Left/Right changes
the selected level in 10% steps from 0 to 100; Enter/A toggles mute and restores
the last nonzero level. The default is 100% for both. Changes apply when you
resume, with the new gain targets set before audio restarts and a 20 ms ramp
to avoid abrupt level changes. Muting continues playback rather than restarting
songs or effects, and levels are retained across levels and DOS/Amiga switches.
Saving them between launches remains part of task 12.

Amiga module music and PCM effects are scaled separately before mixing. DOS
keeps the original single-chip sequencer and shared channel 6: its output is
assigned to music or effects according to the sequencer's actual writes. This
retains the original percussion/effect channel stealing. A small documented
Nuked OPL3 extension scales channel outputs after synthesis; 100% output is
bit-identical to the previous unmodified chip renderer for the reference EXE.

```
make build/tests/check_mixer_volumes
build/tests/check_mixer_volumes
python3 tests/check_audio_volumes.py ../orig-games/dos/1.EXE
BLUES_TEST_VOLUMES=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=0
```

These tests cover independent music/PCM gains, bounded ramps, exact mute,
continuity over arbitrary callback sizes, reference DOS PCM hashes, unchanged
synthesis progression while muted, real DOS/Amiga audio, menu changes and level
retention through resource changes. The existing DOS IRQ/register comparison
also remains unchanged.

### Gameplay assists

**Options → Assists** provides invulnerability, infinite lives, and finite life
counts for Player 1 and Player 2. Left/Right or Enter/A changes a setting;
changes apply when you resume. Counts range from **1 to 99**, including the
current life. Player 2 is disabled when not participating, and an already
eliminated player is not revived by changing these settings.

Invulnerability prevents damage from hits and hazardous tiles. It does not
heal previous damage or undo a death already in progress. Infinite lives is
independent: you can still lose health and retry, but lives are not consumed,
even when the finite counter is one. Your finite count is retained, including
while collecting life bonuses, so disabling infinity restores that count.
Finite life pickups cap at 99 to stay within the original two-digit HUD range.

The native DOS/Amiga HUD graphics remain intact. When enabled, **INVULNERABLE**
and/or **VIDAS INFINITAS** appears in its empty bottom margin. This indicator
follows the selected classic/fixed HUD scale. Settings last for this run; disk
persistence is still task 12. A new game starts with three lives and normal
health, including when using a nonzero `--level`.

```
BLUES_TEST_ASSISTS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=0 --level=0
BLUES_TEST_ASSIST_MENU=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=0 --level=0
```

The physics test covers ordinary/protected hits, hazardous tiles, independent
assists, repeated retries, final-life game over/retry, two-player depletion,
inactive-player handling and life-pickup saturation. The menu test toggles both
assists, changes finite counts and verifies paused state plus retained settings
after a DOS/Amiga switch. Add `BLUES_TEST_ASSIST_TWO=1` to test two active
players, or start with `--cheats=7` to verify disabling existing protections.
Repeat for levels 1–5 and either initial resource mode.

### Ladders and warehouse tube recovery

With the default separate jump control, Up/Down climbs ladders and Shift remains
the jump action. Releasing horizontal input stops walking momentum on a ladder,
and releasing vertical input stops climbing. Holding Down enters ladder caps
and continues through their one-way platform sections, without requiring a
new press at each platform. Horizontal wall probes no longer override a
player's requested climbing direction; enemy climbing behavior is retained.

The warehouse's instrument-gated tube uses the original scripted teleport and
camera recentering. Both camera modes now finish recentering at the bounded
target, including zoomed-out views, so the landing animation stops being
reapplied and control returns normally. Smooth camera positions retain
fractional progress instead of stalling short of the target.

```
BLUES_TEST_TRAVERSAL=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=1
```

This exercises the real warehouse ladders with incoming left/right momentum,
Up/Down/idle and both jump mappings, the ladder's platform crossing, and idle
recovery after the tube with both cameras, normal/wide framing and immunity
on/off. It checks the instrument gate and that movement works after recovery.
Repeat for levels 0–5 to exercise other long ladders, and for either resource
release and HUD mode.

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

### Native HUD regression test

```
BLUES_TEST_HUD=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --cheats=7 --level=0
```

The test compares the displayed HUD pixel-for-pixel with the original native
presentation in both resource modes, verifies all four corners of the world
remain visible between HUD bands, and covers one and two players, three window
sizes, all eight zoom steps and intermediate transition sizes. It also verifies
that the HUD layer is disabled before returning to menus. Repeat for levels
1 through 5 to cover the full set of level markers.

`BLUES_TEST_HUD_SWITCH=1` runs the real game loop with F7 presses during zoom
and checks both directions. Add `--hud=fixed` to test the opposite starting mode.
