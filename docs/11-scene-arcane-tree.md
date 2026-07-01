---
status: draft
last_updated: 2026-06-30
parents:
- 00-engine.md
---

# Scene: Arcane Tree

## Design Unit

The second scene for Magic Panel: a magical tree, as one element within a
wider diorama (not the whole frame), that grows over the workday and hosts
reactive wildlife. Builds on the scene/character architecture and
engine-level primitives defined in [00-engine.md](00-engine.md), and was
stubbed as a placeholder in [10-scene-desk-spirit.md](10-scene-desk-spirit.md).

This scene is also where the persistent-progress mechanism left open since
the first engine design pass gets resolved.

## Problem

`00-engine.md` left the persistent-progress mechanism unresolved, ruling
out only a literal numeric counter. `10-scene-desk-spirit.md` stubbed
Arcane Tree as the candidate for solving it, since growth naturally
accumulates without a number. Separately, a single vertically-dominant tree
motif doesn't suit the panel's 2:1 wide aspect ratio, a constraint
`00-engine.md` already established scenes must respect.

## Proposed Solution

**Composition: tree as one element in a wider scene, not the whole frame.**
The tree occupies part of the width; the rest of the diorama (ground, sky,
space for wildlife) fills out the frame horizontally. This satisfies the
aspect-ratio constraint without forcing the tree itself into an unnatural
shape.

**Tree growth resolves persistent progress, via a new engine-level
reaction class: Accumulating.** `00-engine.md`'s reaction-persistence model
currently has three classes (transient, sticky, duration-bound); none of
them fit "state that builds up over time and only clears on an explicit
reset." This scene requires a fourth: **Accumulating** — a reaction adds to
persistent scene state rather than reverting, and only clears on an
explicit reset event. This class belongs in the engine doc as a shared
primitive (the same pattern used for mood classes in Desk Spirit), not
scene-specific logic, so it will be added there once this scene is agreed.

**Event → tree reaction mapping:**

- `git_commit` → a leaf appears (**accumulating**).
- PR merged → flowers bloom (**accumulating**; note: "PR merged" is not yet
  in the event vocabulary — needs adding, generic/sanitized per Discovery).
- `build_failed` → branches wilt (**sticky** — persists until a
  corresponding `build_passed`/fix event, not a timer; matches the pattern
  used for Angry in Desk Spirit).
- `production_incident` → lightning strikes the tree (**transient** VFX for
  the strike itself) and the tree enters a storm-damaged look (**sticky**,
  cleared by `incident_resolved` — same event already defined for Desk
  Spirit's Angry mood).
- Sprint end (new event, e.g. `sprint_ended`) → leaves fall and the tree
  regrows (**this is the explicit reset event for the accumulating
  state** — the mechanism by which growth doesn't accumulate forever).

**Wildlife reacts to events, layered onto the same scene.** Wildlife
(birds, small creatures) are a second reaction surface within the same
scene, using the same engine-level reaction classes — e.g. creatures
appearing/settling during sustained positive activity, scattering on
`production_incident`, hiding during the storm-damaged state. The specific
roster of creatures and their individual event mappings are implementation
detail (comparable to sprite art in Desk Spirit), not fixed here; the
architectural point is that wildlife consumes the same event stream and
reaction-class model as tree growth, rather than needing its own system.

## References

- [Engine Design](00-engine.md)
- [Scene: Desk Spirit](10-scene-desk-spirit.md) — origin of the stub, and
  the precedent for reusing engine-level reaction classes rather than
  scene-specific logic.
- [Discovery Brief](discovery-brief.md)

## Tradeoffs

- **Tree as one element vs. tree as the whole scene:** keeps the scene
  faithful to the original idea while respecting the aspect-ratio
  constraint, at the cost of needing to design the rest of the diorama
  (ground/sky/wildlife space) rather than just the tree itself.
- **Adding a fourth reaction class (Accumulating) vs. approximating growth
  with Sticky:** Sticky reactions are cleared by a single resolving event
  and don't stack; growth needs to keep adding state across many events
  over time. Modeling it as its own class keeps the engine's reaction model
  honest rather than overloading Sticky to mean two different things.
- **Wildlife sharing the tree's event stream and reaction classes vs. its
  own system:** avoids building a second reaction mechanism for what is
  architecturally the same problem, at the cost of wildlife behavior being
  constrained to whatever the shared reaction-class model supports.

## Readiness

**Partially ready.** The composition approach, the growth mechanism (and
the new Accumulating class it requires), and the event→reaction mapping
are concrete enough to design the engine-level addition and begin scene
implementation.

Remaining open, deliberately left unresolved:

1. **Wildlife roster and individual behaviors** — which creatures, how many,
   and their specific event mappings beyond "reacts to events" are
   implementation-time content decisions, not architectural ones.
2. **New event names** (`sprint_ended`, a PR-merged event) still need to be
   finalized and added to the shared event vocabulary in `00-engine.md`.
3. **Visual/sprite content** — tree growth stages, wildlife sprites, storm
   damage appearance — not specified here, same boundary as Desk Spirit's
   sprite art.

This unit should **continue**. Before implementation starts, `00-engine.md`
needs a small follow-up revision to add the Accumulating reaction class and
the new event names — that update is a direct consequence of this design
and should happen next, not be treated as a new open-ended design pass.
