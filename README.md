# Magic Panel

A physical desk companion built on a 64x32 HUB75 RGB LED matrix. It runs a
persistent, animated visual world that reacts to your workday — commits,
builds, tests, deploys, incidents — through sanitized, high-level events
only. No source code, logs, or proprietary data ever reaches the display.

It's meant to feel like companionship and ambient engagement, not another
status dashboard: a living scene that responds to your work rather than a
screen that reports on it.

## How it's built

- **[Discovery brief](docs/discovery-brief.md)** — the problem, intended
  outcomes, and constraints this project is designed against.
- **[Engine design](docs/00-engine.md)** — the platform: a scene/character
  system, swappable rendering canvas (emulated now, real HUB75 panel
  later), a local event protocol, and shared primitives (connection
  liveness, reaction persistence) that any scene can build on.
- **[Scene: Desk Spirit](docs/10-scene-desk-spirit.md)** — the first scene,
  a single character whose mood reacts to build/test/deploy/incident
  events.
- **[Scene: Arcane Tree](docs/11-scene-arcane-tree.md)** — the second
  scene, a tree that grows over the workday (commits, merged PRs) and
  hosts reactive wildlife.

Current stage: software and an emulated display first, with real HUB75/
Raspberry Pi hardware bring-up as a later phase. See
[docs/initial-project-dump.md](docs/initial-project-dump.md) for the raw
hardware notes and original project sketch.
