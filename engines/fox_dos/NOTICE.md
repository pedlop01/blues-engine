# Titus the Fox DOS

Titus Engine extends OpenTitus (GPL-3.0-or-later), revision
6540e8f2867f99aff54ad1a710ae6379ecb9c590, by the OpenTitus team.
Upstream: https://github.com/stople/opentitus

The DOS game logic remains separate from the Amiga implementation. Our SDL2
host adds smooth presentation, map framing, menus, state saves and rewind.
Resource and pointer state are serialized explicitly; original game files are
required and are never included. See legacy/UPSTREAM.json for source provenance.

OPL synthesis uses Nuked OPL3 by Nuke.YKT (LGPL-2.1-or-later); its license is
included. Audio retains OpenTitus sequencing with independent music/effect buses.
