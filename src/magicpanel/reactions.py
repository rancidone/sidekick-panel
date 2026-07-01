"""Reaction-persistence primitives shared by all scenes
(docs/00-engine.md: "Reaction persistence model").

Four classes of reaction, selected per rule rather than fixed globally:

- transient: reverts to baseline automatically after a fixed duration.
- sticky: persists until an explicit resolving event clears it.
- duration_bound: active only between a start event and its matching end
  event.
- accumulating: adds to durable state (see state.py) rather than
  reverting; only clears on an explicit reset event.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum, auto

from magicpanel.state import AccumulatingStateStore


class ReactionKind(Enum):
    TRANSIENT = auto()
    STICKY = auto()
    DURATION_BOUND = auto()
    ACCUMULATING = auto()


@dataclass(frozen=True)
class ReactionRule:
    name: str
    kind: ReactionKind
    trigger_event: str

    # transient
    duration_seconds: float | None = None
    # sticky
    resolve_event: str | None = None
    # duration_bound
    end_event: str | None = None
    # accumulating
    reset_event: str | None = None


class ReactionEngine:
    """Tracks which reactions are currently active for a scene, driven by
    incoming events and elapsed time.
    """

    def __init__(
        self,
        rules: list[ReactionRule],
        accumulator: AccumulatingStateStore | None = None,
    ) -> None:
        self._rules = rules
        self._accumulator = accumulator or AccumulatingStateStore()

        self._by_trigger: dict[str, ReactionRule] = {}
        self._by_resolve: dict[str, list[ReactionRule]] = {}
        self._by_end: dict[str, list[ReactionRule]] = {}
        self._by_reset: dict[str, list[ReactionRule]] = {}
        for rule in rules:
            self._by_trigger[rule.trigger_event] = rule
            if rule.resolve_event:
                self._by_resolve.setdefault(rule.resolve_event, []).append(rule)
            if rule.end_event:
                self._by_end.setdefault(rule.end_event, []).append(rule)
            if rule.reset_event:
                self._by_reset.setdefault(rule.reset_event, []).append(rule)

        # active[name] = expiry timestamp (transient) or True (sticky/duration_bound)
        self._active: dict[str, float | bool] = {}
        self._clock: float = 0.0

    def handle_event(self, event_name: str) -> None:
        # Clear sticky reactions this event resolves.
        for rule in self._by_resolve.get(event_name, []):
            self._active.pop(rule.name, None)

        # End duration-bound reactions this event closes.
        for rule in self._by_end.get(event_name, []):
            self._active.pop(rule.name, None)

        # Reset accumulating reactions this event clears.
        for rule in self._by_reset.get(event_name, []):
            self._accumulator.reset(rule.name)

        rule = self._by_trigger.get(event_name)
        if rule is None:
            return

        if rule.kind is ReactionKind.TRANSIENT:
            self._active[rule.name] = self._clock + (rule.duration_seconds or 0.0)
        elif rule.kind in (ReactionKind.STICKY, ReactionKind.DURATION_BOUND):
            self._active[rule.name] = True
        elif rule.kind is ReactionKind.ACCUMULATING:
            self._accumulator.increment(rule.name)

    def tick(self, dt: float) -> None:
        self._clock += dt
        expired = [
            name
            for name, expiry in self._active.items()
            if isinstance(expiry, float) and expiry <= self._clock
        ]
        for name in expired:
            del self._active[name]

    def is_active(self, name: str) -> bool:
        return name in self._active

    def accumulated(self, name: str) -> int:
        return self._accumulator.get(name)
