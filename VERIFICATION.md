# Port-kit verification

Verified 2026-10-21 on macOS arm64 with Homebrew clang/LLVM 23.1.2.
See [verification.json](verification.json) for hashes and build provenance.

- Prepared a new external workspace from the pinned upstream game and SDK.
  Existing local repositories were used as clone caches; this was not a fresh
  internet-only download test. Recursive library dependencies were initialized.
- Did not initialize the game-data submodule or copy game assets into the kit.
- Compared 820 reconstructed files against the committed development snapshot;
  all matched, as did the two source-library gitlinks. The reusable lock-based
  check covers 345 integration/overlay files.
- Compared the three patched SDK/SDL files with the original tested source;
  all matched byte for byte.
- Ran a full clean SDK/game build in the new workspace, producing `default.xbe`.
  A subsequent incremental build reproduced the same hash:
  `e2bb810deb6f424a1c22191a8c0d0e21a053ba0740b62716484858dddc8bf48b`.
  XBE header/section bounds passed, the 64 MB limit flag is on, and timestamps
  match the specified `SOURCE_DATE_EPOCH`.
- Audited this kit's reachable Git blobs: source/text only, with no asset,
  archive, binary or asset-targeting patch violations. Negative tests confirm
  rejection of media extensions, binary content without an extension, patches
  targeting assets and an asset still present only in old commits.

The generated XBE and full build logs remain outside this repository. They are
not release uploads. Builds do not initialize or publish runtime game data.
The four negative audit fixtures use synthetic content, not game assets.

## Repeat the checks

```sh
python3 tools/verify-reconstruction.py --workspace /tmp/smw-nxdk-dev
python3 tools/build.py --workspace /tmp/smw-nxdk-dev --clean
python3 tools/test-audit.py
python3 tools/audit.py
```

The build writes `KIT-BUILD-INFO.json` beside the XBE with recipe/source/SDK
identity, local diffs, flags and artifact hash. A clean recipe state was recorded
for the successful incremental verification. Preparation/build directory paths
are local configuration and do not belong in this repository.

## Runtime acceptance remains separate

This newly reconstructed executable has **not** been emulator- or hardware-tested.
Earlier physical Xbox results are recorded in [port/TESTING.md](port/TESTING.md)
and apply to their exact original XBE hash, not automatically to this build.
The original working game checkout, installed candidate, assets and development
history were not replaced. Full historical source and asset-bearing upstream
objects are confined to external development checkouts, not this kit's history.

## Four-controller update, 2026-10-25

The current recipe adds a bounded four-route KVM harness, exact-hash diagnostic
log collection, optional main-loop watchdog instrumentation (off by default),
and separate records for partial original-candidate coverage, completed
120/600-second four-human diagnostic matches, and a 2v2 Classic match. See [testing](port/TESTING.md) and
[runtime investigation](port/RUNTIME-STALL.md). The longer original-candidate
incident remains unresolved; no game fix or complete release acceptance is claimed.

A fresh source-only reconstruction applied the updated integration patch and
verified all 351 locked files plus the fmt/stb gitlink revisions. The asset-audit
negative fixtures passed. This update has not undergone a new clean recipe
build; the earlier clean-build hash above belongs to the earlier recipe.
The diagnostic executable was built in the development workspace and hardware
tested separately, with its own source revision and hash in the result record.

## Thumbnail-cache fix and mode audit, 2026-10-25

The missing thumbnail-cache directory reproduced a fatal error on the Xbox.
The port now creates that directory and can render previews in memory when a
cached PNG is missing, invalid or has incorrect dimensions. Two thumbnail pages,
map selection/reopen/cancel and 18 generated PNG files were verified on the
first fix candidate. Six additional modes received short gameplay/exit checks.
The newer candidate passed a deliberate empty-PNG test and a completed Classic
round followed by normal exit. It is installed as the current private test
`default.xbe`, with the previous binary preserved for rollback. See
[exact hashes and scope](port/THUMBNAILS.md). No public binary or media was added.

These are development-workspace builds and native tests with separate hashes.
The updated recipe is source-reconstructed and hash-checked; the earlier clean
recipe build above does not qualify this updated recipe as a new clean build.

## Native Windows builder, 2026-11-22

The source-only kit at recipe revision `242452c2022af0d1466916fb43b869a77ad55a34`
completed a clean, end-to-end build on Windows using the pinned XboxDev
MSYS2/MinGW toolchain. The test used neither WSL nor Docker. Python and the
other Unix-style build utilities were installed only inside the isolated
toolchain directory by the bootstrap script.

The build consumed game source revision
`c7056790ccb485b7309137251167b75fed1482e4` and data revision
`1139d89ef7e38368536317afd7db54cea2488d5b`, produced a 3,469,312-byte XBE
with SHA-256
`f5e05e35f4a6b0ba0cd3b874be3530d0a8c85fb2bedb8d0a7a8d7d4b3a946922`,
and staged 1,486 data files. The generated file began with the `XBEH` magic;
the intentionally excluded Contest content was absent.

The assembled folder and build workspace remained outside this repository.
After upload to a new Xbox directory, all 1,489 private package files passed
readback hash verification. This exact XBE then booted on physical original
Xbox hardware, rendered the title and menus at 720x480p60, accepted ready input
from all four ESP-KVM controller routes, started a four-player Classic match on
`0smw`, accepted a combined right/jump input from player 1, and paused/resumed.
The pre-existing Xbox title was not overwritten. This is a short gameplay smoke
test, not exhaustive mode, audio, save, long-run or stock-64-MB acceptance; no
emulator test was performed. Private screenshots and the asset-bearing install
remain outside the repository. Machine-readable details are in
[native-windows-verification.json](native-windows-verification.json).
