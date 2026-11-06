# Super Mario War — nxdk

A lean, source-only port kit for the original Xbox using the open-source nxdk.
This independent repository contains Xbox platform code, text patches, build
recipes and test findings. It is not a GitHub fork or a complete copy of the
game. No game sprites, audio, maps, screenshots, data archives, XBEs or release
packages are included. No proprietary Microsoft XDK is required.

The full upstream game and its history are obtained separately into an external
workspace. That upstream history contains assets; it is never imported into this
repository. Game data is a separate, optional local download. Referencing that
download does not grant rights to the assets.

## What connects this kit to upstream

- Game: [mmatyas/supermariowar](https://github.com/mmatyas/supermariowar), pinned
  SDL2 base `c7056790ccb485b7309137251167b75fed1482e4`.
- SDK: [XboxDev/nxdk](https://github.com/XboxDev/nxdk), pinned base
  `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`, plus the included SDK/SDL patches.
- `port/` is copied into upstream's `ports/nxdk/` directory.
- `patches/game.patch` supplies integration changes to the game and source-library
  submodule pointers. It contains no game media or binary patch payloads.
- `upstream-lock.json` records exact dependency revisions and SHA-256 hashes of
  every overlay file and applied patch. The scripts reject mismatched inputs.

## SDL2 pin policy

The game base is the **last commit before upstream converted to SDL3**:
`c7056790ccb485b7309137251167b75fed1482e4`. It is the direct parent of
`cf1f6a5fe9e13565b4db2d37ab57d06b4f7d4397` ("Port to SDL3"). We pin the
immutable commit, not `master`, a release archive or the moving `sdl2` branch.
The latter currently points to an older SDL2 commit, not this final baseline.
Future maintenance on a separate SDL2 branch does not silently change our base.

The SDK library is pinned independently: nxdk's Xbox **SDL2 2.0.9** fork at
`2554902f7bbf469449216ee25f88016421271cd8`, plus the shutdown patch in this kit.
This is not a claim that 2.0.9 is the latest desktop SDL2 release. Replacing it
with a newer desktop release requires porting and testing the Xbox backend.
Upgrading either pin requires explicit patch/reconstruction and runtime review;
SDL3 is outside the current port scope.

## Prepare, build and run locally

Prerequisites: Git, Python 3, GNU make, clang/LLVM including lld/llvm-ar, CMake,
bison and flex, plus nxdk's host-tool prerequisites. The original tested host
was macOS arm64 with Homebrew LLVM 23.1.2. Other hosts are not qualified here.
Use an external workspace with no spaces; the scripts refuse to overwrite an
existing directory. Library/toolchain initialization can download substantial
source code. Nothing is pushed by these commands.

```sh
# From this port-kit checkout; gets code/libraries but no game-data submodule.
python3 tools/prepare.py --workspace /tmp/smw-nxdk-dev
python3 tools/build.py --workspace /tmp/smw-nxdk-dev --clean
```

The output is `/tmp/smw-nxdk-dev/repos/modern/ports/nxdk/bin/default.xbe`.
Build logs and `KIT-BUILD-INFO.json` provenance stay in that external workspace.
The lock also pins the embedded display revision/date and the complete make
configuration, including the 45-second runtime watchdog used by candidate
`84a132cd...`. The display revision identifies the runtime source; reconstructed
Git commits are synthetic and are recorded separately in the build manifest.
The corrected independent reconstruction and original clean development build
now produce identical XBE SHA-256
`4addeb4e4e32def7a77125edf1815d3c1065835239fd431a1afca39f844803a9`
(3,473,408 bytes) on the same macOS/LLVM host. The recipe fixes nxdk's stale
JPEG-object clean rule and maps compiler macro paths to the established
`/tmp/smw-xbe-workspace` prefix with forward slashes. This establishes same-host
reconstruction reproducibility, not cross-host reproducibility or full hardware
acceptance. See [exact-build evidence](port/test-results/clean-reconstruction-exact-20261106.json).

The build does not create an uploadable release or automatically deploy anything.

To run the game, obtain matching assets separately for local use. Either start
with a different fresh workspace and explicitly request them:

```sh
python3 tools/prepare.py --workspace /tmp/smw-nxdk-play --with-data
python3 tools/build.py --workspace /tmp/smw-nxdk-play --clean
```

Or initialize data in an already prepared code workspace:

```sh
git -C /tmp/smw-nxdk-dev/repos/modern submodule update --init -- data
```

The pinned data revision is `1139d89ef7e38368536317afd7db54cea2488d5b` from
`mmatyas/supermariowar-data`. Keep its directory layout. Locally assemble a
writable Xbox folder containing `default.xbe` and `data/` beside each other.
The port reads `D:/data/`. See [port notes](port/README.md) for FATX limitations
and hardware procedures. Do not upload that assembled game folder here.

The kit targets SDL2, disabled networking, stock-64-MB memory constraints and up
to four local players. A full clone may retain upstream's desktop packaging;
this kit does not build or distribute desktop packages or icons.

## Testing and limits

[Testing approach and original hardware results](port/TESTING.md) describe the
previous exact-XBE qualification: complete Classic and ten-minute local matches,
saves/recovery checks, and captured music/effects. The test console was a 128 MB
Stellar Xbox with a virtual pad and CPU players. Physical stock64 and four
physical pads remain unqualified. The audio capture lost 0.825 seconds in its
transport; subjective listening and gapless-loop acceptance remain open.

[Kit verification](VERIFICATION.md) distinguishes reconstruction/build checks
performed on this kit from those original hardware results. A freshly built
XBE has new provenance and does not inherit another hash's hardware acceptance.
This kit itself does not launch hardware or publish files.

## Keep the repository lean

Run `python3 tools/audit.py` after committing any proposed update. It scans every
reachable blob for media/archive extensions, nontext content and common embedded
media markers. It also checks patch target paths. This is a technical guard
alongside review, not legal clearance or proof against all possible encodings.
Ignore rules are additional protection, not a way to hide tracked assets.
Never merge upstream's history into this repository or use `git push --mirror`.

To develop, edit the reconstructed upstream checkout. Export reviewed source
changes as text patches or update `port/`, update lockfile hashes, re-audit and
re-run reconstruction checks. Avoid `git diff --binary`. Do not commit local
checkouts, recordings, builds, screenshots or asset-containing archives.

## Attribution and notices

Super Mario War and its original authors are credited upstream and in preserved
source headers. The work here derives from that game and adds nxdk platform
support; it does not claim ownership of the game or Nintendo assets. Existing
copyright/license notices in code remain intact. See [NOTICE.md](NOTICE.md).
