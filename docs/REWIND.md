# Rewind timeline and VHS presentation

The first Blues Brothers engine keeps a ring of 241 `struct bb_state` snapshots:
one current boundary plus 240 previous 30 Hz frames (eight seconds). Allocation
is bounded and released on level exit; allocation failure disables rewind while
leaving normal gameplay available. Snapshots are recorded only after completed
simulation frames and use the same full-state capture as disk saves. They include
mutable SQL tiles, triggers, animation counters, RNG/game variables, both players,
objects, camera, viewport dimensions and zoom transition state. They do not include
thumbnail pixels, audio phase or serialized files. Static table references remain
valid within a single resource context.

`bb/level_rewind.h` owns the ring. Forward recording wraps at capacity. A reverse
tick consumes two snapshots at roughly 30 Hz, then renders the selected boundary
without running physics, triggers, RNG or HUD side effects. The count/head move
back immediately: resuming can never select an abandoned future. The oldest state
holds when count reaches one. The same runtime restoration helper serves disk
loads and rewind; only disk loads reinitialize resources. Rewind resizes renderer
and VGA storage when crossing zoom/room boundaries, then restores camera fields
and repaints. Allocation failure stops rewind safely.

Loading successfully clears history. New levels and character selection allocate
a new timeline. Resource signature/native mode, stage, camera mode, HUD mode and
assists are context boundaries: a mismatch discards prior snapshots before reuse.
A door to a bonus room is within the same context, so it can be crossed backwards.
An ordinary pause and audio-volume/control changes retain the timeline. RAM
history is never written into the eight save slots or into preferences.

R and SDL controller Left Shoulder are reserved hold controls; independent held
flags allow either to keep rewind active until both are released. Focus loss or
controller removal releases the corresponding input. Releasing rewind, opening
pause or quitting exits the reverse path. On resume, held gameplay inputs are
blocked until released/neutral and simulation resumes on a fresh input tick.
Pause requests survive that input cleanup. Quitting frees history without
reinitializing audio.

Audio is paused throughout reverse playback. Once on release, the mixer and
sound backend are restarted to discard effects from the old future, and the
restored song is played from its beginning. User music/effect levels remain
unchanged. There is no per-snapshot audio or file reload.

`sys_sdl2_rewind.h` applies a presentation-only effect. A cached target texture
captures the full physical renderer output, retaining the native HUD and both
framing modes. SDL logical transforms are recalculated when changing targets;
otherwise the scene can shrink into the upper-left corner. Thin displaced strips,
scanlines, a moving translucent tracking band and short noise streaks create the
tape effect, with a bitmap-font rewind/time label. Noise uses a private local seed
and cannot affect simulation RNG. Without render-target support the overlay still
works without displaced strips. Preview capture and pause UI exclude the effect.
The original textures/palettes are never modified; blend/color/logical state is
restored afterwards and the target is recreated only for output-size changes.

Run `python3 tests/check_rewind_launches.py` after building
`build/tests/check_viewport`; set `BLUES_TEST_BINARY` to use a sanitizer build.
The suite compares all snapshot fields against an independent reference sequence,
checks ring wrap/exhaustion, tile/inventory edits, deterministic future replay,
room/zoom restoration, context reset and successful disk-load reset. It also
verifies scene pixels before distortion, complete effect removal, audio pause,
and R/LB release/pause/quit behavior through the production game loop, across all
six levels and both releases, cameras and HUD modes. The two-player cases resize
to 853×533; presentation rates cover 30/60/120 Hz. `BLUES_VIEW_TEST_OUTPUT` selects
an existing directory for BMP captures of VHS and resumed scenes.
