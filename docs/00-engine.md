---
status: draft
last_updated: 2026-07-01
parents:
- discovery-brief.md
---

# Magic Panel Engine v1 — Emulated Display

## Design Unit

The persistent visual engine, character/scene system, and local event
protocol for Magic Panel, running against a software-emulated 128x64 display.
This unit deliberately excludes HUB75/Raspberry Pi hardware bring-up, BLE
transport, and workflow integrations (Claude Code, git, tests) — those are
separate units once this one is solid.

## Problem

Discovery established that Magic Panel needs to feel like a companion with
character, not a dashboard, and that its visual identity is intentionally
undecided at the Discovery stage. Before any hardware work happens, there
needs to be a concrete engine architecture that can actually support
"magical and has character" — persistent scenes, a character presence, and
room for game-like feedback — and that can be iterated on quickly without
waiting on physical hardware bring-up.

## Proposed Solution

**Scene system as the core abstraction.** The engine is built around
multiple composable scenes (visual dioramas), rather than one fixed world.
Each scene can host a persistent character and/or ambient elements (e.g. a
tree-based scene). This resolves the "single character vs. ambient world"
question as "both, via scenes" rather than picking one.

**Composition constraint.** The 128x64 panel is a 2:1 wide aspect ratio.
Scenes must be composed horizontally — a tall, vertically-dominant motif
(e.g. a single centered tree) would leave large dead zones on either side.
Any scene design should treat width as the primary compositional axis.

**Scene lifecycle (v1).** Scene switching is user/CLI-driven in v1 (manual
selection of the active scene). The scene manager is architected so
event-driven switching (e.g. a "celebration" event swapping in a themed
scene) can be added later without restructuring, but v1 only wires up
manual switching.

**Stack: Python, with a swappable rendering-canvas abstraction.**
- Now: a Pygame window stands in for the 128x64 grid.
- Later: the same scene/character/engine code targets
  `hzeller/rpi-rgb-led-matrix`'s Python bindings, the mature de facto driver
  for this HUB75 panel type.
- The engine defines a `Canvas`/output interface; a `PygameCanvas`
  implementation backs it now, an `RGBMatrixCanvas` implementation backs it
  later. Scene and character code never needs to change when the backend
  swaps.
- Rationale: at 128x64 resolution, animation/scene-authoring iteration speed
  matters more than raw performance, and Python has the fastest loop for
  that. See Tradeoffs.

**Local event protocol (v1).** A CLI process writes JSON-line events to a
local Unix domain socket that the engine reads asynchronously (e.g.
`{"event": "tests_passed"}\n`). This is intentionally shaped like the
eventual Mac-to-Pi event stream (small, semantic, line-delimited JSON) so
that swapping the socket for BLE/another local transport later is a
transport-layer change, not a protocol redesign. The engine remains the
sole source of visual truth — the sender only ever emits semantic events,
never pixel-level instructions, matching the boundary set in Discovery.
Delivery is **best-effort, not durable** — if the engine is unreachable
when an event fires (disconnected transport, engine restarting), the event
is simply lost, not queued or replayed. No outbox/retry mechanism is built
on the sender side. This matches the ambient-companion framing from
Discovery over strict accuracy of accumulated progress.

**Accumulating state is durable across restarts (engine-level).** Scene
state built up via the Accumulating reaction class (see below) is
persisted to local storage on the engine side and reloaded on startup —
it survives engine/process restarts and reboots, and is cleared only by
the scene's own explicit reset events (e.g. Arcane Tree's sprint-end
reset), never by a restart. This is what makes "progress that can be
looked back on" (Discovery's outcome) actually hold day to day. Best-effort
event delivery (above) means this state can undercount missed events, but
it does not reset or corrupt because of a restart or reconnect.

**Connection liveness and sleep (engine-level).** The engine owns
connection-liveness tracking, not individual scenes. The sender emits a
periodic heartbeat over the event transport; if the engine receives no
heartbeat or event within a threshold window, it flags the connection as
lost. Scenes read this liveness state to drive their own "away/asleep"
representation (a scene is not required to react to it, but the engine
guarantees the signal exists) — this reuses the existing event transport
rather than introducing a separate presence-detection mechanism, and fits
the eventual Mac↔Pi topology naturally (laptop disconnected = engine goes
quiet).

**Reaction persistence model (engine-level).** Scenes map incoming events to
visual reactions, and not all reactions should behave the same way once
triggered. The engine's event→reaction mapping supports four classes,
selectable per event/reaction rather than fixed globally:

- **Transient** — reverts to the scene's baseline automatically after a
  fixed duration.
- **Sticky** — persists until an explicit resolving event clears it,
  regardless of time elapsed (e.g. an incident reaction that lasts until a
  recovery event, not a timer).
- **Duration-bound** — tied to the active window of a start/end event pair
  (e.g. active only while a build or deploy is in progress), ending when
  the corresponding "done" event arrives.
- **Accumulating** — adds to persistent scene state rather than reverting;
  only clears on an explicit reset event (e.g. growth/decoration that
  builds up over many events and resets at a defined milestone, not per
  reaction).

This is a shared primitive any scene can use for any kind of reaction (mood,
growth, decoration, etc.), not something specific to a single scene's
content. Accumulating is also the engine's answer to the persistent-progress
outcome named in Discovery: progress is scene state that accumulates via
this class, not a literal counter.

**Extensible event vocabulary.** Discovery's example event list is a
starting point, not a fixed set. Scenes may need additional semantic event
names (e.g. `deploy_started`, `production_incident`) as they're designed;
new event names remain subject to Discovery's sanitization constraint
(generic/semantic only, never proprietary content) but are otherwise
freely extensible per scene.

**Eventual topology (context, not this unit's scope).** The engine will
ultimately run on the Raspberry Pi; the macOS laptop will feed it a stream
of events over a local, non-network transport (BLE preferred, per
Discovery). This unit's local-socket protocol is designed to carry over to
that topology with the transport layer swapped out.

## References

- [Discovery Brief](discovery-brief.md)
- [Initial Project Dump](initial-project-dump.md) — raw hardware/protocol
  background; not authoritative for design decisions, used only where it
  informed concrete choices above (e.g. naming `rpi-rgb-led-matrix` as the
  target hardware driver).

## Tradeoffs

- **Python vs. Rust/C++/Go:** Python was chosen over systems languages for
  faster scene/animation-authoring iteration, which is the actual hard
  problem at this resolution (not raw performance). Cost: lower runtime
  performance and weaker typing. Accepted because 128x64 has minimal
  performance demands and this stack has established precedent in the
  HUB75 hobbyist space via `rpi-rgb-led-matrix`'s own Python bindings.
- **Manual (CLI-driven) scene switching vs. event-driven switching now:**
  Building event-driven switching first would require deciding scene
  selection rules before any scenes exist to test against. Manual switching
  lets scene content and the switching mechanism be validated independently
  first. Cost: v1 doesn't yet deliver the "events shape the visual world"
  experience end-to-end; that arrives once event-driven switching is added.
- **Best-effort event delivery vs. a durable outbox/retry queue:** a durable
  queue would make accumulated progress a more accurate reflection of
  actual activity, at the cost of real complexity (local persistence,
  ordering, replay-on-reconnect) on the sender side. Rejected for v1 as
  disproportionate given Discovery's explicit stance against chasing
  numeric accuracy — occasional missed events are acceptable for an
  ambient companion.
- **Local Unix socket vs. building BLE now:** Building against BLE before
  the engine or scenes exist would couple two unproven things together.
  Cost: this unit doesn't prove out the eventual Mac-to-Pi hardware
  transport; that risk is deferred to a future unit.

## Readiness

**Partially ready.** The scene/character architecture, aspect-ratio
constraint, scene-switching model, stack, and event protocol are concrete
enough for implementation to start on this unit.

Both prior open items are now resolved by the child scene designs:

1. **Persistent-progress mechanism** — resolved by the Accumulating
   reaction class above, driven by
   [Scene: Arcane Tree](11-scene-arcane-tree.md)'s growth mechanic.
2. **Concrete scene/character content** — resolved by
   [Scene: Desk Spirit](10-scene-desk-spirit.md) (first scene) and
   [Scene: Arcane Tree](11-scene-arcane-tree.md) (second scene, stubbed
   until its own design pass, now complete).

This unit should **continue** rather than decompose further — it is
cohesive as "engine + emulator" scope. Implementation of the engine
skeleton (canvas abstraction, scene manager, event socket, heartbeat,
reaction-persistence primitives including Accumulating) can proceed against
this document.
