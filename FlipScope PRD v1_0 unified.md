# FlipScope: Unified Product Requirements

**Document status:** Draft PRD — unified
**Version:** 1.0
**Date:** July 22, 2026
**Supersedes:** FlipScope device PRD v0.3, FlipScope Host PRD v0.2
**Working name:** FlipScope (subject to change)

---

## Part I — Foundations

## 1. Executive summary

FlipScope is a three-stage educational RF project built around a Flipper Zero.

**Stage 1 — Device.** A receive-only Flipper Zero application (FAP) that turns the built-in CC1101 sub-GHz radio and 128×64 display into a handheld RF activity visualizer and signal-capture tool. It sweeps supported frequencies sequentially, samples relative signal strength (RSSI), and presents the results as a spectrum-like trace, monochrome waterfall, peak list, and single-frequency activity graph. From a detected peak it can stop sweeping, lock to that frequency, record demodulated pulse timings, attempt known-protocol decoding, and save the observation for later analysis.

**Stage 2 — Host TUI.** A Rust terminal application that connects to the device over USB serial and acts as a *remote head*: keyboard control plus high-resolution ASCII, block, and braille visualization of the sweep, waterfall, peaks, and captured pulse trains — rendering the same data at a resolution the 128×64 screen cannot reach.

**Stage 3 — Host DSP engine.** A hand-built signal-processing library that analyzes captured pulse timings and RSSI envelopes on the computer, where there is no RAM, CPU, or screen constraint. The device becomes a capture head; the computer does the analysis. This stage exists explicitly to learn how signal processing works by building the stages from first principles.

The product is intentionally described as a **pseudo-SDR**, not a software-defined radio or calibrated spectrum analyzer. It does not capture IQ samples, monitor a wide band simultaneously, or guarantee identification of arbitrary signals. Its purpose is to make sub-GHz radio behavior visible, recordable, and approachable — first with no computer required at all, and later with the computer as an optional analysis partner.

## 2. Problem and opportunity

Flipper Zero can receive supported sub-GHz signals, but its standard workflows center on detecting, decoding, and saving individual transmissions. A learner who wants to explore the surrounding RF environment has no compact view of questions such as:

- Which parts of a supported band are active?
- How does the noise floor change across a band?
- Is an observed signal continuous, periodic, or bursty?
- How does a known remote or sensor appear in frequency and time?
- What effect do scan step size, dwell time, threshold, and peak hold have?

And once a signal *is* captured, a learner has no on-ramp to the next question — what is this pulse train actually made of? — without either an existing decoder matching it or a jump to unfamiliar desktop tooling.

FlipScope addresses both with an educational, standalone visualization tool that favors honest measurements, simple controls, and safe receive-only behavior, plus an optional computer-side companion that turns captures into a signal-processing sandbox.

### 2.1 Prior art and differentiation

Two existing tools already cover parts of the device-side space, and FlipScope must be positioned against them rather than presented as the first of its kind:

- **Stock Frequency Analyzer (official firmware).** Reports the strongest currently detected frequency with an RSSI readout. It answers "what frequency is this transmitter on?" but provides no spectrum view, no history, and no path from detection to capture.
- **Spectrum Analyzer FAP (official app catalog; originally by @jolcese, maintained by @xMasterX and others).** Plots amplitude versus frequency across selectable sub-GHz bands with zoom and span controls. It answers "where is there activity right now?" but offers no waterfall/history, no peak tracking over time, no session persistence, and no capture or decode workflow. Its display also does not disclose the sequential-sweep and relative-RSSI caveats that FlipScope treats as a core principle.

FlipScope's differentiation is therefore **not** the swept-RSSI display itself — that is proven, well-trodden ground, which de-risks Stage 1 considerably. The differentiation is the integrated pipeline and its record-keeping:

1. **Sweep → peak → lock → capture → decode as one workflow.** No existing on-device tool moves from "I see a peak" to "I recorded and attempted to decode it" without leaving one app and reconfiguring another.
2. **Time dimension.** Waterfall history, peak first/last-seen tracking, and the single-frequency Activity Monitor turn a momentary display into an observation record.
3. **Session and capture persistence.** Saved session summaries and paired `.sub`/`.fscope` records make observations comparable across days and locations.
4. **Honest instrumentation as a feature.** Explicit relative-RSSI labeling, visible sweep rate, and pseudo-SDR framing are a deliberate educational stance the incumbents do not take.
5. **A computer-side analysis path** (Stages 2–3) that no existing Flipper sub-GHz visualizer provides.

If official catalog publication becomes a goal (see Open decisions), this positioning is the required story: FlipScope complements the existing Spectrum Analyzer rather than duplicating it.

## 3. The IQ boundary

This is the load-bearing technical constraint for the entire project, across all three stages. Everything else in this document depends on stating it accurately.

**The Flipper does not produce IQ.** The internal CC1101 delivers two kinds of data and no others:

1. **Demodulated pulse timings** — alternating high/low durations in microseconds, produced by the CC1101's own demodulator under a *chosen preset* (OOK/AM or 2-FSK). This is already-decided data: the modulation type, bandwidth, and deviation were fixed at capture time and cannot be changed afterward.
2. **RSSI** — a relative received-signal-strength reading, sampled over frequency (sweeps) or over time (Activity Monitor).

Consequently, neither the device nor the host can:

- Reconstruct the original RF waveform
- Recover a modulation discarded by the CC1101 demodulator
- Perform IQ FFT, matched filtering, demodulation, or constellation analysis
- Repair a capture made under the wrong preset
- Decrypt or defeat rolling-code security

The signal processing in Stage 3 is therefore **timing-domain and envelope-domain DSP**, not IQ-domain DSP. That is not a downgrade — it is the less-tooled corner of the field where protocol reverse-engineering actually lives: pulse clustering, symbol quantization, line-code classification, framing and sync detection, repeat detection, entropy estimation, and cross-capture diffing.

For IQ-domain work, the right tools are dedicated SDR hardware (RTL-SDR, HackRF, KrakenSDR) with an entirely different pipeline. FlipScope is deliberately the *demodulated-pulse* companion to that gear, not a replacement for it.

## 4. Product principles

### 4.1 Honest visualization

Interfaces must label readings as relative RSSI and make the sequential nature of the sweep visible. They must not imply a simultaneous or calibrated spectrum measurement. This applies equally to the device screen and the terminal.

### 4.2 Standalone first

All Stage 1 features must work with only a Flipper Zero and, for saving, a microSD card. The host application is an accessory that may be added later and removed at any time; pulling the cable mid-session must never disrupt the device.

### 4.3 Receive-only by design

No stage contains a user-facing transmit action, and transmit code paths are avoided entirely. Radio state must return safely to idle when scanning stops, the app exits, or an error occurs. The host wire protocol contains no command capable of initiating transmission — a reviewed invariant, not an accident of the current command list.

### 4.4 Useful defaults, visible tradeoffs

Users choose simple scan profiles — Fast, Balanced, Sensitive — rather than configuring low-level timing first. The UI shows how each profile affects sweep speed and sensitivity.

### 4.5 Evidence preservation

Original captured timings are immutable. Every derived, cleaned, normalized, or analyzed representation is written *alongside* the original, never over it. This holds on-device and on the host.

### 4.6 Learn by building

Stage 3's core analysis stages are implemented from first principles rather than delegated to a DSP framework. Every stage exposes its intermediate output for visualization, because a histogram you can look at teaches more than a returned number. This is a design constraint, not a preference — see §14.

## 5. Target user

The primary user is a technically curious Flipper owner learning embedded development, radio fundamentals, and — in Stage 3 — signal processing. They may have remotes, wireless sensors, or other devices they own and are authorized to test, but they do not need prior RF engineering experience.

## 6. Project-wide non-goals

Across all stages, FlipScope will not provide:

- IQ sample capture, processing, or waveform reconstruction
- Simultaneous wideband reception
- Calibrated power measurements or laboratory-grade frequency analysis
- Arbitrary modulation recognition or universal signal decoding
- Audio demodulation or listening modes
- Signal transmission, replay, jamming, brute forcing, or transmit controls of any kind
- Decryption, rolling-code defeat, or treating protocol identification as cloning capability
- Automatic claims that a peak represents a particular device or protocol
- Recovery from a recording made with an incorrect frequency, modulation, bandwidth, or preset
- A required computer for any device-side functionality
- A graphical desktop application (the terminal UI is a deliberate choice)

## 7. Hardware and platform constraints

- The internal radio is a CC1101 transceiver, not a wideband IQ receiver.
- Frequencies must remain within the device's supported sub-GHz ranges and applicable regional restrictions.
- A sweep is produced by retuning the radio, waiting briefly, sampling RSSI, and repeating. Short bursts may occur between samples and be missed.
- RSSI should be treated as relative and indicative, not calibrated absolute power.
- The device display is 128×64 monochrome, so plots must be legible without color.
- Device CPU time, RAM, battery, and microSD writes are constrained.
- The app must coexist safely with the firmware's ownership of the sub-GHz radio.
- Host-side targets Linux (including Raspberry Pi 5) as primary, with macOS and Windows best-effort.

Initial presets should be region-aware and validated at runtime. Candidate learning presets include narrow spans around commonly used 315 MHz, 433.92 MHz, and 915 MHz areas where supported by the specific device and region. The application must not assume that every candidate preset is valid everywhere.

---

## Part II — Stage 1: Device application

## 8. Stage 1 goals

1. Visualize relative RF activity across supported sub-GHz spans using the internal CC1101.
2. Operate entirely on the Flipper Zero during normal use.
3. Teach practical concepts: tuning, RSSI, noise floor, sweep rate, resolution, burst timing, and peak detection.
4. Make common exploration tasks usable on the 128×64 monochrome screen.
5. Save compact session summaries and detected peaks to microSD for later comparison.
6. Capture demodulated pulse timings from selected peaks and preserve them in a documented, reusable format.
7. Attempt immediate decoding through supported protocol decoders while preserving unknown signals for later analysis.
8. Ship as a Flipper application package (FAP) on official firmware if the required stable APIs are available.
9. Guarantee receive-only behavior throughout the application.

## 9. Core use cases

1. **Find activity:** Sweep a supported preset and see where relative RSSI rises above the local noise floor.
2. **Observe a known device:** Press a personally owned remote and watch its transmission create a peak.
3. **Study timing:** Lock onto one frequency and observe whether activity is periodic, continuous, or bursty.
4. **Compare scan settings:** See the tradeoff between faster sweeps, finer frequency steps, and more sensitive sampling.
5. **Record observations:** Save the span, profile, duration, noise estimate, and detected peaks as a session summary.
6. **Inspect a peak:** Pause the sweep, move a cursor to a peak, and open the single-frequency activity view.
7. **Capture a signal:** Lock onto a selected peak, choose a modulation preset, and record one or more pulse bursts.
8. **Decode now:** Run captured pulses through supported protocol decoders and inspect any confident match.
9. **Decode later:** Save an unknown signal as a standard RAW `.sub` file with FlipScope metadata for later analysis.

## 10. Stage 1 scope

### 10.1 MVP — P0

| Feature | Description |
|---|---|
| Preset selection | Region-valid learning presets plus a manually configured start/end frequency and step size |
| Spectrum Sweep | Current sweep rendered as relative RSSI versus frequency |
| Monochrome Waterfall | Scrolling history of completed sweeps using ordered dithering or density levels |
| Peak detection | Identify and cluster bins above an adaptive threshold |
| Peak hold | Retain recent maxima long enough to reveal brief transmissions |
| Peak list | Frequency, relative RSSI, first/last seen, and observation count |
| Activity Monitor | RSSI-versus-time graph at one selected frequency |
| Scan profiles | Fast, Balanced, and Sensitive presets |
| Pause and inspect | Freeze the current display, move a cursor, and open a selected frequency |
| Session summary | Save settings, duration, noise estimate, and peaks to microSD |
| Peak Capture | Stop sweeping, lock to a selected frequency, and record threshold-triggered pulse timings |
| Capture presets | Manually select a supported OOK/AM or 2-FSK preset before recording |
| Known-protocol decode | Feed captured pulse timings to supported firmware decoders and report confident matches |
| RAW preservation | Save unknown or user-requested captures as standard RAW `.sub` files |
| Capture metadata | Save a paired `.fscope` record containing detection and recording context |
| Receive-only enforcement | No transmission controls or transmission workflow |
| Graceful cleanup | Release radio resources and restore a safe state on exit or error |

### 10.2 Follow-up — P1

- Burst-duration and activity-event summaries in single-frequency mode
- User favorites and named presets
- Automatic zoom around a persistent peak
- Automatic preset probing and pulse-quality ranking
- On-device pulse-timing inspection, repeat detection, and simple timing histograms
- Derived BinRAW generation while retaining the original RAW evidence
- Session reopening and comparison on the Flipper

## 11. Primary user flow

1. Launch FlipScope.
2. Choose **Quick Scan**, **Presets**, **Manual Span**, **History**, or **Settings**.
3. Select or configure a valid frequency span and scan profile.
4. Start the spectrum view.
5. Observe the current trace, peak hold, and sweep-rate indicator.
6. Switch between Trace, Waterfall, and Peaks.
7. Pause, move the cursor, and open Activity Monitor at a selected frequency.
8. Choose **Capture** to stop sweeping and lock the receiver to the selected frequency.
9. Select a modulation preset, arm the RSSI trigger, and activate a personally owned or authorized device.
10. Review a known-protocol result or save the pulse stream as RAW with its FlipScope metadata.
11. Resume scanning or save the broader session summary.
12. Exit; the application returns the radio to idle and releases all resources.

## 12. Interaction model

Button behavior may be refined during usability testing, but the MVP baseline is:

| Input | Spectrum/Waterfall | Activity Monitor |
|---|---|---|
| Left / Right | Move frequency cursor while paused; change view while running | Change time scale |
| Up / Down | Cycle detected peaks or adjust display scale | Adjust event threshold |
| OK | Pause or resume | Place an observation marker |
| Long OK | Open selected-frequency action menu | Open action menu |
| Back | Return to scan menu | Return to previous view |
| Long Back | Stop scan and return home | Stop monitor and return home |

A short contextual help screen must be available from each primary view. Destructive or ambiguous button chords should be avoided.

## 13. Device screen specifications

### 13.1 Spectrum Sweep

- Header: preset/span label, run/pause state, and approximate sweeps per second
- Main plot: current RSSI trace with a distinct peak-hold overlay
- Cursor: vertical marker shown when paused
- Footer: cursor frequency and relative RSSI, or current threshold while running
- Optional indicators: SD recording state and radio/API warning

### 13.2 Waterfall

- Horizontal axis: frequency bins
- Vertical axis: recent completed sweeps, newest at the bottom
- Intensity: four or five monochrome density levels implemented with dithering
- Header/footer: same span, profile, and cursor information as Spectrum Sweep

### 13.3 Peaks

- Scrollable rows containing frequency, strongest observed RSSI, and age
- Selecting a row opens Activity Monitor
- Adjacent above-threshold bins should appear as one clustered peak rather than separate entries

### 13.4 Activity Monitor

- Fixed tuned frequency shown in the header
- Scrolling RSSI-versus-time trace
- Threshold line and burst/event count
- Minimum, maximum, and rolling noise estimate
- Clear indication that the radio is monitoring one frequency rather than sweeping

### 13.5 Capture and Decode

- Header: locked frequency, selected preset, armed/recording state, and RSSI
- Trigger control: adjustable RSSI threshold with manual-start option
- Capture summary: duration, pulse count, burst count, and repetition estimate
- Decode result: protocol, bit length, relevant decoded fields, and confidence/repeat count when available
- Unknown result: clear **No known decoder matched** state with **Save RAW** action
- Preset warning: the saved pulse data is valid only for the frequency and radio preset used during capture
- Save result: paired `.sub` and `.fscope` filenames, or a clear storage error

## 14. Device functional requirements

| ID | Requirement |
|---|---|
| FR-001 | The app shall initialize the internal sub-GHz radio without transmitting. |
| FR-002 | The app shall reject or adjust spans outside supported device and regional ranges. |
| FR-003 | The app shall sequentially tune and sample RSSI across a configured span. |
| FR-004 | The app shall expose Fast, Balanced, and Sensitive scan profiles. |
| FR-005 | The app shall render each completed sweep as a spectrum-like trace. |
| FR-006 | The app shall display the approximate completed sweep rate. |
| FR-007 | The app shall maintain a configurable peak-hold value with decay. |
| FR-008 | The app shall calculate a rolling noise-floor estimate and an adjustable detection margin. |
| FR-009 | The app shall cluster adjacent above-threshold bins into a single peak. |
| FR-010 | The app shall render recent sweep history as a monochrome waterfall. |
| FR-011 | The user shall be able to pause a sweep and inspect individual bins. |
| FR-012 | The user shall be able to monitor RSSI over time at a selected frequency. |
| FR-013 | The app shall maintain a session peak list with first seen, last seen, count, and maximum RSSI. |
| FR-014 | The app shall save a compact session summary when microSD storage is available. |
| FR-015 | The app shall remain usable without microSD and clearly disable saving. |
| FR-016 | The app shall release radio, worker, timer, and storage resources on every exit path. |
| FR-017 | The app shall not expose transmit, replay, brute-force, or jamming functionality. |
| FR-018 | The app shall disclose that readings are swept and relative rather than simultaneous and calibrated. |
| FR-019 | The user shall be able to stop a sweep and lock the receiver to a selected peak frequency. |
| FR-020 | The user shall select a supported radio preset before recording pulse data. |
| FR-021 | Capture shall begin manually or when RSSI exceeds an adjustable threshold. |
| FR-022 | The app shall record demodulated high/low durations with microsecond timing for a bounded capture window. |
| FR-023 | The app shall feed a captured pulse stream to available supported protocol decoders when the required stable APIs are accessible. |
| FR-024 | A decoder result shall be reported only after its validation conditions are met, such as a complete frame and consistent repeats. |
| FR-025 | If no decoder matches, the app shall label the capture unknown without guessing a device identity. |
| FR-026 | The app shall save requested unknown captures as documented RAW `.sub` files containing frequency, preset, and signed pulse timings. |
| FR-027 | The app shall save a paired `.fscope` metadata record containing sweep, trigger, RSSI, timing, and decoder context. |
| FR-028 | Capture files shall remain receive-only inside FlipScope; the app shall not provide playback or transmission actions. |
| FR-029 | The app shall preserve the original RAW capture when creating any cleaned, normalized, or derived representation. |
| FR-030 | The app shall warn that an incorrect modulation preset cannot reliably be corrected after capture because no IQ waveform is stored. |
| FR-031 | RSSI-triggered capture shall either pre-arm with a ring buffer so pulses preceding the trigger crossing are retained, or clearly disclose in the capture UI that the initial burst may be truncated. |

## 15. Scan profiles

Exact timing values must be determined experimentally in the technical spike. Product-level behavior is:

| Profile | Intended behavior | Tradeoff |
|---|---|---|
| Fast | Fewer samples and/or coarser bins | Better chance of revisiting frequencies quickly; noisier readings |
| Balanced | Default bin count and sample aggregation | General exploration |
| Sensitive | More settling/samples and optionally finer bins | Slower sweep; improved stability and ability to see weak persistent activity |

The first prototype should target at least two completed display sweeps per second for a roughly screen-width span in Balanced mode, but this is a validation target rather than a committed hardware guarantee. CC1101 retune plus PLL settling plus RSSI validity is on the order of low single-digit milliseconds per bin, which makes the Balanced target plausible for ~128 bins; however, the Sensitive profile's multi-sample aggregation and longer settling will fall well below it by design. That is acceptable: the always-visible sweep-rate indicator exists precisely so the user sees the speed cost of sensitivity rather than being surprised by it. Do not distort the Sensitive profile to chase the Balanced-mode target.

## 16. Sweep signal-processing approach

For each frequency bin:

1. Validate the requested frequency.
2. Tune the radio.
3. Wait for the profile-specific settling interval.
4. Take one or more RSSI readings.
5. Aggregate readings using a robust statistic such as median, trimmed mean, or maximum depending on the profile.
6. Update the current sweep buffer.
7. Update the rolling noise estimate and peak detector.

At the end of a sweep:

1. Atomically swap the completed and display buffers.
2. Add a downsampled row to the waterfall history.
3. Cluster adjacent above-threshold bins.
4. Update session peaks and peak-hold decay.
5. Begin the next sweep.

The initial threshold model should be `rolling noise estimate + user margin`. The UI should avoid presenting this as a calibrated dBm threshold until hardware behavior and API semantics are validated.

## 17. Capture and decoding approach

Detection and decoding are separate stages. A spectrum sweep measures RSSI only and therefore cannot recover message data. To capture a candidate signal, FlipScope must stop sweeping and dedicate the receiver to one frequency and one radio preset.

### 17.1 Capture pipeline

1. The user selects a detected peak or a fixed frequency.
2. FlipScope stops the sweep and waits for the scanner worker to release radio ownership.
3. The capture worker tunes to the selected frequency and applies the user-selected preset.
4. The user starts capture manually or arms an RSSI threshold trigger.
5. The receiver records alternating high/low durations in microseconds for a bounded time or pulse count.
6. The untouched pulse stream is retained as the source capture.
7. The stream is offered to available known-protocol decoders.
8. A confident match produces a decoded observation; otherwise the result remains **Unknown**.
9. On request, FlipScope saves a standard RAW `.sub` file and a paired `.fscope` metadata record.
10. Radio ownership returns to Activity Monitor, the spectrum scanner, or idle according to the user's next action.

**Trigger latency and first-burst loss.** An RSSI-threshold trigger cannot capture the opening pulses of the burst that trips it: by the time RSSI is sampled, compared, and the capture path reacts, those pulses are gone. For repeating transmitters (most remotes and sensors) this is harmless — subsequent frames are captured intact — but a genuinely single-shot transmission may be recorded incomplete or missed. The MVP must state this behavior in the capture UI help rather than hide it. The correct fix, if validated as affordable in the spike, is to pre-arm: enter asynchronous receive immediately on arming, continuously record into a small ring buffer, and discard data until the trigger condition is met, so the buffer already contains the pulses that preceded the trigger crossing. Pre-arm capture is the preferred implementation if RAM and the async-RX API permit it; the document-the-limitation approach is the fallback, not the goal.

### 17.2 Modulation and preset handling

MVP capture requires an explicit preset selection. Initial choices should map to supported stock presets such as OOK/AM and 2-FSK configurations. The UI must explain that RAW data is already demodulated according to this selection; it is not an IQ recording. Choosing the wrong modulation, deviation, bandwidth, or frequency may produce meaningless timings that cannot be repaired offline.

Automatic preset probing is a P1 feature. A future implementation may listen briefly under several presets and rank them using edge count, timing stability, repeatability, and signal-to-noise behavior. It must present the outcome as a suggestion, because changing presets consumes time and can miss short bursts.

### 17.3 Known-protocol results

When a supported decoder reports a complete, valid frame, FlipScope may show:

- Protocol name
- Frequency and radio preset
- Bit length and decoded payload fields exposed by the decoder
- Timing element or symbol period when available
- Repeat count and first/last observation times

The app must distinguish **protocol identified**, **frame decoded**, and **security defeated**. The latter is never claimed. Dynamic or encrypted protocols may be identifiable while their security properties remain intact. FlipScope saves received observations but provides no replay action.

### 17.4 Unknown and later decoding

Unknown captures are valuable evidence, not failures. The original RAW `.sub` file preserves signed pulse durations together with the frequency and CC1101 preset. This is precisely the input Stage 3 consumes.

Offline analysis cannot reconstruct the original RF waveform, recover a modulation discarded during demodulation, or decrypt a protected protocol merely from pulse timings.

## 18. Device architecture

| Component | Responsibility |
|---|---|
| UI and view dispatcher | Menus, plots, button handling, dialogs, contextual help |
| Scanner worker | Radio tuning, settling, sampling, and completed-sweep publication |
| Radio adapter | Small abstraction over stable firmware sub-GHz device APIs |
| Sweep buffers | Double-buffered current/display data without UI-thread contention |
| Peak detector | Noise estimation, thresholding, clustering, peak hold, and session statistics |
| Activity monitor | Fixed-frequency sampling and event timing |
| Capture worker | Exclusive fixed-frequency pulse acquisition, trigger handling, and capture bounds |
| Decoder adapter | Feeds pulses into available supported decoders and normalizes results |
| RAW writer | Writes standards-compatible RAW `.sub` files without modifying source timings |
| Capture metadata writer | Writes paired `.fscope` context and decoder-attempt records |
| Session recorder | Buffered, compact persistence to microSD |
| Settings store | Presets, scan profile, display options, and thresholds |
| Fake radio backend | Deterministic development and unit-test signal patterns |

The UI must never block on a full scan or capture. Radio access should be owned by exactly one worker at a time, with explicit transitions among scanning, fixed-frequency monitoring, armed capture, recording, decoding, and idle states. A capture transition must wait for positive scanner shutdown rather than allowing concurrent access.

**Forward-compatibility constraint (single publication point).** All state that Stage 2 will need to stream — completed sweep buffers, peak-table updates, capture events, and decoder results — must flow through one internal publication point per stream rather than being pushed directly into UI widgets from multiple call sites. The double-buffered sweep swap (§16) already provides this for sweeps; the peak table and capture state machine should follow the same discipline. This is the **only** change Stage 1 must make to keep Stage 2 cheap: adding "also serialize this to serial" then becomes a small tap at each publication point instead of a refactor. No serialization code ships in Stage 1 — only the clean seam.

## 19. Storage model

Proposed application data location:

```text
/ext/apps_data/flipscope/
  config.ff
  presets/
  sessions/
  captures/
```

An MVP session stores:

- App and file-format versions
- Timestamp and duration
- Preset name or manual span
- Start/end frequency, step size, and bin count
- Scan profile
- Observed minimum/maximum RSSI and rolling noise estimate
- Peak records: center frequency, bandwidth in bins, first seen, last seen, observation count, and maximum RSSI
- Optional periodic downsampled snapshots, disabled by default if write volume is excessive

Each saved capture consists of:

```text
captures/capture_YYYYMMDD_HHMMSS.sub
captures/capture_YYYYMMDD_HHMMSS.fscope
```

The `.sub` file follows the documented Flipper RAW format and contains the file version, tuned frequency, radio preset, `Protocol: RAW`, and signed `RAW_Data` timings in microseconds. FlipScope does not add private fields to the standard file.

**`.fscope` format.** The metadata file shall use the Flipper File Format (the same `Key: value` line-oriented text format used by `.sub` and other stock files), with a `Filetype: FlipScope Capture Metadata` header and an integer `Version` field. This choice is deliberate: it reuses the firmware's existing FlipperFormat read/write APIs on-device (no custom parser, no JSON library), remains trivially parseable on the host side, and is human-readable when inspected over qFlipper or a card reader. The format must be documented in the repository from day one so the Stage 3 analyzer never has to reverse-engineer an ad-hoc layout. Field names, units (frequencies in Hz, durations in microseconds), and version-migration rules are part of that document.

The paired `.fscope` metadata contains:

- FlipScope metadata-format version and capture identifier
- Detection time, capture time, and optional user label
- Source session and peak identifiers
- Original sweep span, step size, profile, peak RSSI, and noise estimate
- Locked frequency, selected preset, trigger mode, and threshold
- Capture duration, pulse count, burst count, and truncation status
- Decoder names attempted, result state, and normalized decoded fields
- Hash or other correlation value for the paired RAW file if affordable on target hardware

Parsed decoder results remain observation records inside FlipScope. The MVP always permits preserving the original RAW pulse evidence even when a decoder matches.

The recorder should buffer writes and prioritize summaries over logging every sample. A missing, full, or removed microSD card must not crash or stop live visualization.

## 20. Device non-functional requirements

- **Responsiveness:** Button input and view changes should remain responsive while scanning.
- **Startup:** The main menu should appear within three seconds under normal conditions.
- **Stability:** A 30-minute continuous scan must complete without a crash, deadlock, or unbounded memory growth.
- **Memory discipline:** Avoid per-bin and per-frame heap allocation after scanning begins.
- **Power awareness:** Show a warning or reduce refresh work if sensitive scanning creates unacceptable battery drain or heat during testing.
- **Storage safety:** Buffer writes, handle I/O errors, and never corrupt configuration when a save fails.
- **Radio safety:** Always stop receive activity and release radio ownership on exit, error, or app termination.
- **Clarity:** Every measurement view must identify the data as relative RSSI and the visualization as a sequential sweep.
- **Compatibility:** The preferred implementation targets stable official firmware application APIs and avoids a firmware fork unless a formal feasibility decision changes this constraint.
- **Capture integrity:** Decoder preprocessing must operate on a copy or derived representation and never overwrite the original pulse timings.
- **Bounded recording:** Pulse buffers, capture duration, and file writes must have explicit limits with a visible truncated-capture state.

## 21. Device error handling

The app must provide actionable messages for:

- Radio unavailable or already in use
- Unsupported or region-invalid frequency/span
- Failure to tune or sample RSSI
- Worker timeout or unexpected radio state
- Missing, removed, full, or unwritable microSD
- Invalid or incompatible saved configuration/session
- Invalid capture preset or failure to enter asynchronous receive mode
- Capture buffer full, timing overrun, or pulse stream truncated
- No known decoder match, which is an ordinary result rather than a fatal error
- RAW or metadata file partially written

On a radio error, the application should stop scanning, attempt safe cleanup, preserve any already buffered session summary when possible, and return the user to a recoverable screen.

## 22. Stage 1 acceptance criteria

Stage 1 is complete when all of the following are demonstrated on target hardware:

1. The app installs and runs as a standalone FAP on the selected official firmware baseline.
2. A user can select a valid preset and see a continuously updating swept RSSI trace.
3. Activating a personally owned, compatible sub-GHz device produces a visible and repeatable peak under suitable conditions.
4. The app displays approximate sweep rate, frequency span, profile, and relative RSSI labeling.
5. The user can pause, move a cursor, and open Activity Monitor at a selected frequency.
6. The waterfall scrolls completed sweeps using legible monochrome intensity levels.
7. Adjacent active bins are clustered into useful peak records.
8. The user can lock onto a selected peak, choose a preset, and record a bounded pulse stream.
9. At least one supported known protocol can be decoded from a personally owned or authorized test device under suitable conditions.
10. An unmatched signal is labeled **Unknown** without an invented identity or protocol.
11. A requested capture is saved as a valid RAW `.sub` file with a paired `.fscope` metadata record.
12. The saved RAW file can be parsed by an independent format-validation test and contains the selected frequency, preset, and original signed timings.
13. Decoder preprocessing and derived analysis do not alter the saved original RAW timings.
14. A session summary can be saved and reopened when microSD is present.
15. The app remains functional without microSD, with saving clearly unavailable.
16. No application screen or code path initiates transmission.
17. Exiting from scanning, monitoring, armed capture, and active recording returns the radio to a safe idle state.
18. A 30-minute continuous Balanced scan completes without a crash or unbounded memory growth.

---

## Part III — Stage 2: Host terminal application

## 23. Stage 2 goals and relationship to the device

The host application is a **remote head**: a view and a keyboard, nothing more. The scanner worker, peak detector, capture state machine, and file writing all stay on the Flipper exactly as specified in Part II. The host sends the same commands the D-pad would and receives a stream of state.

| Concern | Device (Stage 1) | Host (Stages 2–3) |
|---|---|---|
| Radio ownership and safety | Owns it entirely; receive-only enforced | Never touches radio directly; sends intent only |
| Scanner, peak detector, capture state machine | Authoritative | Mirrors state for display; does not re-implement |
| File writing (`.sub`, `.fscope`, sessions) | Authoritative | Reads copies; may request saves; writes its own analysis artifacts |
| Standalone operation | Required; host is never a dependency | Optional accessory |
| Analysis depth | Constrained (screen, RAM, CPU, battery) | Unconstrained — this is the point |
| Device changes required | One publication seam (§18) | — |

Goals:

1. Provide a keyboard-driven remote head over USB serial.
2. Render the sweep, waterfall, peak list, and pulse captures at a resolution the device screen cannot reach.
3. Read and write the device's `.sub` and `.fscope` formats without modifying originals.
4. Act as an unbounded capture sink for streamed pulse data.
5. Produce a single static binary; Linux and Raspberry Pi 5 primary.
6. Remain fully usable over SSH and tmux.

## 24. Operating modes

Three modes from one codebase, differing only in where data comes from:

1. **Remote head (live control).** Cable connected, Flipper scanning/capturing. The TUI renders live state; the keyboard drives the device.
2. **Headless batch (offline analysis).** No device required. Point the host at saved `.sub` + `.fscope` files and run the Stage 3 pipeline to produce reports. **This mode needs no serial link and no device changes, so it can be built first.**
3. **Live offload (streamed capture + real-time analysis).** Cable connected, Flipper streaming pulse captures as they happen. The host acts as an **unbounded sink** — recording captures longer than device RAM could hold — and runs the DSP engine in real time.

## 25. Package architecture

A single Cargo workspace. Rust is chosen for the single static binary, strong guarantees on state-machine and parsing code, and continuity with the developer's current trajectory.

| Crate | Responsibility | I/O? |
|---|---|---|
| `flipscope-proto` | Wire protocol types and (de)serialization; shared frame definitions and version constant | No |
| `flipscope-format` | `.sub` and `.fscope` read/write; RAW timing model; preserves original timings immutably | Files only |
| `flipscope-dsp` | The Stage 3 signal-processing engine. Pure functions over data; no I/O, no threads | No |
| `flipscope-link` | USB CDC serial transport; framing, reconnect, backpressure | Serial |
| `flipscope-tui` | ratatui front end: rendering, keymap, mode wiring | Terminal |
| `flipscope-cli` | Headless batch entry point over `flipscope-format` + `flipscope-dsp` | Files/stdout |

`flipscope-dsp` having **no I/O and no device dependency** is the key architectural decision: it is developed and tested against synthetic and recorded pulse streams with zero hardware in the loop, which is what makes Stage 3 a viable sandbox.

**Suggested tooling** (verify at spike time, not committed): `ratatui` for the TUI with its canvas/braille marker; `serialport` for CDC; `serde` for protocol and report serialization; an FFT crate **only** for autocorrelation on RSSI time series. The pulse-domain analysis is mostly clustering and sequence algorithms needing little beyond the standard library — resist heavy DSP crates it does not need (see §14 principle 4.6 and §31).

## 26. Wire protocol

### 26.1 Principles

- **Line-oriented UTF-8 text frames**, one record per line, `Key=value` fields — consistent with the FlipperFormat aesthetic already chosen for `.fscope`. Debuggable in a bare terminal, versionable, greppable in a log.
- **Versioned.** The first exchange negotiates a protocol version; mismatches degrade gracefully or refuse cleanly.
- **Device is authoritative.** Host frames express *intent*; the device confirms via state frames. The host never assumes a command took effect until echoed.
- **Receive-only invariant preserved at the protocol layer.** No command in the protocol initiates transmission. The command set is closed and reviewed against this.
- **Bandwidth is trivial.** 128 bins × 10 sweeps/s is a few KB/s; text framing overhead is irrelevant.

### 26.2 Device → host frames (illustrative)

```text
HELLO fw=<baseline> proto=1 dev=flipscope
SWEEP seq=1421 f0=433050000 f1=434790000 bins=128 fmt=b64 data=<base64 int8 relative>
NOISE seq=1421 est=-96
PEAK id=7 f=433920000 rssi=-58 first=1421 last=1440 count=19 bw=3
STATE mode=scanning profile=balanced paused=0
CAPSTART id=3 f=433920000 preset=AM650
CAPPULSE id=3 seq=0 fmt=b64 data=<base64 signed varint microsecond durations>
CAPEND id=3 pulses=214 bursts=3 truncated=0
DECODE id=3 result=identified proto=Princeton bits=24 repeat=5
DECODE id=3 result=unknown
SAVED id=3 sub=capture_20260722_193210.sub fscope=capture_20260722_193210.fscope
ERR code=radio_busy msg=<text>
```

### 26.3 Host → device frames (illustrative, closed set)

```text
HELLO proto=1 host=flipscope-tui
SETSPAN f0=433050000 f1=434790000 step=13593
PROFILE balanced
PAUSE
RESUME
CURSOR f=433920000
LOCK f=433920000
ARM mode=rssi threshold=-70
ARM mode=manual
CAPTURE start
CAPTURE stop
SAVE id=3
STREAM captures=on
BYE
```

No frame in this set can cause transmission. This is a reviewed invariant, not an accident of the current list.

### 26.4 Streaming and the unbounded sink

In live-offload mode the device emits `CAPPULSE` frames as its internal buffer fills, and the host reassembles them into a capture that may far exceed device RAM. The device still writes its own bounded `.sub` locally (Stage 1 truncation semantics unchanged); the host's reassembled copy is a *superset* stored host-side. Frame `seq` numbering lets the host detect drops and mark a host capture as gapped rather than silently corrupt.

## 27. Terminal UI design

### 27.1 Layout

A view-switching single-window TUI mirroring the device's views at terminal resolution:

- **Spectrum** — braille line plot of the current sweep with a dimmer peak-hold ghost trace; cursor and readout in the footer
- **Waterfall** — the payoff view; see §27.2
- **Peaks** — live sortable table (frequency, strength, recency, count) with per-peak activity sparklines
- **Capture** — logic-analyzer-style pulse-train diagram; see §27.3

### 27.2 Character-cell rendering techniques

This section is deliberately concrete, because the rendering is one of the project's stated pleasures. The core problem is mapping a continuous value (relative RSSI, roughly −110…−40) onto a discrete glyph grid.

**Density ramps (most portable).** Map normalized RSSI to an index into an ordered ramp string:

```text
ASCII (safe everywhere):    " .:-=+*#%@"          10 levels
Unicode blocks (nicer):     " ░▒▓█"                5 levels
```

The ASCII ramp gives more levels but uneven perceptual spacing (`.` to `:` is a smaller visual jump than `#` to `%`); the block ramp has fewer levels but near-linear perceived density. Both are one lookup per cell, both survive SSH and tmux, and both are the mandatory fallback path.

**Partial blocks for sub-cell vertical resolution.** For the spectrum trace, eighth-block characters give 8× vertical resolution within a row:

```text
▁▂▃▄▅▆▇█
```

A bar of height 3.6 cells renders as three full `█` plus a partial — meaning a 20-row plot area resolves 160 distinct levels rather than 20.

**Braille (highest resolution).** Unicode braille (U+2800–U+28FF) packs a 2×4 dot matrix into every cell, so an 80×24 terminal becomes a 160×96 addressable bitmap. This is what makes a terminal spectrum trace look smooth rather than blocky, and why ratatui's canvas defaults to a braille marker. Tradeoff: braille dots are visually light, so a braille trace overlaid with a block-based peak-hold ghost reads better than braille-on-braille.

**Color as a second channel.** Where 256-color or truecolor is available, RSSI can drive color while glyph density drives a *second* variable — density for signal strength, hue for age/recency, so a fading peak visibly cools. Classic SDR waterfall palettes (black → blue → green → yellow → white) map cleanly onto 256-color ramps. Color must always be additive: removing it may lose the second channel but must never make the primary reading illegible.

**Waterfall scroll mechanics.** Keep a ring buffer of completed sweep rows; render newest at top or bottom (pick one, be consistent with the device). Redrawing the whole pane each frame is fine at these rates — a 128×40 cell region is ~5 KB of glyph writes — so resist premature optimization with terminal scroll regions.

**Aspect ratio.** Character cells are roughly twice as tall as wide, so a "square" waterfall region needs about 2:1 columns:rows. Failing to account for this is the most common reason a terminal plot looks subtly wrong.

### 27.3 Pulse-train rendering

The capture view draws the demodulated pulse stream as a timing diagram, where the pulse-domain mental model becomes visual and obvious:

```text
     ┌─┐ ┌───┐ ┌─┐ ┌─┐ ┌───┐ ┌───┐ ┌─┐
─────┘ └─┘   └─┘ └─┘ └─┘   └─┘   └─┘ └─────
     │←Te→│
```

Or more compactly for long streams:

```text
▁▁▁▁▁███▁▁███████▁▁███▁▁███▁▁███████▁▁▁▁▁
```

Requirements:

- Horizontal zoom (a full capture may span tens of thousands of microseconds; interesting structure is often at the tens-of-microseconds scale)
- Pan with cursor keys, with a position indicator showing where in the capture the window sits
- **Te gridline overlay** once the Stage 3 engine has estimated a base timing unit, so the user can *see* whether pulses land on the grid — the single most instructive visualization in the project
- Burst boundaries (long lows) marked, so frame structure is visible at a glance

### 27.4 Controls

Everything reachable by key; mouse-free is a requirement.

| Key | Action |
|---|---|
| `←/→` | Move cursor (paused) / change view (running) |
| `↑/↓` | Cycle peaks / adjust scale |
| `space` | Pause/resume |
| `l` | Lock cursor frequency |
| `a` | Arm capture (toggles trigger mode) |
| `c` | Start/stop capture |
| `s` | Save current capture/session |
| `Tab` | Switch view |
| `p` | Cycle scan profile |
| `?` | Contextual help overlay |
| `q` | Quit host (device keeps running) |

### 27.5 Degradation

The TUI must detect terminal capability and degrade cleanly: truecolor → 256-color → monochrome density ramp → pure ASCII. It must remain legible in a plain `xterm` and over SSH/tmux, so no feature may *depend* on color or Unicode beyond an ASCII fallback.

---

## Part IV — Stage 3: Signal-processing engine

## 28. Purpose

`flipscope-dsp` is a library of composable analysis stages over the two data types the CC1101 can produce (§3). Each stage is a pure function with property tests and a documented failure mode.

**This part is a curriculum as much as a specification.** The stated purpose is to learn how signal processing works by *building the stages*, not by calling a library that has already solved them. Two rules follow, and they are design constraints:

- **No black boxes for the core stages.** Stages 1–7 in §29 are implemented from first principles. They are clustering, statistics, and sequence algorithms over small arrays — none require a DSP framework, and reaching for one would remove exactly the part worth learning. External crates are appropriate for infrastructure (serialization, CLI, terminal) and for genuinely standard numerical primitives (an FFT for autocorrelation in §30), not for the analysis logic itself.
- **Every stage must be inspectable.** Each stage exposes its intermediate output for rendering in the TUI, so the effect of the algorithm is *visible* rather than inferred from a final answer.

Stages ship incrementally, and each is independently useful — the pipeline is worth running when only the first two exist.

## 29. Pulse-timing pipeline (the rich path)

Input: a sequence of signed microsecond durations (positive = high, negative = low) — the RAW model from §19.

| # | Stage | What it does | What building it teaches |
|---|---|---|---|
| 1 | **Pulse-width histogram / clustering** | Cluster durations in log space (they span orders of magnitude) to find characteristic pulse widths | Why log-space binning matters for multiplicative data; how a clean signal produces obvious modes and a noisy one does not |
| 2 | **Base timing unit (Te) estimation** | Derive the fundamental symbol period the widths are integer multiples of | The keystone insight of pulse-domain RE: nearly all OOK protocols are built on one time quantum. Approaches worth implementing and comparing: GCD-like search over cluster centers, histogram of ratios, and fitting candidate Te values by residual error |
| 3 | **Symbol quantization** | Re-express the raw stream in units of Te, with tolerance and per-symbol confidence | How tolerance windows work, and how quantization error accumulates or doesn't |
| 4 | **Encoding classification** | Score hypotheses — OOK-PWM, OOK-PPM, Manchester, NRZ, PCM — against the quantized stream; report ranked candidates | How line codes differ at the timing level, and why several can fit the same stream (hence ranking, never a single answer) |
| 5 | **Framing and sync detection** | Segment frames on long inter-burst gaps; find repeated preambles via autocorrelation over the symbol stream | Autocorrelation as a pattern-finding tool, in the domain where it is easiest to see it working |
| 6 | **Repeat detection** | Identify identical or near-identical frames within and across bursts | Why remotes transmit N repeats, and how redundancy is exploited for confidence |
| 7 | **Bit extraction** | Under the top-ranked encoding hypothesis, extract a candidate bitstream with confidence annotations | How a timing stream becomes data — and how much interpretation that step involves |
| 8 | **Entropy / randomness estimate** | Flag likely rolling or encrypted codes (high inter-press variance) versus static codes (stable frames) | What entropy measures and what it does *not* prove. Observation only; never a defeat claim |
| 9 | **Cross-capture diff** | Align and diff extracted frames across multiple captures (same button repeatedly, or different buttons) | The single most useful RE primitive: which bits change, and what that implies about structure |

Stages 1 and 2 alone, rendered as a histogram plus a Te gridline over the pulse diagram (§27.3), already deliver most of the "oh, *that's* how this works" moment. Build those first and resist skipping ahead.

## 30. RSSI-envelope pipeline (the coarse path)

Input: an RSSI-versus-time series from Activity Monitor or streamed sampling. Lower rate, cruder, still real DSP.

1. **Duty-cycle / on-time estimation** above an adaptive threshold.
2. **Periodicity detection** via autocorrelation (optionally FFT-assisted) — estimate beacon/telemetry intervals, e.g. a sensor that chirps every 30 s.
3. **Burst statistics** — count, spacing distribution, and stability over a window.

Honesty note governing this section: this is envelope analysis of a low-rate amplitude signal, **not** IQ spectral analysis. Periodicity of *bursts* is recoverable; anything about the underlying modulation is not.

## 31. Reporting and data reuse

Every analysis emits a structured report with: inputs and their provenance (which `.sub`/`.fscope`), each stage's ranked hypotheses with confidences, and an explicit `insufficient_evidence` state rather than a forced conclusion. Reports are the batch-mode deliverable and the live-mode side panel content.

The host reads device files unmodified and never rewrites original timings (FR-029 is honored across the cable):

- `.sub` — standard Flipper RAW; the host parses frequency, preset, and signed timings
- `.fscope` — the FlipperFormat sidecar from §19; the host reads capture context and writes its analysis as a **separate** artifact, never by mutating originals

Host-generated derived data (quantized symbol streams, extracted bitstreams, diffs) is always written alongside, never over, the evidence.

---

## Part V — Roadmap, validation, and open questions

## 32. Validation plan (Stage 1)

### 32.1 Technical spike

Before full implementation, build the smallest possible FAP that validates the items below. They are ordered by risk, and the ordering matters: tuning and repeated RSSI reads from a FAP are proven ground (the existing catalog Spectrum Analyzer demonstrates them), so they are cheap confirmations. The genuinely unknown, potentially fatal question is item 1 — whether the SubGhz protocol decoder registry is reachable from a FAP through stable public APIs. Answer it first, because its outcome decides whether FR-023 ships in the MVP or immediate decoding drops to the RAW-preservation fallback.

1. Feeds a known test pulse stream to at least one protocol decoder through stable public APIs, or proves the decoder APIs are not FAP-accessible.
2. Enters asynchronous receive mode and records pulse-level durations under at least one stock preset, and determines whether a pre-arm ring buffer (FR-031) is affordable in RAM.
3. Writes and reopens a minimal valid RAW `.sub` file.
4. Acquires the internal sub-GHz device through stable public APIs.
5. Validates and tunes two or more supported test frequencies.
6. Reads RSSI repeatedly in receive mode.
7. Stops and releases the radio reliably from scanning and recording states.
8. Confirms all required calls are available to external applications on the chosen official firmware baseline.
9. **(Stage 2 forward-look)** Determines whether a FAP can claim a USB CDC channel through stable APIs, or whether commands must layer onto the existing CLI channel.

If the stable application API does not expose the necessary operations, pause implementation and make an explicit decision among reducing scope, using an approved compatibility layer, or maintaining a firmware-specific build. Do not silently couple the app to unstable internals.

### 32.2 Automated tests

- Noise-floor estimator behavior
- Adjacent-bin peak clustering
- Peak hold and decay
- Frequency/span validation
- Profile calculations and buffer bounds
- Settings and session round trips
- RAW `.sub` serialization, line splitting, signed-duration alternation, and round trip
- `.fscope` metadata serialization and RAW/metadata correlation
- Capture buffer bounds, truncation, and SD-write failure
- Pre-arm ring buffer retention of pulses preceding the trigger crossing, if implemented
- Known decoder match, rejected partial frame, ambiguous result, and unknown result
- Proof that decoder normalization leaves the original timing buffer unchanged
- Graceful handling of malformed files
- Fake-backend patterns: flat noise, single carrier, adjacent peaks, moving peak, and short bursts

### 32.3 Hardware tests

- Region-valid presets on the target Flipper
- Known owned remotes or sensors at controlled distance
- No-signal/quiet baseline and locally noisy environment
- Fast, Balanced, and Sensitive profile comparisons
- Long-duration scan and repeated view switching
- Exit during scan, SD failure, and radio acquisition failure
- Manual and RSSI-triggered capture with OOK/AM and 2-FSK test sources where available
- Known-protocol capture, unknown-protocol capture, and deliberately incorrect-preset capture
- Capture buffer full, SD removal during save, and exit while armed or recording
- Reopening saved RAW files with an independent compatible parser in a non-transmitting workflow
- Optional validation against a signal generator or known SDR capture when such equipment becomes available

## 33. Unified milestone roadmap

Milestones are ordered by dependency, not by stage number. Note that **H0 and H1 require no device changes and no serial link**, so the signal-processing work can begin as soon as a few `.sub` captures exist — even from stock firmware, before the FAP is written.

| # | Milestone | Deliverable | Exit criterion |
|---|---|---|---|
| **D0** | Feasibility spike | Minimal FAP proves decoder API access first, then pulse recording with pre-arm feasibility, RAW writing, tuning/RSSI, cleanup, and CDC availability | Decoder-access question answered and stable API path confirmed, or explicit scope decision made |
| **H0** | Formats + DSP skeleton | `flipscope-format` reads real `.sub`/`.fscope`; `flipscope-dsp` runs histogram + Te estimation | Correct Te recovered on ≥3 known owned-remote captures; **no device work required** |
| **D1** | Simulated device UI | Menus, trace, waterfall, peak list using fake data | Usable on-device navigation and legible plots |
| **D2** | Live spectrum | Scanner worker and live RSSI sweep | Repeatable peak from a known owned device |
| **H1** | Batch CLI | `flipscope-cli` produces a full analysis report offline | Report ranks encoding and finds repeats on known captures |
| **D3** | Analysis views | Waterfall, peak hold, peak clustering, Activity Monitor | All primary exploration flows work |
| **D4** | Capture and decode | Frequency lock, preset selection, trigger, pulse buffer, decoder adapter | Known and unknown authorized test signals produce correct result states |
| **D5** | Persistence and hardening | RAW + metadata saving, settings, sessions, errors, long-run tests | Stage 1 acceptance criteria (§22) pass |
| **H2** | Serial link | `flipscope-link` + protocol; live `STATE`/`SWEEP`/`PEAK` mirrored | Stable read of live device state |
| **H3** | TUI remote head | Spectrum, waterfall, peaks rendered live; keyboard control | Control parity with device D-pad for scan/inspect |
| **H4** | Live capture + offload | `STREAM`, unbounded sink, live pulse diagram with Te overlay | Streamed capture exceeds device RAM buffer and reassembles gap-free |
| **H5** | DSP depth | Encoding classification, framing/sync, cross-capture diff, entropy | Diff correctly isolates changing bits across repeated presses |
| **D6** | Device extensions (P1) | Automatic preset ranking, on-device pulse inspection, BinRAW derivation | P1 items separately approved and validated |
| **H6** | Hardening + portability | Reconnect, terminal degradation, Pi 5 + macOS builds | 30-min live session stable over SSH/tmux |
| **X1** | Optional futures (P2) | BLE streaming, external CC1101 support, richer desktop analysis | Separately scoped after H6 |

## 34. Risks and mitigations

| Risk | Stage | Impact | Mitigation |
|---|---|---|---|
| Required radio APIs are not stable or FAP-accessible | 1 | Blocks official-firmware implementation | Complete the D0 spike before building the full UI |
| Protocol decoder APIs unavailable to FAPs | 1 | Immediate decoding blocked | Validate first in D0; preserve standard RAW captures and rely on Stage 3 for decoding |
| Retuning too slow for a useful broad sweep | 1 | Low refresh rate and missed bursts | Narrower presets, fewer bins, scan profiles, fixed-frequency monitoring |
| Short transmissions occur between samples | 1 | Peaks missed | Revisit frequencies faster, peak hold, Activity Monitor |
| Display implies more precision than exists | 1, 2 | Misleading learning outcome | Label relative RSSI, show sweep rate, explain sequential sampling |
| Noise floor varies significantly by frequency | 1 | Poor peak detection | Local/rolling estimates and user-adjustable margin |
| Frequent logging wears storage or hurts performance | 1 | Reduced reliability | Buffer writes; compact summaries rather than every sample |
| Radio ownership conflicts with firmware services | 1 | Crashes or stuck radio state | Centralize access in a radio adapter; test all cleanup paths |
| Regional restrictions differ | 1 | Invalid or unsafe presets | Validate against device-supported ranges; keep receive-only |
| Wrong preset produces unusable pulse timings | 1 | Later decoding fails | Require explicit preset display, provide guidance, add P1 preset ranking |
| Decoder produces an ambiguous or false match | 1, 3 | Misidentification | Require complete-frame checks and repeat consistency; show Unknown when confidence is insufficient |
| Pulse buffer or SD throughput exceeded | 1 | Truncated or lost capture | Bound captures, stream in chunks where safe, mark truncation explicitly |
| Cleanup or normalization destroys evidence | 1, 3 | Later analysis unreliable | Preserve immutable original timings; write derived data separately |
| FAP cannot claim a CDC channel via stable APIs | 2 | Live modes blocked | Checked in D0; fall back to the existing CLI channel; batch mode needs no link and still delivers Stage 3 value |
| Serial drops during streaming | 2 | Corrupt host capture | Sequence-number frames; mark gaps explicitly rather than silently concatenating |
| Terminal lacks color/Unicode | 2 | Unreadable waterfall | Mandatory ASCII density-ramp fallback; capability detection |
| DSP over-claims a protocol identity | 3 | Misleading RE | Ranked hypotheses with confidence and an `insufficient_evidence` state; never a single forced answer |
| Reaching for a DSP library to shortcut a hard stage | 3 | Defeats the entire learning purpose | No-black-box rule (§28): core stages hand-built; external crates only for infrastructure and standard primitives |
| Expecting IQ-grade analysis | all | Wrong mental model | State the IQ boundary (§3) prominently in README and `--help`; point IQ work at dedicated SDR gear |
| Scope creep from "it's just a PC, it can do anything" | 2, 3 | Never ships | Crate boundaries and milestone gating; GUI explicitly out of scope |

## 35. Open decisions

**Product**

1. Keep **FlipScope** as the name or choose a more playful Flipper-themed name?
2. Which region and first three presets should be optimized for the initial build?
3. Should the default launch action be Quick Scan or the last-used preset?
4. Should capture default to manual start or an RSSI threshold trigger?
5. Which stock OOK/AM and 2-FSK presets should appear in the first capture menu?
6. What maximum pulse count and recording duration balance useful evidence against RAM and SD constraints?
7. How much sweep history is worth saving versus keeping session files minimal?
8. Is publication in an official/community app catalog a goal, or is this a personal learning build? If publication is a goal, §2.1's differentiation story is the submission positioning, and catalog review requirements should be read before D1.
9. Should detailed raw pulse inspection remain a separate companion view to keep the primary workflow focused?

**Host and DSP**

10. Protocol on a dedicated CDC channel vs. commands layered on the existing Flipper CLI — decided by D0.
11. Analysis report format: JSON sidecar vs. FlipperFormat-style text vs. both.
12. Does `flipscope-tui` embed batch mode as a sub-command, or stay separate binaries sharing the library?
13. Is there value in compiling a subset of `flipscope-dsp` back onto the device (device P1 "on-device pulse inspection"), or does the host own analysis permanently?
14. BLE streaming — same protocol over a different transport, or its own design? Defer until USB is proven.
15. Minimum supported terminal baseline (plain xterm? require 256-color?) for the shipped experience vs. the fallback.

## 36. References

- [Flipper Zero Sub-GHz documentation](https://docs.flipper.net/zero/sub-ghz)
- [Reading raw Sub-GHz signals](https://docs.flipper.net/zero/sub-ghz/read-raw)
- [Flipper Zero development documentation](https://docs.flipper.net/zero/development)
- [Flipper Zero firmware API symbol list](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/targets/f7/api_symbols.csv)
- [Flipper Zero product specifications](https://flipper.net/)
- [Official app catalog (Flipper Lab)](https://lab.flipper.net/apps) — includes the existing Spectrum Analyzer FAP referenced in §2.1
- [ratatui — Rust terminal UI library](https://ratatui.rs/)
- [serialport-rs — cross-platform serial](https://github.com/serialport/serialport-rs)
