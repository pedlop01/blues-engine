
# Titus Engine — expanding Blues Engine

**An extension of [cyxx/blues](https://github.com/cyxx/blues), the engine
reimplementation by [Gregory Montoir (cyxx)](https://github.com/cyxx) and its
contributors.** The original engine and reverse-engineering work are theirs;
this repository preserves the upstream history and adds a new set of features
focused on the first **Blues Brothers** game.

**Titus Engine now has an SDL2 game library, persistent catalog and detection CLI.** The
current Windows executable still runs Blues Brothers; the global library and new
engine adapters are tracked in the expansion plan. Blues playtesting continues
in parallel, and final Windows packaging follows the validated expansion. The
existing repository, executable names and save paths remain in place during
this transition.

[Original project](https://github.com/cyxx/blues) ·
[This extension](https://github.com/pedlop01/blues-engine) ·
[Credits and lineage](CREDITS.md) ·
[Plan y seguimiento](PLAN.md) ·
[Titus Engine: expansion plan](docs/TITUS_PLATFORM_PLAN.md) ·
[Local game inventory](docs/GAME_INVENTORY.md) ·
[Engine audit and startup checks](docs/ENGINE_AUDIT.md) ·
[Game detection rules](docs/GAME_DETECTION_RULES.md) ·
[Reference engine validation](docs/ENGINE_REFERENCE_TESTS.md) ·
[Architecture and v1 contracts](docs/TITUS_ARCHITECTURE.md) ·
[Catalog core: build, commands and tests](docs/CATALOG_CORE.md) ·
[Global library: build, controls and tests](docs/LIBRARY_FRONTEND.md)

The independent `titus-catalog` tool recognizes the eight audited DOS/Amiga
resource sets, links known ADF sources and manages installation references without
changing game files. It manages the catalog; the graphical frontend launches games. Build and test it with
`make check-catalog`; [TH-04 documentation](docs/CATALOG_CORE.md) covers optional
read-only checks against user-supplied games and the Windows build.

The new `titus-engine` frontend lists installed games and DOS/Amiga releases,
remembers per-game selections and provides a folder/ADF browser, verification
and installation management. Keyboard, mouse, controller and fullscreen are
supported. **Blues Brothers DOS/Amiga now launch from the library**, with a clean return
and initial reversible import of classic preferences and saves. Other engines
remain scheduled for TH-07–09. See the [Blues runtime guide](docs/BLUES_RUNTIME.md). Build it with
`-DTITUS_BUILD_LAUNCHER=ON` and SDL2/SDL2_ttf/libmodplug development files; see the
[library guide](docs/LIBRARY_FRONTEND.md). The classic game executable still works.

## What this extension adds

| Feature | Current implementation |
|---|---|
| Native Windows x64 | Build from WSL with MinGW-w64, SDL2 and libmodplug. |
| Smoother motion | Camera, characters and objects presented at 30, 60 or 120 Hz while gameplay simulation remains at 30 Hz. |
| Camera choice | Classic following or smooth following with directional anticipation and a vertical dead zone; select with `--camera=classic` or `--camera=smooth` (default). |
| Native Amiga layering | Original `.m` masks put scenery in front of actors while preserving holes and opaque black. Copper gradients fill the whole playfield; native props retain their four color planes and level palette. |
| Stable scrolling edges | A guarded map background covers intermediate camera positions; Amiga copper gradients stay fixed on screen while scenery scrolls, including at 1×/60 Hz. |
| Dynamic framing | `-` reveals more of the map, `+` moves closer, and `0` restores the initial view; changes ease over about 300 ms. |
| Original DOS audio | Native playback of four AdLib songs and 18 effects, read from the user's supported `1.EXE`, with Nuked OPL3 chip emulation. |
| Live DOS / Amiga switch | `F6` changes graphics, palettes, HUD and native audio while retaining game state, map edits, rooms and framing. |
| HUD choice | Classic HUD scales with the map (default); fixed HUD keeps its display size and reserves separate bands without covering the map. `F7` switches live. Both preserve DOS/Amiga artwork and layout. |
| In-game pause menu | `Esc`, `P` or controller Start opens a centered menu with Resume, Options, Controls, Level Selection, Character Selection and confirmed Quit. Simulation, zoom and audio pause; menu inputs are consumed before returning to play. |
| Separate audio levels | Options → Sound provides independent 0–100% music/effect levels and mute, with short gain ramps in DOS and Amiga. |
| Gameplay assists | Options → Assists offers invulnerability, independent infinite lives and finite counts from 1–99 per active player. Active protections are labeled in the original HUD’s empty margin. |
| Level selection | Six stages from the pause menu, with confirmation, current-stage restart and unavailable-data detection. Starts fresh while retaining characters, life counts, assists and presentation settings. |
| Larger views | Dynamic sprite storage replaces the fixed sprite limit; camera bounds and defeated-enemy visibility follow the current view. |
| Persistent preferences | Presentation, volumes, controls, preferred DOS/Amiga release, assists and chosen starting lives survive restarts. Confirmed reset in Options restores defaults. |
| Complete save state | Pause → Partida saves/restores gameplay, enemies, carried objects, edited maps, rooms, native release and zoom. Versioned format and compatibility checks, with eight confirmed save positions. |
| Eight save slots | Save/load browser with native scene thumbnails, level, local date, lives and explicit empty/damaged/incompatible states. Previous saves remain loadable. |
| Rewind and VHS | Hold `R` or controller `LB` to rewind up to eight seconds at 2× speed, with tape tracking/noise and a remaining-history indicator. Release to continue from the restored state. |
| Regression tests | Tests cover DOS sequencer accuracy, PCM continuity, zoom, doors, defeated enemies and live presentation changes. |

Audio output is 44.1 kHz. Technical tests pass, and the Windows DOS audio
adjustment has been confirmed in gameplay.

## Ideas and roadmap

Blues regression testing continues alongside the Titus Engine expansion. Final
Windows packaging is scheduled after the initial multi-engine catalog is validated.

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


## Windows multigame reference build

`Makefile.win` now accepts `MULTIGAME=1` to build Blues Brothers, Jukebox
Adventure and Prehistorik 2 together in `build/windows-x64-multigame/blues.exe`.
The default Windows build remains Blues-only in `build/windows-x64/`.
Use `--datapath` to select the game data; the global Titus library is still
planned. Fox and Prehistorik 1 currently have separate reference builds.
See [TH-02 reference checks](docs/ENGINE_REFERENCE_TESTS.md) for builds,
controls/audio results, source patches and known limitations.

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
digits and `R` are reserved during keyboard capture; `LB` is reserved on the controller. Controls shows the prepared
bindings, and held capture/confirmation inputs are consumed on resume.
Settings and bindings remain active through levels and across launches; see
Persistent preferences below.

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
They are also saved between launches.

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
follows the selected classic/fixed HUD scale. Assists and the life counts explicitly chosen in the menu persist between
launches. A new game uses those starting counts (three by default) and normal
health, including when using a nonzero `--level`. Lives gained/lost during play
do not change the chosen starting counts.

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

### Level selection

Open the pause menu and choose **Seleccionar nivel**. Choose any of the six
stages, then confirm **Iniciar**; **Volver** is selected by default. Choosing
the current stage restarts it. Stages with missing files are disabled, and a
DOS demo only offers its first stage. Amiga Concert supports the paired DOS
map fallback used by the normal loader.

A confirmed selection starts at the stage entrance with initial health, a fresh
map, enemies, inventory and checkpoints. Characters, one/two-player mode,
finite life counts (at least one), assists, DOS/Amiga mode, camera, HUD, zoom,
render rate, volumes and control bindings are retained. Both players rejoin in
two-player mode. Starting a stage discards the current stage's progress; merely
browsing or cancelling does not change the world.

```
BLUES_TEST_LEVELS=1 SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  build/tests/check_viewport --dos-datapath=../orig-games/dos \
  --amiga-datapath=../orig-games/amiga --level=1
```

The test drives the actual level loop through six destinations and a same-level
restart, checking fresh world state and retained settings. Add
`BLUES_TEST_LEVEL_MODE=1` to alternate the staged DOS/Amiga setting at each
selection, and `BLUES_TEST_LEVEL_TWO=1` for two players. The pause-menu unit test
also covers cancellation, disabled entries, default confirmation and layout.

### Persistent preferences

Options and controls load automatically at launch and are saved after applying
the pause menu, changing live presentation settings, and exiting normally.
Saved values include preferred DOS/Amiga release, camera, HUD, render rate,
framing, fullscreen, independent audio volumes, keyboard/controller bindings,
assists and manually chosen initial life counts. Game progress, resource paths,
current level, health and remaining lives are not saved here.

The per-user file is `preferences.ini` in SDL's application preference directory:

- Windows: `%APPDATA%\pedlop01\blues-engine\preferences.ini`.
- Linux/WSL: `$XDG_DATA_HOME/pedlop01/blues-engine/preferences.ini`, or
  `~/.local/share/pedlop01/blues-engine/preferences.ini` by default.

`BLUES_CONFIG_FILE` can select another file (its parent directory must exist);
an empty value disables reading and writing. Linux/WSL and Windows keep separate
preferences unless explicitly configured to use the same file.

**Options → Restaurar ajustes** asks for confirmation, restores options and
controls, and retains the running stage. It sets finite counts to three and
disables assists; it does not heal damage or resurrect an eliminated player.
The default release is DOS when both data sets are available; a single available
release is kept. Select Continue to apply the prepared changes.

Explicit command-line settings override loaded preferences. A saved Amiga
preference is used when both data paths are supplied; `--datapath` explicitly
chooses the initial data set, and a missing secondary path falls back to the
provided release. Game progress uses the separate save files described below.

The text format is versioned. Invalid values are ignored, duplicate/reserved
bindings are rejected, and unknown keys are tolerated. A newer format is left
untouched until an explicit reset. Writes replace the old file only after a
successful temporary write; configuration failures do not prevent playing.

```
make build/tests/check_preferences build/tests/check_viewport
build/tests/check_preferences
python3 tests/check_preferences_launches.py
```

Tests cover independent game-process launches, CLI precedence, Unicode paths
and bindings, bad/truncated values, future formats, failed writes, the real reset
menu, preserved health and restored controls/starting lives.

### Save and load a complete game

Open **Pause → Partida → Guardar partida** or **Cargar partida** and select one
of **eight numbered slots**. The selected slot shows its scene thumbnail, level,
DOS/Amiga release, local save date/time and life count (both players when playing
together, or an infinite-lives indication). The browser distinguishes empty,
damaged, incompatible and unreadable slots; only valid saves can be loaded.

Saving asks for confirmation, explicitly naming an occupied slot before
replacing it. Loading also asks for confirmation, with **Volver** selected by
default. Saving keeps the menu paused and refreshes that slot's preview and
metadata; loading restores its scene without leaking menu inputs into gameplay.
Errors leave the current game and the previous slot file intact.

Thumbnails capture the actual frozen game scene and native HUD, without the
menu, preserving aspect ratio. Image and state are stored atomically in the same
checksummed file. Your task-13 save remains **slot 1**; older saves still load,
showing **Sin miniatura** until replaced with a new save.

The save contains position, health/lives, inventory, checkpoints, enemies,
projectiles/carried objects, map edits, trigger states, doors, bonus room,
character/one-two-player state, eliminated players, native DOS/Amiga graphics,
HUD/camera, camera smoothing and in-progress zoom. Pending options apply on
Continue; saving captures the currently paused world. Your current controls,
fullscreen, render rate, audio volumes and preferred initial life counts remain
preferences. The saved music track restarts on load; exact audio playback phase
and transient effects are not restored.

Files are stored separately from original game data and preferences:

- Windows: `%APPDATA%\pedlop01\blues-engine\saves\slot01.bbs` through `slot08.bbs`.
- Linux/WSL: `$XDG_DATA_HOME/pedlop01/blues-engine/saves/slot01.bbs`, or
  `~/.local/share/pedlop01/blues-engine/saves/slot01.bbs` by default.

`BLUES_SAVE_DIR` can select an existing folder; an empty value disables save
storage. The file format is versioned, checksummed and independent of pointer
size. The saved release's original game files must still be supplied and match
the captured resources. Loading can change level and DOS/Amiga mode directly.
To load after restarting the application, start playing and open Pause → Partida.
The slots are independent; overwriting one does not alter the other seven.

```
make build/tests/check_viewport build/tests/check_preferences
python3 tests/check_state_launches.py
python3 tests/check_slots_launches.py
```

See [docs/SAVE_FORMAT.md](docs/SAVE_FORMAT.md) for the format, compatibility
rules and complete snapshot boundary.

### Rewind with a VHS effect

Hold **R** or **LB (left shoulder)** on an SDL game controller to travel backwards
through the last **eight seconds** of the current level, at **2× speed**. Release
the control to continue from the displayed point. At the oldest available state,
the picture holds until release. The future you abandoned is discarded.

The timeline restores both players, lives/health, inventory and carried objects,
enemies, edited tiles, triggers, rooms, camera and zoom transitions. It is a
bounded RAM buffer and does not write save slots. Loading a game, changing level
or characters, switching DOS/Amiga, or changing camera/HUD/assists starts a fresh
history. Simply pausing, changing volume or visiting a room preserves it. History
is not retained after closing the game.

While rewinding, audio is silent and a VHS overlay shows scanlines, moving
tracking bands, displaced strips, noise and the available seconds. Native HUD
artwork and the full map framing are preserved. On release the overlay disappears,
old sound effects are discarded and the restored state's music track restarts;
exact audio playback position is not rewound. Held gameplay controls must be
released before they act again. Pause and Quit also work during rewind.

```
make build/tests/check_viewport build/tests/check_pause_menu
build/tests/check_pause_menu
python3 tests/check_rewind_launches.py
```

The regression matrix runs the real game loop with R/LB events and checks all six
DOS/Amiga levels, both HUD/camera choices, 30/60/120 Hz, bonus rooms, zoom, two
players and irregular window sizes. It compares complete restored states,
deterministic replay, timeline wraparound, discarded futures, reset boundaries,
scene framing before distortion and clean pixels after removing the effect.
See [docs/REWIND.md](docs/REWIND.md) for implementation details.

### Return to character selection

**Pause → Elegir personaje** opens the original character screen after a
confirmation (Cancel selected by default). Choose Jake, Elwood or two players;
the current level restarts from its entrance, retaining finite life counts,
assists, DOS/Amiga mode, controls, volumes and presentation preferences.
Inventory, damage, checkpoints, enemies and map changes are reset. A returning
eliminated player receives at least one life. Cancelling keeps the game intact.

Add `BLUES_TEST_CHARACTER=1` to `BLUES_TEST_LEVELS=1` above to test repeated
returns through the actual character screen, selecting Jake, Elwood and two
players before verifying the newly initialized stage.

### Ladders and warehouse tube recovery

Player ladder input preserves the exact horizontal position, including the
first frame of Up/Down. Player coordinates represent the sprite centre;
snapping them to a tile edge caused the visible leftward jump. Only NPCs retain
the original grid alignment. Residual conveyor acceleration is cleared on
ladders. Regression tests use actual keyboard input across all 16 entry offsets
on short and long ladders, and also cover conveyor impulses, ascent/descent,
idle, jump mappings and the warehouse pipe. The first-frame position check
fails against the previous implementation and passes with this correction.


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

### Level presentation and progress marks

The inter-level map keeps completed-level marks on a native 320×200 canvas,
animates the new crossed-out location and displays the next destination's musical
note. Each animation step uploads the changed bitmap before presentation, and
Space skips the remaining animation without leaving a partial mark. The original
final-stage behavior is retained. The canvas stays centered for larger gameplay
viewports; markers retain the original DOS/Amiga artwork and DOS CGA dithering.

```
make build/tests/check_intermission
build/tests/check_intermission ../orig-games/blues-amiga
build/tests/check_intermission ../orig-games/blues-dos
build/tests/check_intermission ../orig-games/blues-amiga wide
build/tests/check_intermission ../orig-games/blues-dos wide cga
```

Replace the data paths with your own installations. The test runs the production
presentation for all six destinations and independently decodes the brush/note
pixels. It checks accumulated marks, progressive rendering, actual texture
uploads, centering, skip/quit handling and clipped stamps. Set
`BLUES_VIEW_TEST_OUTPUT` to an existing directory for BMP captures.

### Amiga foreground, background and lift graphics

```
make build/tests/check_viewport
python3 tests/check_amiga_layers.py
```

Native `.m` files contain 16×16 foreground masks. The masks are copied before
the temporary resource buffer is reused, combined with each tile's native colors,
and drawn after actors. Transparent holes and opaque black stay distinct; masked
copper-color pixels retain their live gradient color. Foreground drawing uses
the current SQL animation frame and includes the same scroll guard band as the
background. The copper gradient spans the playfield from its top to its bottom,
rather than ending early above the lower HUD.

Player graphics use three bitplanes and colors 16–23; common `objet` graphics use
four bitplanes and the level palette. The tests compare all foreground mask pixels,
holes, black, copper rows, native prop colors, level sprite flips and alternating
lift frames. They also compare 400 incremental tile edits with a full redraw at a
non-aligned camera origin: moving lifts and their shafts must update in viewport
coordinates, while the SQL map retains world coordinates. Tests run in all six
Amiga stages and both HUD modes. Rideable lifts select their native color
reference from the actual platform type in the level, retaining the original
transparent silhouette. This covers the shopping-centre/city pair 146/147 and
Concert's pair 160/161. The city reference matches BLK tile 0 exactly; its
regression checks that independent map graphic rather than the miscolored
coverage indices. Moving/flipped lift tests compare all intermediate pixels
for 60 ticks in each of those stages. With `BLUES_VIEW_TEST_OUTPUT` pointing to
an existing directory, before/after occlusion, city lift and Concert lift
captures are available.

### Scrolling edges and Amiga copper regression

```
make build/tests/check_viewport
python3 tests/check_scroll_launches.py
```

At 1× and 60 Hz, this checks every world pixel at the four camera boundaries and
an interior position, in both directions and diagonally, including intermediate
frames. All six DOS/Amiga stages and both native HUD choices are covered. The
former renderer is also exercised to prove the fixtures detect its edge gaps,
HUD bleed and moving copper colors. A 32-pixel map guard band supplies the scenery
outside the current viewport; Amiga copper colors are composed at their fixed
screen rows behind transparent background pixels. Camera jumps larger than that
margin cut immediately instead of interpolating across disconnected views.

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
