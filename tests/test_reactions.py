from pathlib import Path

import pytest

from magicpanel.reactions import ReactionEngine, ReactionKind, ReactionRule
from magicpanel.state import AccumulatingStateStore


@pytest.fixture
def store(tmp_path: Path) -> AccumulatingStateStore:
    return AccumulatingStateStore(tmp_path / "state.json")


def test_transient_reverts_after_duration(store: AccumulatingStateStore) -> None:
    engine = ReactionEngine(
        [ReactionRule("happy", ReactionKind.TRANSIENT, "tests_passed", duration_seconds=5.0)],
        accumulator=store,
    )
    engine.handle_event("tests_passed")
    assert engine.is_active("happy")

    engine.tick(4.0)
    assert engine.is_active("happy")

    engine.tick(1.5)
    assert not engine.is_active("happy")


def test_sticky_persists_until_resolved(store: AccumulatingStateStore) -> None:
    engine = ReactionEngine(
        [
            ReactionRule(
                "angry",
                ReactionKind.STICKY,
                "production_incident",
                resolve_event="incident_resolved",
            )
        ],
        accumulator=store,
    )
    engine.handle_event("production_incident")
    engine.tick(1000.0)
    assert engine.is_active("angry")

    engine.handle_event("incident_resolved")
    assert not engine.is_active("angry")


def test_duration_bound_active_between_start_and_end(store: AccumulatingStateStore) -> None:
    engine = ReactionEngine(
        [
            ReactionRule(
                "casting_spells",
                ReactionKind.DURATION_BOUND,
                "deploy_started",
                end_event="deploy_finished",
            )
        ],
        accumulator=store,
    )
    assert not engine.is_active("casting_spells")

    engine.handle_event("deploy_started")
    assert engine.is_active("casting_spells")

    engine.handle_event("deploy_finished")
    assert not engine.is_active("casting_spells")


def test_accumulating_grows_and_resets(store: AccumulatingStateStore) -> None:
    engine = ReactionEngine(
        [
            ReactionRule(
                "leaves",
                ReactionKind.ACCUMULATING,
                "git_commit",
                reset_event="sprint_ended",
            )
        ],
        accumulator=store,
    )
    engine.handle_event("git_commit")
    engine.handle_event("git_commit")
    assert engine.accumulated("leaves") == 2

    engine.handle_event("sprint_ended")
    assert engine.accumulated("leaves") == 0


def test_accumulating_state_survives_new_engine_instance(store: AccumulatingStateStore) -> None:
    rules = [ReactionRule("leaves", ReactionKind.ACCUMULATING, "git_commit")]
    engine = ReactionEngine(rules, accumulator=store)
    engine.handle_event("git_commit")

    reloaded_store = AccumulatingStateStore(store._path)
    reloaded_engine = ReactionEngine(rules, accumulator=reloaded_store)
    assert reloaded_engine.accumulated("leaves") == 1
