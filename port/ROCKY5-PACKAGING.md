# XBMC4Gamers downloader preparation

Status: submission preparation only; no public archive uploaded and no maintainer
contact made. Current local candidate is not release accepted. Assets remain
external to the code-only repository.

## Confirmed dashboard layout

Rocky5's README places `_resources` beside `default.xbe`; its example also
includes root `default.tbn`, `fanart.jpg` and `icon.png`. The example resources
contain `default.xml` for synopsis and an `artwork` directory with `poster.jpg`,
`poster_small.jpg`, `thumb.jpg`, `icon.png`, `fanart.jpg` and other optional views.
These filenames are confirmed; dimensions and required downloader submission
fields still need checking against the current artwork tools/catalogue.

References inspected 2026-11-06:

- https://github.com/Rocky5/XBMC4Gamers/blob/master/README.md
- https://github.com/Rocky5/XBMC4Gamers/tree/master/Synopsis%20Example
- https://github.com/Rocky5/XBMC4Gamers/tree/master/Synopsis%20Example/Star%20Wars%20-%20Battlefront%202/_resources/artwork
- https://github.com/Rocky5/XBMC4Gamers/blob/master/Mod%20Files/system/scripts/URLDownloader/default.py

Do not replace an official catalogue entry or imply downloader acceptance from
local ZIP installation. Prepare a final accepted candidate, title/version,
archive and unpacked sizes, checksum, source/build link, controls, requirements,
known limitations and artwork for maintainer review. Confirm the desired top
folder/extraction destination with the catalogue entry before submission.

## Local package and checks

`tools/package-game.py` takes a preserved XBE/manifest, verifies the pinned clean
data revision, validates FATX paths and writes a deterministic ZIP, checksums,
provenance and exact-XBE evidence. It refuses overwritten outputs or mismatched
executables/evidence. The 2026-11-06 Health package was built locally with:

```sh
python3 ports/nxdk/tools/package-game.py \
  --build-dir ../../artifacts/health-life-candidate-20261106 \
  --output ../../artifacts/SMW-nxdk-health-20261106 \
  --evidence ports/nxdk/test-results/health-candidate-baseline-20261106.json
```

This package includes external upstream game data for private installation; it
must not be pushed into the source repository. Public binary/data distribution
remains unresolved under the user's asset-free publication requirement.

## Artwork work remaining

An original cover concept was requested using geometric arena contestants,
without copied sprites or official console branding. The image tool rejected
the generation; no artwork was produced. Do not describe a missing cover as
completed or include unrelated commercial artwork. Final artwork needs visual
review in the dashboard after the exact format/dimensions are confirmed.

## Executable archive without game media

`--without-data` produces an executable/provenance package with no `data`
directory. Its README explicitly requires the external pinned data before
launch. The 2026-11-06 binary-only candidate has 23 files and passed complete
checksum and ZIP integrity verification; it is still experimental. This is a
possible submission artifact that preserves separation of game media, not a
complete ready-to-play downloader install. A downloader entry would need an
explicit upstream-data acquisition/install step or another maintainer-approved
workflow; do not advertise the executable archive as self-contained.

```sh
python3 ports/nxdk/tools/package-game.py \
  --build-dir ../../artifacts/health-life-candidate-20261106 \
  --output ../../artifacts/SMW-nxdk-binary-only-20261106 --without-data
```

No public archive has been uploaded. Packaging absence of the data directory
is not a substitute for reviewing executable resources and redistribution
notices before publication.

## Reproducible clean candidate package, November 6

A private binary-only archive now contains candidate `4addeb4e...`, provenance
and three exact-XBE native result files (boot, Classic/input/audio and cache
recovery). It has 25 files; all 24 listed checksums, all ZIP members against the
unpacked tree, and ZIP integrity passed. Archive SHA-256:
`273ad243357f052bec46f25a80fd9934b3f13e1f608bd2fe3be9c9ccf89da922`.
No data directory is included. This is not a standalone downloader install and
was not uploaded publicly; artwork, final qualification and maintainer-compatible
external asset acquisition remain open. See
[package verification](test-results/clean-binary-package-20261106.json).
