---
status: draft
last_updated: 2026-06-30
---

# Magic Panel — Discovery Brief

## Problem Statement

When working solo and running multiple Claude Code agents concurrently across
split panels, the developer loses a sense of ambient companionship and
activity presence. Existing tooling only reflects state through screen-based
dashboards, which compete for the same visual attention as the work itself
and feel sterile in isolation. There is no glanceable, non-screen presence
that makes solo desk work feel alive or engaging.

Note: the developer already manages awareness across 2-3 concurrent agent
panels directly and does not need a device to disambiguate between them.
This is not a monitoring or dashboard product.

## Intended Outcomes

A physical, non-screen desk companion that:

1. **Provides ambient companionship** during solo work — a persistent,
   always-alive presence rather than a static or blank display.
2. **Reflects an aggregate stream of work activity** (not per-agent detail)
   — a single interactive status stream driven by high-level events like
   tests passing, commits, or Claude Code activity.
3. **Makes work feel engaging through game-like feedback** — both momentary
   celebration (an event produces a satisfying visual reward in the moment)
   and persistent progress (something like a score or streak that
   accumulates and can be looked back on).

**Success signal:** the developer keeps the device running and enjoys it
unprompted, rather than turning it off once the novelty wears off.

## Constraints

- **No proprietary data may leave the Mac.** Source code, filenames, branch
  names, ticket names, customer names, logs, PR text, and commit messages
  must never be sent to the panel — only sanitized, high-level semantic
  events (e.g. "tests_passed", "git_commit", "claude_thinking").
- **The panel/controller must never join the corporate network.** The Mac is
  on a corporate network; the display side must stay air-gapped from it.
- **Local-only communication.** The Mac-to-display link must be a local,
  non-network protocol.
- **Hardware is already fixed** (not open for Discovery): Raspberry Pi 4 as
  controller, 64x32 P4 HUB75 RGB matrix panel, dedicated 5V ~10A power
  supply.

## Assumptions

- BLE is the preferred local transport, but is not assumed reliable or
  guaranteed available; USB serial may be tried for development but
  corporate security software may block or complicate it, so no single
  transport can be relied on exclusively.
- The display side (Pi + engine) is the sole source of visual truth — the
  Mac only ever emits semantic events, never pixel-level instructions.
- Multi-agent disambiguation (representing 2-3 concurrent agent streams as
  visually distinct) is explicitly out of scope for v1; aggregate/global
  state reactions are sufficient.
- Visual theme and art direction are intentionally undecided and deferred to
  a separate brainstorming exercise — not part of this problem framing.
- The feedback system must be able to support both momentary
  (event-triggered) reactions and persistent (accumulating) state,
  extensibly, rather than being locked to only one mode.

## Risks and Edge Cases

- **Distraction creep:** frequent, harsh, or abrupt visual reactions could
  tip the device from "engaging" into "distracting," conflicting with the
  core companionship goal. Transitions should stay smooth and ambient rather
  than jarring.
- **Transport reliability:** BLE pairing/connection loss has no guaranteed
  fallback, since USB is not dependable in the corporate environment either.
- **Novelty decay:** the device may be engaging initially and then get
  ignored or switched off. This is directly addressed by the defined success
  signal above rather than left implicit.

Gamification-induced pressure (e.g. feeling bad about failed tests or low
output days) was raised and explicitly ruled out as a real risk by the
developer — noted here so Design doesn't need to re-litigate it.
