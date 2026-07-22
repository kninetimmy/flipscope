# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

Greenfield. No code exists yet. The single source of truth is
`FlipScope PRD v1_0 unified.md` (v1.0, unified) — read the relevant section
before designing or implementing anything; section references below (§) point
into it. When the PRD and this file disagree, the PRD wins; update this file.

## What FlipScope is

A three-stage educational RF project around a Flipper Zero, explicitly a
**pseudo-SDR** (never call it an SDR or spectrum analyzer):

1. **Stage 1 — Device (FAP):** receive-only Flipper Zero app (C, official
   firmware stable APIs) using the internal CC1101. Swept-RSSI spectrum/
   waterfall/peaks views, single-frequency Activity Monitor, pulse-timing
   capture with known-protocol decode, saved as `.sub` + `.fscope` pairs.
2. **Stage 2 — Host TUI (Rust):** terminal remote head over USB serial.
   The device stays authoritative; the host only sends intent and renders
   state at higher resolution.
3. **Stage 3 — Host DSP engine (Rust):** hand-built timing-domain/envelope
   analysis of captured pulses. A learning curriculum, not a product feature.

## Load-bearing invariants (violating any of these is a design bug)

- **The IQ boundary (§3):** the CC1101 yields only demodulated pulse timings
  and relative RSSI — never IQ. Nothing in any stage may claim, imply, or
  attempt waveform reconstruction, IQ FFT, modulation recovery, or
  wrong-preset repair.
- **Receive-only (§4.3):** no transmit code paths anywhere — device, host, or
  wire protocol. The host→device command set is closed and reviewed against
  this. No replay, playback, brute-force, or jamming features, ever.
- **Evidence preservation (§4.5, FR-029):** original captured timings are
  immutable. Derived/cleaned/analyzed data is written alongside, never over.
- **Honest visualization (§4.1):** every measurement view labels data as
  relative RSSI from a sequential sweep — never a calibrated or simultaneous
  spectrum.
- **No black boxes in DSP (§28):** Stage 3 core stages 1–7 are implemented
  from first principles. External crates only for infrastructure (serde, CLI,
  ratatui) and standard primitives (FFT for autocorrelation only).

## Architecture decisions already made

- **Device (Stage 1):** components per §18 — scanner worker, radio adapter,
  double-buffered sweep buffers, peak detector, capture worker, decoder
  adapter, fake radio backend for tests. Exactly one worker owns the radio at
  a time; UI never blocks on scan/capture. All state Stage 2 will stream must
  flow through **one publication point per stream** (§18) — this seam is the
  only Stage 2 accommodation Stage 1 makes.
- **Host (Stages 2–3):** single Cargo workspace, crates per §25:
  `flipscope-proto`, `flipscope-format`, `flipscope-dsp` (pure, no I/O, no
  threads), `flipscope-link`, `flipscope-tui`, `flipscope-cli`. Linux/Pi 5
  primary; macOS/Windows best-effort.
- **Formats:** `.sub` is standard Flipper RAW (no private fields).
  `.fscope` sidecar uses Flipper File Format (`Key: value` lines) with
  `Filetype: FlipScope Capture Metadata` and integer `Version`. Frequencies
  in Hz, durations in microseconds. The format doc lives in this repo from
  day one.
- **Wire protocol (§26):** line-oriented UTF-8 `Key=value` frames, versioned
  HELLO exchange, device authoritative, sequence numbers for gap detection.

## Milestone order (§33)

D0 (feasibility spike — decoder-API access is the make-or-break question,
answer it first) → H0 (formats + DSP skeleton; **needs no device work, can
start immediately from stock-firmware captures**) → D1–D5 device build →
H1–H6 host build. Don't build the full device UI before D0 resolves.

## Build/test commands

None yet. Device toolchain (ufbt vs. firmware-tree fbt) is chosen at D0;
host is standard Cargo once the workspace exists at H0. Update this section
when either lands. Automated-test expectations are listed in §32.2.

<!-- orchestrator:managed:start version=1 -->
This file is partially managed by Orch (see `.orchestrator/config.toml`).
- In **Assist** mode, tracked-file changes are mechanically denied; a mutating
  request triggers read-only planning instead.
- In **Delivery** mode, work happens in an isolated per-issue worktree, never in
  this checkout directly.
- Model/effort routing, concurrency, and host plugin setup live in
  `.orchestrator/config.toml` — edit that file, not this block.
- Orch upgrades this block through Delivery. Do not hand-edit it; a hand edit
  blocks the next install/upgrade until reverted or removed.
<!-- orchestrator:managed:end -->