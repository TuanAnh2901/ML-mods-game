import importlib.util
import pathlib


MODULE = pathlib.Path(__file__).with_name("bunny_roulette_staging_integrity.py")
SPEC = importlib.util.spec_from_file_location("staging_integrity", MODULE)
ADDON = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ADDON)


BASELINE = {
    "result": {
        "is_black_mark": False,
        "rewards": [{"type": "resource", "id": "qa_item", "quantity": "7"}],
    }
}


def test_observe_is_unchanged():
    changed, fields = ADDON.mutate_payload(BASELINE, "observe")
    assert changed == BASELINE
    assert fields == []


def test_black_mark_fixture_mutation():
    changed, fields = ADDON.mutate_payload(BASELINE, "flip_black_mark")
    assert changed["result"]["is_black_mark"] is True
    assert fields == ["result.is_black_mark"]


def test_reward_fixture_mutation():
    changed, fields = ADDON.mutate_payload(BASELINE, "increment_first_reward")
    assert changed["result"]["rewards"][0]["quantity"] == "8"
    assert fields == ["result.rewards[0].quantity"]


def test_invalid_fixture_mutation():
    changed, fields = ADDON.mutate_payload(BASELINE, "malformed_result")
    assert "rewards" not in changed["result"]
    assert changed["result"]["is_black_mark"] == "qa-invalid-type"
    assert fields == ["result.rewards", "result.is_black_mark"]


if __name__ == "__main__":
    test_observe_is_unchanged()
    test_black_mark_fixture_mutation()
    test_reward_fixture_mutation()
    test_invalid_fixture_mutation()
    print("STAGING_INTEGRITY_TESTS_OK")
