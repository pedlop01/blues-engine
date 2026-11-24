#!/usr/bin/env python3
"""Development validator for the TH-03 v1 design contracts, not a launcher.

Requires jsonschema >=4.10,<5. No engine is executed, data folder inspected,
or live catalog/preferences/save modified. The C++ implementation follows later.
"""
import argparse
import json
from pathlib import Path, PurePosixPath, PureWindowsPath

from jsonschema import Draft202012Validator, FormatChecker

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = json.loads((ROOT / "contracts/titus-v1.schema.json").read_text())
CHECKER = FormatChecker()
GAME_ENGINES = {"blues": "blues-family", "jukebox": "blues-family",
                "prehistorik2": "blues-family", "fox": "opentitus", "prehistorik": "olduvai"}


def loads(text):
    size = len(text.encode("utf-8"))
    require(size <= 1024 * 1024, "Document exceeds v1 size limit")
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, f"Duplicate JSON key: {key}")
            result[key] = value
        return result

    def constant(value):
        raise ValueError(f"Non-JSON numeric constant: {value}")

    result = json.loads(text, object_pairs_hook=pairs, parse_constant=constant)
    limits = {"launch": 128 * 1024, "inspect_request": 128 * 1024,
              "result": 32 * 1024, "inspection": 32 * 1024, "event": 16 * 1024, "command": 16 * 1024}
    if isinstance(result, dict):
        require(size <= limits.get(result.get("kind"), 1024 * 1024), "Message exceeds its v1 size limit")
    # A UTF-8 document must not carry isolated surrogate code points.
    json.dumps(result, ensure_ascii=False).encode("utf-8")
    return result


@CHECKER.checks("titus-absolute-path")
def absolute_path(value):
    return isinstance(value, str) and "\0" not in value and (
        PurePosixPath(value).is_absolute() or PureWindowsPath(value).is_absolute())


def require(condition, message):
    if not condition:
        raise ValueError(message)


def unique(items, key, message):
    values = [item[key] for item in items]
    require(len(values) == len(set(values)), message)


def path_of(location):
    cls = PureWindowsPath if location["platform"] == "windows" else PurePosixPath
    path = cls(location["path"])
    require(path.is_absolute() and ".." not in path.parts, "Path must be absolute and normalized for its platform")
    return path


def native_engine(game,edition="dos",engine=None):
    if game in {"fox","prehistorik"} and (edition=="amiga" or engine in {"fox-amiga-native","pre1-amiga-native"}):
        return "fox-amiga-native" if game=="fox" else "pre1-amiga-native"
    return GAME_ENGINES[game]


def contains(parent, child):
    return child == parent or parent in child.parents


def validate(document):
    Draft202012Validator(SCHEMA, format_checker=CHECKER).validate(document)
    kind = document["kind"]
    if kind == "catalog":
        unique(document["profiles"], "id", "Duplicate profile id")
        unique(document["installations"], "id", "Duplicate installation id")
        for installation in document["installations"]:
            game, engine = installation["game_id"], installation["engine_id"]
            supported = installation["edition_id"] == "dos" or game == "blues" or game in {"fox","prehistorik"}
            expected = native_engine(game,installation["edition_id"])
            legacy = game in {"fox","prehistorik"} and installation["edition_id"] == "amiga" and engine is None and installation["status"] in {"missing_engine","incomplete","unverified"}
            require(legacy or engine == (expected if supported else None), "Wrong engine for game/edition")
            if engine is None:
                require(installation["status"] in {"missing_engine", "incomplete", "unverified"},
                        "Unsupported edition cannot be playable")
            locations = [(item["kind"], item["location"]["platform"], path_of(item["location"]))
                         for item in installation["locations"]]
            require(len(locations) == len(set(locations)), "Duplicate location")
    elif kind == "engine_registry":
        unique(document["engines"], "engine_id", "Duplicate engine id")
        games = []
        for entry in document["engines"]:
            for game in entry["games"]:
                require(native_engine(game["game_id"],engine=entry["engine_id"]) == entry["engine_id"], "Registry engine/game mismatch")
                require(game["editions"] == (["amiga"] if entry["engine_id"] in {"fox-amiga-native","pre1-amiga-native"} else ["dos", "amiga"] if game["game_id"] == "blues" else ["dos"]),
                        "Unverified edition must not be offered")
                games.extend((game["game_id"],edition) for edition in game["editions"])
        require(len(games) == len(set(games)), "Game registered twice")
    elif kind == "launch":
        require(native_engine(document["game_id"],document["edition_id"]) == document["engine_id"], "Launch engine/game mismatch")
        unique(document["sources"], "installation_id", "Duplicate launch source")
        primary = [item for item in document["sources"] if item["installation_id"] == document["installation_id"]]
        require(len(primary) == 1, "Missing primary installation")
        require(primary[0]["edition_id"] == document["edition_id"], "Primary edition mismatch")
        for source in document["sources"]:
            require(source["game_id"] == document["game_id"], "Cross-game auxiliary data")
            require(source["location"]["platform"] == document["platform"], "Source platform mismatch")
            require(source["edition_id"] == "dos" or document["game_id"] in {"blues","fox","prehistorik"}, "Unsupported edition")
            path_of(source["location"])
        require(document["game_id"] == "blues" or len(document["sources"]) == 1, "Auxiliary editions only for Blues")
        unique(document["sources"], "edition_id", "Ambiguous duplicate edition in launch")
        writable = []
        for location in document["paths"].values():
            require(location["platform"] == document["platform"], "Writable platform mismatch")
            writable.append(path_of(location))
        for source in document["sources"]:
            original = path_of(source["location"])
            for destination in writable:
                require(not contains(original, destination) and not contains(destination, original), "Storage overlaps original data")
        profile, work, session = [path_of(document["paths"][name]) for name in
                                  ("profile_directory", "work_directory", "session_directory")]
        require(work != profile and contains(profile, work), "Native workspace must be inside the game profile")
        require(not contains(profile, session) and not contains(session, profile), "Session cache must be separate from profile")
        features = set(document["required_features"])
        settings = document["settings"]
        require("video" not in settings or "fullscreen" in features, "Video setting requires fullscreen capability")
        require("audio" not in settings or "separate_volumes" in features, "Volumes require separate_volumes capability")
        require("input" not in settings or "controller_bindings" in features,"Bindings require controller_bindings capability")
        if "blues" in settings:
            require(document["game_id"] == "blues", "Blues settings leaked to another game")
            gates = {"camera": "smooth_camera", "hud": "hud_modes", "view_step": "dynamic_view",
                     "invulnerable": "invulnerability", "infinite_lives": "lives_options",
                     "lives_player1": "lives_options", "lives_player2": "lives_options",
                     "start_level": "level_select", "sound_mode": "audio_mode_selection"}
            require(all(gates[key] in features for key in settings["blues"]), "Setting capability not requested")
        for group in ["fox","prehistorik"]:
            if group in settings:
                require(document["game_id"]==group and document["edition_id"]=="amiga", "Native settings require their Amiga edition")
                gates={"camera":"smooth_camera","hud":"hud_modes","view_step":"dynamic_view","invulnerable":"invulnerability","infinite_lives":"lives_options","lives":"lives_options","start_level":"level_select"}
                require(all(gates[key] in features for key in settings[group]), "Native setting capability not requested")
        if document["action"]["type"] == "load_slot":
            require((document["game_id"] == "blues" or document["engine_id"] in {"fox-amiga-native","pre1-amiga-native"}) and "save_slots" in features, "This edition has no global resume")
    elif kind == "event" and document["event"] == "ready":
        features = set(document["body"]["features"])
        require((document["body"]["save_slot_count"] > 0) == ("save_slots" in features), "Slot capability/count mismatch")
        require("save_thumbnails" not in features or "save_slots" in features, "Thumbnails require slots")
    elif kind == "save_index":
        require(document["engine_id"]==native_engine(document["game_id"],"amiga"),"Wrong save index engine")
        unique(document["slots"], "slot", "Duplicate save slot")
        for slot in document["slots"]:
            require(slot["file"] == (f"slot-{slot['slot']}.json" if document["game_id"] in {"fox","prehistorik"} else f"saves/slot{slot['slot']:02}.bbs"), "Wrong native slot filename")
            require(slot["format_id"]==(document["game_id"]+".amiga.json" if document["game_id"] in {"fox","prehistorik"} else "blues.bbs"),"Wrong native save format")
    elif kind == "inspect_request":
        validate(inspection_as_launch(document))
    elif kind == "inspection":
        require(native_engine(document["game_id"],engine=document["engine_id"]) == document["engine_id"], "Inspection identity mismatch")
        features = set(document["features"])
        require((document["save_slot_count"] > 0) == ("save_slots" in features), "Inspection slot capability/count mismatch")
        require("save_thumbnails" not in features or "save_slots" in features, "Inspection thumbnails require slots")
        require(document["status"] != "preparation_required" or document["preparation_ids"], "Missing preparation plan")
        index = document["save_index"]
        if index is not None:
            validate(index)
            for key in ("profile_id", "game_id", "engine_id", "save_compatibility_id"):
                require(index[key] == document[key], "Inspection returned another profile's saves")
            require(document["save_slot_count"] == 8, "Inspection lost Blues slot count")
    return document


def inspection_as_launch(request):
    result = {key: value for key, value in request.items() if key not in ("kind", "inspection_id")}
    result.update(kind="launch", session_id=request["inspection_id"], action=dict(type="new_game"),
                  settings={}, required_features=["return_to_library"], allow_silent_audio=False)
    return result


def validate_inspection(request, result, process_exit):
    validate(request); validate(result)
    require(request["kind"] == "inspect_request" and result["kind"] == "inspection", "Wrong inspection document kinds")
    for key in ("inspection_id", "profile_id", "game_id", "engine_id", "save_compatibility_id"):
        require(request[key] == result[key], "Inspection result belongs to another request/context")
    if result["status"] != "failed":
        require(process_exit == 0, "Inspection process failed")
        return True
    return False


def validate_launch_catalog(request, catalog):
    validate(request); validate(catalog)
    require(request["kind"] == "launch" and catalog["kind"] == "catalog", "Wrong launch/catalog document kinds")
    require(any(item["id"] == request["profile_id"] for item in catalog["profiles"]), "Unknown profile")
    for source in request["sources"]:
        matches = [item for item in catalog["installations"] if item["id"] == source["installation_id"]]
        require(len(matches) == 1, "Unknown installation")
        item = matches[0]
        require(item["status"] in ("recognized", "needs_preparation"), "Installation not launchable")
        require(item["engine_id"] == request["engine_id"], "Catalog engine mismatch")
        for key in ("game_id", "edition_id", "variant_id", "fingerprint"):
            require(source[key] == item[key], f"Source {key} differs from catalog")
        require(item["save_compatibility_id"] == request["save_compatibility_id"], "Save compatibility mismatch")
        require(any(location["kind"] == "directory" and location["location"]["platform"] == request["platform"]
                    and path_of(location["location"]) == path_of(source["location"]) for location in item["locations"]),
                "Source location not registered")


def validate_session(request, events, result, process_exit, commands=()):
    validate(request); validate(result)
    require(request["kind"] == "launch" and result["kind"] == "result", "Wrong session document kinds")
    require(result["session_id"] == request["session_id"], "Result belongs to another session")
    require(result["game_id"] == request["game_id"] and result["engine_id"] == request["engine_id"], "Result identity mismatch")
    ready, closing, failed, confirmation = False, False, False, False
    available = set()
    for sequence, event in enumerate(events, 1):
        validate(event)
        require(event["session_id"] == request["session_id"] and event["seq"] == sequence, "Wrong session/event sequence")
        name = event["event"]
        require(not closing, "Event after closing")
        require((sequence == 1) == (name == "hello"), "First event must be the only hello")
        if name == "hello":
            require(event["body"]["engine_id"] == request["engine_id"], "Wrong runtime hello")
        elif name == "ready":
            require(not ready and not failed, "Duplicate/late ready")
            ready = True; available = set(event["body"]["features"])
            require(request["allow_silent_audio"] or event["body"]["audio_active"], "Silent fallback was not allowed")
            require(set(request["required_features"]) <= available, "Runtime cannot satisfy requested capabilities")
            if "live_edition_switch" in available:
                require(request["game_id"] == "blues" and {item["edition_id"] for item in request["sources"]} == {"dos", "amiga"},
                        "Live edition switch needs both Blues editions")
            if request["game_id"] == "blues" and "save_slots" in available:
                require(event["body"]["save_slot_count"] == 8, "Blues must preserve eight slots")
        elif name == "progress":
            require(not ready and not failed, "Startup progress after ready/error")
        elif name == "return_requested":
            require(ready and not failed, "Return requested before ready")
            confirmation = event["body"]["confirmation_required"]
        elif name == "return_cancelled":
            require(confirmation and ready and not failed, "Unexpected return cancellation")
            confirmation = False
        elif name == "closing":
            closing = True
        elif name == "error":
            failed = True
    require(bool(events), "Missing hello")
    require(result["ready"] == ready, "Result ready disagrees with events")
    for sequence, command in enumerate(commands, 1):
        validate(command)
        require(command["session_id"] == request["session_id"] and command["seq"] == sequence, "Wrong command sequence/session")
        if command["command"] in ("pause", "resume"):
            require("remote_pause" in available, "Remote pause is unsupported")
    success = result["outcome"] in ("returned", "quit")
    if success:
        require(process_exit == 0 and closing and not failed, "Unclean or incomplete session exit")
    elif result["outcome"] == "cancelled":
        require(process_exit == 0 and closing and not failed, "Unclean cancellation")
    return success


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("documents", nargs="+", type=Path)
    args = parser.parse_args()
    Draft202012Validator.check_schema(SCHEMA)
    for path in args.documents:
        validate(loads(path.read_text(encoding="utf-8")))
        print(f"{path.name}: valid v1 design document")


if __name__ == "__main__":
    main()
