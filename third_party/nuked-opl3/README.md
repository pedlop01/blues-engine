Nuked OPL3 1.8 by Nuke.YKT, used in OPL2 compatibility mode for DOS AdLib audio.

Upstream: https://github.com/nukeykt/Nuked-OPL3
Revision: 765ec962e473aeb767e4cba74ffdc8f588ffbfe8

`opl3.c` and `opl3.h` carry a blues-engine extension: per-channel Q16 output
gains, initialized to unity and applied after synthesis. Unity output remains
bit-identical to the pinned upstream; register handling, operators, envelopes
and resampling are unchanged. This allows separate volume control for the
music/effect ownership of the original shared DOS channel 6.
The files remain licensed under LGPL 2.1 or later. The complete license is included in `LICENSE`.
