# TH-11 development pilot

`titus-inprocess-pilot` is an opt-in SDL host, not the production library UI.
It calls the real managed Olduvai bootstrap either directly or through the
same executable in a child process. A profile lock, verified source request,
private native preparation and the existing protocol remain mandatory.

Build with `TITUS_BUILD_INPROCESS_PILOT=ON` and the pinned TH-09 source setup.
The option defaults to OFF, requires `TITUS_BUILD_PRE1_RUNTIME`, is absent from
the engine registry, and has no install rule. The normal `titus-engine` binary
does not link Olduvai. Explicit exception/exit injection is compiled only into
this diagnostic target. Do not launch the pilot against a real player profile;
the test driver creates disposable storage and preserves all original data.

```sh
python3 tests/check_inprocess_pilot.py --bin-dir build/launcher \
  --orig-games ../orig-games --report build/th11-linux.json
# Native Windows PE, invoked through WSL; no Wine:
python3 tests/check_inprocess_pilot.py --bin-dir build/launcher-windows \
  --orig-games ../orig-games --windows --report build/th11-windows.json
```

The sources here and the conditional bridge are GPL-3.0-or-later. Linking the
pinned engine includes xBRZ/GPL3-only and the dependencies already listed in
[Olduvai notices](../../engines/olduvai/NOTICE.md). No game resources are bundled;
final distribution/source packaging stays in task 18.

See the [evaluation and decision](../../docs/INPROCESS_EVALUATION.md) for the
lifetime, failure, audio and timing evidence and the limits of this experiment.
