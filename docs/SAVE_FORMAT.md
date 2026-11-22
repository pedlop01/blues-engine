# Blues Brothers save states, versions 1 and 2

The gameplay snapshot is defined by `bb/state.h`, with explicit scalar lists in
`bb/state_fields.h` and its codec in `bb/state.c`. Never write C structures,
pointers, compiler padding or library internals directly to a save file. Bump
the format version/schema whenever field order, table identities or semantics
change incompatibly.

## File layout

All multibyte values are little-endian, independent of host pointer size.

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 8 | Magic bytes `BBSAVE\r\n` |
| 8 | 4 | Format version: 1 (legacy) or 2 (with preview) |
| 12 | 4 | Payload byte count, excluding the 24-byte header |
| 16 | 4 | IEEE CRC-32 of the payload |
| 20 | 4 | Schema identifier `0x42423031` |
| 24 | variable | Explicit gameplay payload |

The total file is limited to 131,072 bytes. Truncation, trailing data, unsupported
versions/schemas and checksum errors are rejected before state changes.

Payload order follows `payload()` in `bb/state.c`: scalar variables; spawn
coordinates; tile lookup; doors; every object's scalars and static references;
collision dimensions; trigger scalars and static references; all 16,000 SQL map
bytes (including bonus rooms); 103 animation counters; the six-level object-type
table; viewport/base dimensions and zoom/fractional timing; native presentation
and assist flags, initial position overrides and legacy scrolling; zoom transition; camera state; resource signature; UTC Unix
save timestamp. Scalars use four bytes, including booleans (0 or 1). Signed
values use two's complement. Raw byte arrays remain bytes. Camera doubles are
IEEE-754 binary64, serialized as two little-endian words; the timestamp is also
two words.

Version 2 retains the exact version-1 gameplay payload and appends a 128×80
RGB24 thumbnail (30,720 bytes, row-major R/G/B), then two little-endian uint32
words containing width 128 and height 80. The payload count and CRC cover both
gameplay and preview/footer. The decoder rejects inconsistent dimensions,
truncation, bad CRCs and trailing bytes. The in-memory `bb_state` and base encoder
remain independent of the preview; `bb_state_attach_preview` wraps a valid v1
encoding into v2. Older v1 files are read without modification and simply lack
a preview. Old executables reject v2 as an unsupported version.

Static animation references use an index into `animframes_059d`, with -1 for
null. Trigger references use table IDs, with 255 for null. Animation table 2
uses 86 primary and 17 secondary IDs; shared pointers are canonicalized. Decoder
IDs are resolved against compiled static tables only. No saved address is ever
cast into a pointer. Numeric bounds, table indices, aliases, object types,
viewport bounds and active simulation invariants are checked before application.
Some original BINs contain invalid animation indices in unused entries; these
are preserved but may not be referenced by the saved map.

## Resource compatibility

A native-release/level signature hashes the exact required map, graphics and
trigger resources, including common sprites and the paired DOS Concert map
when the Amiga fallback is used. The signature is captured when resources are
loaded, not invented from an edited live map. Before loading, required files
must be readable and their signature must match. A save does not embed original
game artwork, music or executable data. The appropriate user-supplied release
must still be available.

## Capture and restore boundary

`bb/level_state.h` integrates capture with the active level's local timing and
view state. Capture/save is read-only at a paused simulation boundary. Pending
menu changes remain pending until Continue, so Save captures the current paused
world rather than silently applying other menu edits.

A valid load preflights resources and renderer allocation, stops audio, rebuilds
native resources, rebinds static references, and applies the saved variables,
map, triggers, collision dimensions, mutable animation phases and Concert ending
counter. It restores current zoom size and its in-progress transition, bonus-room
camera lock, camera smoothing, assists, character and player-elimination state.
A separate repaint draws the saved animation frames without moving objects,
advancing animations, spending lives or changing the simulation. Interpolation
history is cleared to avoid blending with the previous scene, and physical menu
inputs are consumed. Loading a rejected file leaves the world untouched.

Controls, fullscreen, render rate, audio volumes and the preferred initial life
counts remain the user's current preferences. Saved native mode, camera/HUD,
view and assists are restored because they define the saved scene. Music restarts
the saved track; transient effects and the exact AdLib/module playback phase
are not serialized. This keeps audio library/chip pointers out of the format.

## Storage and UI

The SDL backend writes `saves/slot01.bbs` through `slot08.bbs` below the per-user preference directory
using a per-process temporary file and atomic replacement. UTF-8 paths use wide
Windows filesystem calls. `BLUES_SAVE_DIR` selects an existing alternate folder;
an empty value disables storage. The backend slot API accepts indices 0–7.
Pause → Partida exposes all eight entries. Occupied saves and loads require
confirmation, defaulting to Cancel. Browser metadata is decoded and checked
without replacing the running world; missing assets/version mismatches are
incompatible, malformed/checksum failures are damaged, and missing files are
empty. Slots with no loadable state cannot enter load confirmation.

Preview capture redraws the frozen scene without the modal overlay, suppresses
Present, reads RGB pixels before presentation (safe with GPU double buffering),
restores the renderer/menu state, and downsamples with aspect-preserving
letterboxing. It neither advances simulation nor resumes audio. Preview capture
and file writes must both succeed before the old slot is replaced. A cached
128×80 RGB texture presents the selected entry; browsing does not load its level
or change its palette/native mode.

`tests/check_state_launches.py` covers the six levels and both native releases,
short deterministic futures, bonus rooms, carried crates, two players (including
an eliminated player), zoom transitions, cross-mode/level/process restoration,
corrupt/future/truncated files, missing/mismatched resources and the production
menu. Save storage write-failure checks are in `tests/check_preferences.c`.

`tests/check_slots_launches.py` verifies all eight independent state/image files,
exact restored previews across processes/releases, classic/fixed HUD, two players,
non-integer window sizes, legacy files, corruption/version/resource states and
failed capture protection. Pause-menu tests also check blocked loads, cancellation,
slot routing, preview colors and layout over multiple window sizes/fullscreen.
