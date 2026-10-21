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
