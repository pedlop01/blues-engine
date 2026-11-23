#!/usr/bin/env python3
"""Design-contract checks for catalog identity, safe launch and session outcome.

No game data or runtime adapters required. These tests validate the TH-03 spec,
not a working launcher or a completed save migration.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from jsonschema import ValidationError
from validate_contract import loads, validate, validate_inspection, validate_launch_catalog, validate_session


def example(name):
    return loads((ROOT / "contracts/examples" / (name + ".json")).read_text(encoding="utf-8"))


def closing(request, seq=3):
    return dict(schema_version=1, kind="event", session_id=request["session_id"], seq=seq, event="closing", body={})


class ContractTests(unittest.TestCase):
    def setUp(self):
        self.request = example("launch")
        self.catalog = example("catalog")
        self.events = [example("event-hello"), example("event-ready"), closing(self.request)]
        self.result = example("result")

    def test_all_examples_and_clean_return(self):
        for path in sorted((ROOT / "contracts/examples").glob("*.json")):
            with self.subTest(example=path.name):
                validate(loads(path.read_text(encoding="utf-8")))
        validate_launch_catalog(self.request, self.catalog)
        self.assertTrue(validate_session(self.request, self.events, self.result, 0, [example("command-return")]))

    def test_zero_exit_after_failed_load_is_not_success(self):
        failed = deepcopy(self.result)
        failed.update(outcome="load_failed", ready=False,
                      error=dict(code="E_DATA_VARIANT", message="Unsupported executable"))
        events = [self.events[0], dict(schema_version=1, kind="event", session_id=failed["session_id"],
                                      seq=2, event="error", body=failed["error"]), closing(self.request)]
        self.assertFalse(validate_session(self.request, events, failed, 0))

    def test_preflight_and_native_save_inspection(self):
        request, result = example("inspect-request"), example("inspection")
        self.assertTrue(validate_inspection(request, result, 0))
        other = deepcopy(result); other["save_index"]["profile_id"] = request["inspection_id"]
        with self.assertRaises(ValueError):
            validate_inspection(request, other, 0)
        with self.assertRaises(ValueError):
            validate_inspection(request, result, 1)

    def test_preparation_is_explicit_before_launch(self):
        result = example("inspection")
        result.update(status="preparation_required", preparation_ids=["stage-resources-v1"], save_index=None)
        self.assertTrue(validate_inspection(example("inspect-request"), result, 0))
        result["preparation_ids"] = []
        with self.assertRaises(ValueError):
            validate_inspection(example("inspect-request"), result, 0)

    def test_folder_move_preserves_identity_and_can_be_relocated(self):
        moved = deepcopy(self.catalog)
        source = moved["installations"][0]
        source["locations"][0]["location"]["path"] = "E:\\Nueva ubicación\\Blues"
        with self.assertRaises(ValueError):
            validate_launch_catalog(self.request, moved)
        relocated = deepcopy(self.request)
        relocated["sources"][0]["location"] = source["locations"][0]["location"]
        validate_launch_catalog(relocated, moved)
        self.assertEqual(relocated["installation_id"], self.request["installation_id"])
        self.assertEqual(relocated["save_compatibility_id"], self.request["save_compatibility_id"])

    def test_mixed_folder_produces_two_distinct_installations(self):
        mixed = deepcopy(self.catalog)
        first, second = mixed["installations"]
        first.update(game_id="prehistorik2", engine_id="blues-family", edition_id="dos",
                     variant_id="full", save_compatibility_id="prehistorik2.full")
        second.update(game_id="prehistorik", engine_id="olduvai", edition_id="dos",
                      variant_id="diet-verified", save_compatibility_id="prehistorik.full", status="needs_preparation",
                      preparation_id="prehistorik-diet")
        second["locations"] = deepcopy(first["locations"])
        validate(mixed)
        self.assertNotEqual(first["id"], second["id"])

    def test_unsupported_amiga_can_report_data_damage_without_becoming_playable(self):
        catalog = deepcopy(self.catalog)
        installation = catalog["installations"][1]
        installation.update(game_id="fox", engine_id=None)
        for status in ("missing_engine", "incomplete", "unverified"):
            installation["status"] = status
            validate(catalog)
        for status in ("recognized", "needs_preparation"):
            installation["status"] = status
            with self.assertRaises(ValueError):
                validate(catalog)

    def test_linux_unicode_locations(self):
        request, catalog = deepcopy(self.request), deepcopy(self.catalog)
        request["platform"] = "linux"
        for source, item in zip(request["sources"], catalog["installations"]):
            location = dict(platform="linux", path="/home/jugador/Juegos clásicos/" + source["edition_id"])
            source["location"] = location
            item["locations"][0]["location"] = location
        profile = "/home/jugador/.local/share/titus-engine/data/profile"
        for key, path in [("profile_directory", profile), ("work_directory", profile + "/native/blues-family"),
                          ("session_directory", "/home/jugador/.cache/titus-engine/sessions/session")]:
            request["paths"][key] = dict(platform="linux", path=path)
        validate_launch_catalog(request, catalog)
        self.assertTrue(validate_session(request, self.events, self.result, 0))

    def test_paths_and_settings_cannot_escape_the_contract(self):
        mutations = {
            "future schema": lambda r: r.update(schema_version=2),
            "arbitrary executable": lambda r: r.update(executable="D:\\Games\\game.exe"),
            "relative path": lambda r: r["sources"][0]["location"].update(path="games/blues"),
            "wrong platform": lambda r: r["sources"][0]["location"].update(platform="linux"),
            "original writable": lambda r: r["sources"][0].update(read_only=False),
            "cross-game auxiliary": lambda r: r["sources"][1].update(game_id="fox"),
            "missing primary": lambda r: r["sources"].pop(0),
            "unsupported setting": lambda r: r["settings"].update(zoom_for_fox=4),
            "capability omitted": lambda r: r["required_features"].remove("separate_volumes"),
            "write into originals": lambda r: r["paths"].update(profile_directory=deepcopy(r["sources"][0]["location"])),
            "work outside profile": lambda r: r["paths"]["work_directory"].update(path="C:\\Other"),
            "bad save slot": lambda r: r.update(action=dict(type="load_slot", slot=9)),
        }
        for name, mutate in mutations.items():
            with self.subTest(case=name), self.assertRaises((ValueError, ValidationError)):
                request = deepcopy(self.request); mutate(request); validate(request)
        registry = example("engine-registry")
        registry["engines"][0]["executables"]["windows"] = "../../orig-games/FOX.COM"
        with self.assertRaises(ValidationError):
            validate(registry)

    def test_unknown_fingerprints_profiles_and_save_contexts_are_rejected(self):
        for key, value in [("fingerprint", "e" * 64), ("save_compatibility_id", "wrong.context"),
                           ("status", "unverified")]:
            with self.subTest(field=key), self.assertRaises(ValueError):
                catalog = deepcopy(self.catalog); catalog["installations"][0][key] = value
                validate_launch_catalog(self.request, catalog)
        catalog = deepcopy(self.catalog); catalog["profiles"][0]["id"] = self.request["session_id"]
        with self.assertRaises(ValueError):
            validate_launch_catalog(self.request, catalog)

    def test_no_false_success_on_crash_missing_ready_or_crossed_session(self):
        cases = [(self.events, self.result, 1), (self.events[:2], self.result, 0),
                 ([self.events[0], closing(self.request, 2)], self.result, 0)]
        wrong = deepcopy(self.result); wrong["session_id"] = self.request["profile_id"]
        cases.append((self.events, wrong, 0))
        duplicate = deepcopy(self.events); duplicate[1]["seq"] = 1
        cases.append((duplicate, self.result, 0))
        for events, result, status in cases:
            with self.subTest(status=status, events=len(events)), self.assertRaises(ValueError):
                validate_session(self.request, events, result, status)

    def test_remote_pause_and_live_edition_switch_are_gated(self):
        pause = example("command-return"); pause["command"] = "pause"
        with self.assertRaises(ValueError):
            validate_session(self.request, self.events, self.result, 0, [pause])
        request = deepcopy(self.request); request["sources"].pop()
        with self.assertRaises(ValueError):
            validate_session(request, self.events, self.result, 0)

    def test_native_return_confirmation_can_be_cancelled(self):
        events = self.events[:2]
        for sequence, name, body in [(3, "return_requested", dict(confirmation_required=True)),
                                     (4, "return_cancelled", {}), (5, "closing", {})]:
            events.append(dict(schema_version=1, kind="event", session_id=self.request["session_id"],
                               seq=sequence, event=name, body=body))
        self.assertTrue(validate_session(self.request, events, self.result, 0))

    def test_silent_audio_requires_explicit_policy(self):
        events = deepcopy(self.events); events[1]["body"]["audio_active"] = False
        with self.assertRaises(ValueError):
            validate_session(self.request, events, self.result, 0)
        request = deepcopy(self.request); request["allow_silent_audio"] = True
        self.assertTrue(validate_session(request, events, self.result, 0))

    def test_native_slots_and_legacy_format_are_preserved(self):
        index = example("save-index")
        validate(index)
        legacy = deepcopy(index); legacy["slots"][0].update(format_version=1, has_preview=False)
        validate(legacy)
        invalid = [deepcopy(index) for _ in range(3)]
        invalid[0]["slots"].append(deepcopy(index["slots"][0]))
        invalid[1]["slots"][0]["file"] = "saves/slot02.bbs"
        invalid[2]["slots"][0].update(format_version=1, has_preview=True)
        for document in invalid:
            with self.assertRaises((ValueError, ValidationError)):
                validate(document)

    def test_strict_json(self):
        for text in ['{"kind":"catalog","kind":"launch"}', '{"value":NaN}', '"\\ud800"']:
            with self.assertRaises(ValueError):
                loads(text)

    def test_unreadable_save_has_unknown_metadata_not_invented_values(self):
        index = example("save-index")
        slot = index["slots"][0]
        slot.update(status="unreadable", format_version=None, edition_id=None, resource_fingerprint=None,
                    native_resource_crc32=None, payload_sha256=None, has_preview=False)
        validate(index)
        slot["status"] = "ready"
        with self.assertRaises(ValidationError):
            validate(index)

    def test_fingerprint_serialization_reference_vector(self):
        vector = loads((ROOT / "contracts/fingerprint-v1-vector.json").read_text(encoding="utf-8"))
        encoded = json.dumps(vector["manifest"], sort_keys=True, separators=(",", ":"), ensure_ascii=False)
        self.assertEqual(encoded, vector["canonical_utf8"])
        self.assertEqual(hashlib.sha256(encoded.encode("utf-8")).hexdigest(), vector["sha256"])

    def test_protocol_size_limits(self):
        document = example("result")
        document.update(outcome="runtime_failed", error=dict(code="E_ENGINE_RUNTIME", message="x" * 40000))
        with self.assertRaises(ValueError):
            loads(json.dumps(document))


if __name__ == "__main__":
    unittest.main(verbosity=2)
