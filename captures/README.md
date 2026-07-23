# FlipScope Capture Corpus (H0.2)

This directory holds a small, fixed corpus of `.sub` capture files used as
fixtures for Stage 3 DSP development and testing (§28, §32.2). It exists so
that timing-domain analysis (Te estimation, envelope detection, decode
validation) can be developed and tested against known-good pulse data before
any device hardware work happens.

Nine files, in three groups:

- Four protocol pairs, each a `RAW` capture and a same-protocol sibling
  `.sub` file (see section 4 — the sibling is an independent
  official-firmware test asset, not necessarily a decode of the raw
  capture):
  - `came_raw.sub` / `came.sub` (CAME, fixed code)
  - `princeton_raw.sub` / `princeton.sub` (Princeton)
  - `nice_flo_raw.sub` / `nice_flo.sub` (Nice FLO)
  - `megacode_raw.sub` / `megacode.sub` (MegaCode)
- One negative fixture: `noise_433920_ambient.sub` — ambient RF noise, no
  transmission present.

## 1. Provenance

The eight CAME/Princeton/Nice FLO/MegaCode files are unmodified copies of the
official firmware's own unit-test assets, from the `flipperdevices/flipperzero-firmware`
repository, path `applications/debug/unit_tests/resources/unit_tests/subghz/`.

Downloaded 2026-07-22 from the `dev` branch, pinned to commit
`7432d21a7e362d4a5f636e24d6209fbb2eedff1f`.

| File | Source URL |
|---|---|
| `came_raw.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/came_raw.sub |
| `came.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/came.sub |
| `princeton_raw.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/princeton_raw.sub |
| `princeton.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/princeton.sub |
| `nice_flo_raw.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/nice_flo_raw.sub |
| `nice_flo.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/nice_flo.sub |
| `megacode_raw.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/megacode_raw.sub |
| `megacode.sub` | https://raw.githubusercontent.com/flipperdevices/flipperzero-firmware/7432d21a7e362d4a5f636e24d6209fbb2eedff1f/applications/debug/unit_tests/resources/unit_tests/subghz/megacode.sub |

`noise_433920_ambient.sub` is a local ambient-noise capture made with stock
Flipper Zero firmware's "Read RAW" feature at 433.92 MHz, using the Flipper
Zero's internal CC1101 with the `Ook650Async` preset. No transmitter was
active during capture; it records whatever the CC1101 heard on an otherwise
quiet channel.

## 2. License / attribution

The eight files sourced from `flipperdevices/flipperzero-firmware` originate
from a GPLv3-licensed repository. They are included here unmodified, as test
data, under the terms of that license. See the upstream repository's
[LICENSE](https://github.com/flipperdevices/flipperzero-firmware/blob/dev/LICENSE)
for full terms.

## 3. Integrity (SHA-256)

Computed with PowerShell `Get-FileHash -Algorithm SHA256`.

| File | SHA-256 |
|---|---|
| `came.sub` | `499AACBBE6298EC3F4DABF518AA7FDF20909E6A4895FE9FF165E49C053C49E24` |
| `came_raw.sub` | `DF91DAA10BC431AE1ABE62CFB42BC35EA68C3A65FC68D74D5E2D2E1B42566DD4` |
| `megacode.sub` | `1B5A68C1B06869E9229A876CBBFDFDC4AB011BE9BB52F1DC8618C5BE179F71FB` |
| `megacode_raw.sub` | `A4A1381B9C4A822E945F35DA363E83F2510F35A0B6AD8A145F02A86B01B034E0` |
| `nice_flo.sub` | `48E43DF0D2038FBA82C22DBFE7F246905AA1686853A0DAF577CE03252F25ADDB` |
| `nice_flo_raw.sub` | `5C83F0CFB79915919FC565C17CE9C67567D160644BF0E105B93FAD876954619E` |
| `noise_433920_ambient.sub` | `2C5225B534DAD1B089504EB1A471F2F76DB6D884C39FB2BDDF3B534B11A765E1` |
| `princeton.sub` | `4E58F7DFA03D0D9457F4214DF1C7F62C04E4F960D335FCEA406D38297DAC9975` |
| `princeton_raw.sub` | `0FE7E7CCDF6722652BE6D1012BB26B39BC1D9A79F4AEF74FAC6A8FC750ACDF08` |

## 4. Ground truth (for DSP validation)

**This section previously claimed that each `*_raw.sub` file's decoded
sibling (`.sub` without the `_raw` suffix) is the answer key for that
capture, produced by the official Flipper Zero firmware's own decoder. That
claim was WRONG**, and here is how it was found: the D0.2 probe
(`device/flipscope/flipscope.c`) fed `princeton_raw.sub`'s first three
bursts, transcribed verbatim, into the official firmware's SubGhz decoder
running on real hardware (build 6631ff5). The decode callback fired and the
decoder reported `Princeton 24bit Key:0x007C5703 Yek:0x00C0EA3E` (Yek is the
24-bit bit-reversal of Key — internally consistent) — not `princeton.sub`'s
`Key: 00 00 00 00 00 95 D5 D4`. The sibling `.sub` files are independent
official-firmware unit-test assets (encoder-test inputs used to exercise the
firmware's SubGhz transmit-encode path), not decoder outputs for the raw
captures, and the naming convention (`X_raw.sub` / `X.sub`) does not imply
that pairing.

The Princeton row below is now hardware-verified against
`princeton_raw.sub` itself. Its Te is not asserted from any sibling file;
it is derived from the capture: a one-off script parsed
`princeton_raw.sub`'s first `RAW_Data` line, took the absolute value of
every entry across the first three bursts (152 entries, through the third
inter-burst guard gap, `-16394`) whose magnitude is < 1000 µs — i.e. the
short pulses, excluding the long pulses and the guard gaps — and averaged
them: 75 short-pulse entries, sum 41483 µs, mean ≈553 µs. `princeton.sub`'s
`TE: 400` does **not** describe this capture.

The CAME, Nice FLO, and MegaCode rows still cite their sibling files as
before, but that citation is now marked **UNVERIFIED as an answer key for
the raw capture** — the same pairing error that broke the Princeton row may
also apply to these, and each needs its own independent re-derivation
(on-device decode of the raw capture, or Stage 3 timing analysis) before
being used as DSP validation ground truth.

| Fixture | Protocol | Expected Te (µs) | Notes |
|---|---|---|---|
| `came_raw.sub` | CAME | ≈320 | UNVERIFIED. Fixed code. Sibling `came.sub`: `Bit: 24`, `Key: 00 00 00 00 00 6A B2 34` — not confirmed as a decode of `came_raw.sub`; re-derive before use as ground truth. |
| `princeton_raw.sub` | Princeton | ≈553 (derived; see above) | Hardware-verified: on-device decode of this capture's first three bursts (build 6631ff5, 2026-07-23) reported `Key: 0x007C5703`, `24bit`. Te is the mean of the 75 short-pulse (< 1000 µs) entries across the first three bursts, not `princeton.sub`'s `TE: 400`, which does not describe this capture. |
| `nice_flo_raw.sub` | Nice FLO | ≈700 | UNVERIFIED. Documented standard Te for the protocol; sibling `nice_flo.sub` not confirmed as a decode of `nice_flo_raw.sub`; re-derive before use as ground truth. |
| `megacode_raw.sub` | MegaCode | ≈1000 | UNVERIFIED. Capture has a leading segment of RF noise before the real signal starts, deliberately kept as a noise-vs-signal discrimination case for the Te estimator and envelope detector; sibling `megacode.sub` not confirmed as a decode of `megacode_raw.sub`; re-derive before use as ground truth. |

## 5. Negative fixture

`noise_433920_ambient.sub` contains no transmission — it is ambient RF noise
captured on 433.92 MHz with no transmitter active. A correct Te estimator run
against this file must refuse to report a Te (or otherwise clearly flag "no
periodic signal found"), not fabricate one from noise.

## 6. Evidence immutability (FR-029)

These nine files are original captured evidence and are immutable: they must
never be edited, reformatted, or overwritten in place. Any derived, cleaned,
or analyzed data (e.g. estimated Te, cleaned pulse trains, decode attempts
against `noise_433920_ambient.sub`) must be written to new files alongside
the originals, never over them.
