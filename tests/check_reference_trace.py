#!/usr/bin/env python3
"""Validate the short Olduvai route and optionally compare Linux/Windows states.

The oracle here is cross-platform determinism of the selected reference engine,
not equivalence to the original DOS executable.
"""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("trace", type=Path)
parser.add_argument("comparison", nargs="?", type=Path)
args = parser.parse_args()
states = [json.loads(line) for line in args.trace.read_text().splitlines()]
assert len(states) == 140, f"Expected 140 frames, received {len(states)}"
assert [state["frame"] for state in states] == list(range(140))
right = states[39]["player_x"] - states[9]["player_x"]
left = states[39]["player_x"] - states[59]["player_x"]
jump = states[59]["player_y"] - min(state["player_y"] for state in states[59:79])
attack = max(state["club_flag"] for state in states[79:99])
assert right > 0 and left > 0 and jump > 0 and attack
assert not any(state["death_flag"] for state in states)
if args.comparison:
    other = [json.loads(line) for line in args.comparison.read_text().splitlines()]
    assert states == other, "Platform traces differ"
digest = hashlib.sha256(json.dumps(states, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
print(json.dumps(dict(frames=len(states), right=right, left=left, jump=jump, attack=attack,
                      canonical_trace_sha256=digest, identical_platforms=bool(args.comparison)), indent=2))
