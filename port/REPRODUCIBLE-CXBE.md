# Deterministic XBE conversion

The pinned nxdk cxbe normally writes the current time into the XBE header and certificate. The same PE therefore produces different XBE hashes. The baseline and patched host test records are in `test-results/cxbe-*.json`.

Apply from the workspace root against nxdk revision `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`:

```sh
git -C repos/nxdk apply ../modern/ports/nxdk/patches/cxbe-source-date-epoch.patch
make -C repos/nxdk/tools/cxbe CXX=/opt/homebrew/opt/llvm/bin/clang++
cd repos/modern
python3 ports/nxdk/tests/run-cxbe.py
SOURCE_DATE_EPOCH=1700000000 ports/nxdk/tools/build-game.sh PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1
```

An existing XBE may be considered up to date: remove only the generated `ports/nxdk/bin/default.xbe` before rebuilding when changing the epoch. The outer SDK makefile does not track cxbe source dependencies, so explicitly rebuild cxbe as shown. An absent environment variable retains the upstream wall-clock behavior; invalid values fail before overwriting output.

Verified: four accepted decimal epoch cases produce identical repeated conversions of the same existing PE, with both timestamps equal to the requested value. Eight malformed/out-of-range cases fail without overwriting the previous XBE. The patch is archived separately from nxdk and captured by build provenance.

This proves host conversion reproducibility only. Two subsequent builds of all game objects and links at source commit `603c33e8`, using the same prebuilt SDK archives, also produced identical PE and XBE hashes (see `test-results/reproducible-game-build.json`). This does not prove two fresh SDK builds are identical or qualify the XBE on an emulator or physical Xbox. Source identity, PE generation, compiler paths, SDK libraries and packaging must also match. Replacing timestamps in a previously tested XBE changes its hash and requires a new runtime evidence association.


A subsequent clean rebuild of both game and SDK libraries at source `bdbd3349`
reproduced the preserved XBE `99884277...` byte-for-byte, using epoch 1700000000
and the same pinned dependency revisions/diffs and toolchain. The commands and
archive catalogs are recorded in `test-results/full-clean-game-sdk-rebuild.json`.
Several SDK archive bytes changed, while the final XBE stayed identical; this
proves executable reproducibility for those inputs, not archive-byte identity
or reproducibility across different compiler versions or build paths. The
original branch was restored after the temporary source checkout.
