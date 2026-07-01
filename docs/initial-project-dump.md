# Magic Panel — Initial Project Dump (raw)

This is the original, unedited project brief the developer provided at
project kickoff. It mixes problem framing with a lot of solution/design
detail (hardware specs, architecture sketch, phased plan, protocol names).

It is kept here as reference/background for Design and Build — it is
**not** the canonical problem framing. See `discovery-brief.md` for that.

---

Project: Magic Panel

I want to build an interactive, practical, developer-focused desk companion
using a HUB75 RGB LED matrix panel. The device should react to my workday in
a playful and expressive way, but the exact visual theme and art direction
are not decided yet. We will brainstorm the theme separately.

## Core concept

The panel runs a persistent pixel-art world or visual system. My work events
should not simply replace the screen with plain status text; instead, they
should influence the running visual world. The display should feel alive,
responsive, and useful without becoming distracting.

## Hardware currently known

- RGB LED matrix panel
- Panel type: P4 indoor HUB75-style panel
- Resolution: 64x32 pixels
- Physical module size: 256mm x 128mm
- Scan: 1/16 scan
- Working voltage: DC 5V
- Driver IC marking observed under microscope: DP32019A / 3H0502
- Controller target: Raspberry Pi 4 initially
- Power supply purchased: dedicated 5V supply, approximately 10A class
- Communication target: MacBook to Pi/display over local non-network
  protocol, ideally Bluetooth LE
- USB serial may be tried for development, but corporate security software
  may block or complicate USB device communication, so do not rely
  exclusively on USB
- The Mac is on the corporate network, but the panel/Pi should not join the
  corporate network

## Work environment

- macOS work MacBook
- VS Code
- Claude Code
- Developer workflow involving terminal, git, tests, builds, and local
  coding activity
- Corporate network constraints mean the display should receive only
  sanitized, non-proprietary semantic events
- Do not send source code, filenames, branch names, ticket names, customer
  names, logs, PR text, commit messages, or proprietary text to the panel
- Send high-level events only, such as "typing", "tests_started",
  "tests_passed", "tests_failed", "git_commit", "claude_thinking",
  "claude_done", "meeting_soon", etc.

## High-level architecture

MacBook runs a small local relay app/daemon. The relay observes local
workflow events and emits sanitized semantic events. The relay communicates
with the Raspberry Pi/display using BLE if practical. The Raspberry Pi runs
the Magic Panel engine and owns all rendering. The panel engine maintains
world state, animations, and visual responses.

### Architecture sketch

**MacBook:**
- VS Code / Claude Code / terminal / git / calendar / local scripts
- Local relay daemon
- Event normalization and sanitization
- BLE client or other local transport

**Raspberry Pi:**
- BLE peripheral/server or local receiver
- Event queue
- Magic Panel engine
- HUB75 renderer
- Pixel-art world or visual simulation

**Display:**
- 64x32 HUB75 RGB matrix
- 5V powered directly from PSU
- Pi and panel share ground
- Brightness should be software-limited for comfort, heat, and power

## Design principles

1. This is not a normal status dashboard.
2. It should feel like an interactive desk companion, not just another
   monitor.
3. The visual system should be persistent and alive.
4. Avoid blank screens, harsh mode switches, or boring static status pages.
5. Work events should influence the visual world rather than merely display
   text.
6. Keep the protocol small, scriptable, and extensible.
7. Prefer semantic events over raw data.
8. Make it easy to add new event sources later.
9. Build this like a tiny pixel engine, not a one-off animation script.
10. Support manual triggering from CLI early so animations can be tested
    without full integrations.
11. Do not lock in the visual theme yet; leave room for later art-direction
    brainstorming.

## Possible categories of visual reactions

- Idle / ambient state
- Typing / active coding
- Thinking / waiting
- Tests running
- Tests passed
- Tests failed
- Build running
- Build passed
- Build failed
- Git commit
- Claude Code active
- Meeting soon
- Break mode
- Focus mode
- Celebration
- Warning / error
- Manual easter eggs

## Suggested project phases

**Phase 0: Hardware bring-up**
- Prepare Raspberry Pi 4
- Install and test HUB75 RGB matrix library, likely hzeller/rpi-rgb-led-matrix
- Configure for a 64x32 panel:
  - `--led-rows=32`
  - `--led-cols=64`
  - `--led-chain=1`
  - `--led-parallel=1`
  - `--led-multiplexing=0` initially
  - `--led-slowdown-gpio=2` initially
- Iterate scan/multiplex/panel-type options if output is scrambled
- Start at low brightness
- Verify power wiring and common ground
- Display a basic test pattern
- Then render a simple animated scene or visual loop

**Phase 1: Rendering engine**
- Build a simple 64x32 pixel engine
- Support sprites, background layers, particles, animation timing, palette
  management, and text only as needed
- Maintain a persistent visual state
- Render continuously and consume events asynchronously
- Avoid blocking rendering on communication

**Phase 2: Local event protocol**
- Define a small event protocol
- Start with simple JSON over a local test transport if easiest
- Later optimize to binary BLE messages if desired
- Events should be small and semantic

Example event names: idle, typing_started, typing_stopped, focus_started,
focus_stopped, claude_thinking, claude_done, tests_started, tests_passed,
tests_failed, build_started, build_passed, build_failed, git_commit,
meeting_soon, break_started, break_ended, celebration, warning, error,
custom_effect.

**Phase 3: Mac relay / CLI**
- Build a small macOS command-line tool first
- It should let me manually send events like:
  - `magicpanel idle`
  - `magicpanel tests started`
  - `magicpanel tests passed`
  - `magicpanel tests failed`
  - `magicpanel claude thinking`
  - `magicpanel claude done`
  - `magicpanel git commit`
  - `magicpanel meeting 5`
  - `magicpanel celebration`
  - `magicpanel warning`
- This CLI can later talk to a daemon over a Unix socket
- The daemon handles BLE connection/reconnection to the Pi

**Phase 4: Workflow integrations**
- Claude Code wrapper or hooks:
  - When Claude starts, send claude_thinking
  - When Claude finishes, send claude_done
  - If tool activity can be detected safely, send generic semantic events
    only
- Git:
  - Detect commit events without sending commit message text
- Tests/builds:
  - Provide shell aliases/wrappers so test commands can emit
    tests_started/tests_passed/tests_failed
- VS Code:
  - Optional later extension
  - Not required for MVP
- Calendar:
  - Optional meeting countdown
  - Only send generic "meeting soon" event with minutes remaining

## Initial MVP

1. Pi drives the 64x32 panel.
2. A simple animated idle scene or visual loop runs continuously.
3. A CLI command from the Mac can manually trigger: tests_started,
   tests_passed, tests_failed, claude_thinking, claude_done.
4. The display reacts with distinct animations or state changes.
5. No proprietary data is sent to the panel.

## Potential implementation languages

- Pi renderer: C++, Rust, or Python initially if performance is acceptable
- Prefer a maintainable engine architecture over premature optimization
- Mac relay: Go, Swift, Rust, or Python
- Go is attractive for a small static CLI/daemon
- BLE can be added after the basic renderer and CLI protocol are proven

## Important engineering choices

- Treat the display engine as the source of truth for visuals.
- The Mac should not tell the Pi what to draw pixel-by-pixel.
- The Mac only emits events.
- The Pi decides how those events affect the visual state.
- The rendering loop should be independent from communication.
- Keep event handling asynchronous.
- Build with future extensibility in mind.
- Theme/art direction should remain flexible until we intentionally choose
  it.

## Power notes

- Panel is 5V.
- For a 64x32 P4 panel, expected draw should be manageable with a 5V 10A
  supply.
- Start brightness low.
- Do not power the panel from the Pi.
- Power the panel directly from the 5V PSU.
- Pi and panel must share ground.
- Consider adding an inline fuse and clean power distribution in the final
  enclosure.
