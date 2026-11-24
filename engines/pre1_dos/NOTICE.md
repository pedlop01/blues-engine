# Prehistorik DOS

Titus Engine extends Olduvai (GPL-3.0-or-later), revision
a69e9204fc311c8810d696a6b799000d1bcd1642, by Krzysztof Sokołowski.
Upstream: https://github.com/ksokolowski/olduvai

This isolated DOS kernel retains DOS AI, the 18 Hz cadence and the original LCG.
Our continuous-world presentation, SDL2 menu, full state saves and rewind are
separate from the Amiga kernel. See logic/UPSTREAM.json and vendor/UPSTREAM.json.
No original game data is included. Original DOS archives and executable are needed.

The original DOS MDI driver uses Nuked OPL3 by Nuke.YKT (LGPL-2.1-or-later).
VOC effects are decoded from the user's archives and mixed on a separate bus.
