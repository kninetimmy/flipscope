# Device toolchain

## Decision: ufbt

FlipScope's device build uses **ufbt** (micro Flipper Build Tool), installed
via pip, rather than a full `fbt` firmware-tree checkout.

## Rationale

- The PRD mandates building against **official-firmware stable APIs only**
  (see root `CLAUDE.md`, "What FlipScope is" / Stage 1). ufbt's whole purpose
  is to deploy a prebuilt SDK for a released firmware version and build FAPs
  against exactly that stable API surface — it is a direct match for what we
  are allowed to depend on.
- `fbt` requires cloning and building the entire `flipperzero-firmware` tree.
  That buys access to internal/unstable headers and private APIs, which the
  PRD explicitly forbids us from depending on (§32.1: "If the stable
  application API does not expose the necessary operations, pause
  implementation and make an explicit decision... Do not silently couple the
  app to unstable internals"). A full firmware tree is also far heavier to
  install and keep in sync for no capability we're permitted to use.
- ufbt is the toolchain the Flipper community and app catalog expect for
  external FAPs; it keeps the loop (`ufbt`, `ufbt launch`, `ufbt debug`) short
  and matches how the app will eventually be distributed/reviewed.

If a later milestone (D0 spike item 1: SubGhz protocol decoder registry
access) turns out to require APIs that are not exposed through the stable SDK
that ufbt deploys, that is itself the answer to the D0 feasibility question —
per §32.1 we stop and make an explicit scope decision rather than switching to
`fbt` to reach into unstable internals.

## Setup

```
pip install ufbt
```

On first build in an app directory, `ufbt` downloads and caches the prebuilt
SDK/toolchain for the current stable release channel under `~/.ufbt/` — this
is expected on a clean machine and only happens once per SDK version.

## Build

From `device/flipscope/`:

```
ufbt
```

This compiles the FAP against the cached SDK and writes the artifact to
`device/flipscope/dist/flipscope.fap` (and a debug ELF under
`device/flipscope/dist/debug/`). Both `dist/` and the generated `.vscode/`
folder are build outputs and are git-ignored.

Other useful ufbt targets (unused by CI, listed for reference):
`ufbt launch` (build, deploy, and run on a connected device over qFlipper),
`ufbt debug` (build and attach a debugger).
