# Blues Brothers save states, version 1

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
| 8 | 4 | Format version, currently 1 |
| 12 | 4 | Payload byte count, excluding the 24-byte header |
| 16 | 4 | IEEE CRC-32 of the payload |
| 20 | 4 | Schema identifier `0x42423031` |
| 24 | variable | Explicit gameplay payload |

The total file is limited to 65,536 bytes. Truncation, trailing data, unsupported
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

The SDL backend writes `saves/slot01.bbs` below the per-user preference directory
using a per-process temporary file and atomic replacement. UTF-8 paths use wide
Windows filesystem calls. `BLUES_SAVE_DIR` selects an existing alternate folder;
an empty value disables storage. The backend slot API accepts indices 0–7, but
task 13 exposes only one save through Pause → Partida. Overwrite and load both
require confirmation, defaulting to Cancel. The eight-slot browser and preview
images remain task 14.

`tests/check_state_launches.py` covers the six levels and both native releases,
short deterministic futures, bonus rooms, carried crates, two players (including
an eliminated player), zoom transitions, cross-mode/level/process restoration,
corrupt/future/truncated files, missing/mismatched resources and the production
menu. Save storage write-failure checks are in `tests/check_preferences.c`.
