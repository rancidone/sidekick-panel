---
status: draft
last_updated: 2026-06-30
parents:
- 00-engine.md
---

# Scene: Desk Spirit

## Design Unit

The first concrete scene for Magic Panel: a single persistent character (a
small spirit living in the display) that reacts to work events through
mood/expression changes. This is the first scene built against the engine
skeleton defined in [00-engine.md](00-engine.md), and the vehicle for
proving out the scene/character abstraction with real content.

Excluded from this unit: the Arcane Tree scene (stubbed only, see below),
and any specific workflow-integration wiring (GitHub/Jenkins/Azure
DevOps/Slack/Teams/etc.) — those are future event *sources* that must still
reduce to the generic semantic events this scene consumes.

## Problem

`00-engine.md` established the scene/character architecture but explicitly
deferred both the first scene's content and the persistent-progress
mechanism. The engine skeleton can't be meaningfully exercised without a
real scene. Desk Spirit was chosen over Arcane Tree as the first scene
because it's structurally simpler (one character, a mood state machine, no
accumulation logic) and proves out the engine's event-to-visual pipeline
fastest.

## Proposed Solution

**Character:** a single spirit sprite with a small set of mood states,
each with its own idle animation loop:

- **Baseline** — default calm/neutral state when connected and no active
  mood.
- **Happy** — builds/tests passing.
- **Casting spells** — while a deploy is in progress (active for the
  duration of the deploy).
- **Breathing fire** — while CI is actively building (active for the
  duration of the build).
- **Angry** — production incident.
- **Sleeping** — no connection to the host (see below).

**Mood persistence, per mood.** Uses the engine-level reaction-persistence
model ([00-engine.md](00-engine.md)) rather than scene-specific logic:

- **Transient:** Happy — a routine pass produces a brief positive reaction,
  then fades back to Baseline.
- **Sticky:** Angry (production incident) — remains until an explicit
  recovery/resolution event is received, not a timer.
- **Duration-bound:** Casting spells (active only while a deploy is in
  progress) and Breathing fire (active only while CI is building) — each
  ends when the corresponding "done" event for that action arrives.
- **Sleeping** uses the engine's connection-liveness signal directly
  (sticky by nature — persists until connectivity resumes); this scene adds
  no presence-detection logic of its own.

Per-mood transient durations are an implementation-time tuning detail, not
an architectural one.

**Event vocabulary needed for this scene** (extends Discovery's example
event list; all remain generic/sanitized per Discovery's constraint):
`build_passed`, `build_failed`, `tests_passed`, `tests_failed`,
`ci_build_started`, `ci_build_finished`, `deploy_started`,
`deploy_finished`, `production_incident`, `incident_resolved`, plus the
heartbeat signal (transport-level, not a semantic work event).

## Arcane Tree (stubbed)

Registered as a second scene for later, not designed further here: a
magical tree that grows over the workday — leaves for commits, flowers for
merged PRs, wilting branches for broken builds, lightning for production
outages, leaf-fall/regrowth at sprint end. This scene is the strongest
candidate for solving the persistent-progress mechanism left open in
`00-engine.md`, since growth naturally accumulates without a literal
counter. Needs its own design pass, including how it resolves the
horizontal-composition constraint for a naturally vertical motif.

## References

- [Engine Design](00-engine.md)
- [Discovery Brief](discovery-brief.md)

## Tradeoffs

- **Using the engine's shared reaction-persistence/heartbeat primitives
  vs. scene-specific logic:** reusing the engine-level mechanisms
  ([00-engine.md](00-engine.md)) means this scene carries no bespoke
  presence-detection or mood-timing code, at the cost of the mood state
  machine being slightly more structured (three reaction classes) than a
  flat per-mood timeout table would be.

## Readiness

**Partially ready.** The mood set, persistence model, and sleep mechanism
are concrete enough to implement against the existing engine skeleton.

Remaining open, deliberately left unresolved:

1. **Exact transient-mood durations** — implementation-time tuning, not an
   architectural gap.
2. **Heartbeat interval and sleep threshold** — needs a concrete number
   once the transport (local socket now, BLE later) is in place; the
   mechanism is decided, the timing constants are not.
3. **Visual/sprite content** — this doc specifies behavior and states, not
   sprite art or animation frames; that's implementation, not design.

This unit should **continue** — it's implementable now. Arcane Tree remains
a stub and should get its own design pass before implementation, given its
tension with the aspect-ratio constraint and its role in resolving the
persistent-progress question.
