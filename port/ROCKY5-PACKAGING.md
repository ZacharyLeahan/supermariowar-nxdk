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

## Pinned example metadata and dimensions

Official example inspected at Rocky5/XBMC4Gamers revision
`f99d9de56e861adc1c1eaec77f4bbb076b110e95`:

| Example file | Pixels |
| --- | --- |
| poster.jpg | 200 × 283 |
| poster_small.jpg | 64 × 91 |
| thumb.jpg | 227 × 129 |
| fanart.jpg | 640 × 360 |
| icon.png | 149 × 263 |

These are observed example dimensions, not normative artwork/catalogue rules.
The unusually tall example icon must not be assumed to require every icon to be
that shape. Only image headers were inspected; no example artwork was saved in
this repository. File/blob identities are in
[dimension evidence](test-results/rocky5-example-artwork-20261106.json).

A text-only [draft synopsis](packaging/xbmc4gamers/default.xml) uses the official
example's XML field names with our own description. Before private installation,
set `foldername` to the final folder name and copy it to `_resources/default.xml`
beside `default.xbe`. Empty release/rating/publisher/title-ID fields intentionally
assert no official metadata. The draft still says qualification is in progress;
update only after acceptance. XML parsing passed on the host; dashboard display
and unknown/empty-field handling remain untested. No template is automatically
included in the existing candidate archive.

## Candidate notice bundle

[notices/INVENTORY.json](notices/INVENTORY.json) binds 22 exported text notices
to repository revisions in the `4addeb4e...` build manifest. Legacy CP1252 text
was transcoded to UTF-8 with original/exported hashes retained. The bundle also
contains its inventory and explanatory README; it is not a complete source archive
or a declaration that redistribution review is finished.

Use `--notices ports/nxdk/notices` when packaging this candidate. The packager
checks its XBE binding, text hashes, flat-file inventory and FATX paths before
creating output. Host tests reject tampered notice text and mismatched XBE
inventories. A private no-data package with these notices now has 49 files;
all 48 listed checksums and all ZIP members passed byte verification. Its archive
SHA-256 is `52c64340bd53171652a4607ef68957188abea42b878dbb7de3f90419484a2e55`.
See [package result](test-results/clean-notices-package-20261106.json).

## Original vector artwork draft

An original code-native SVG design now exists privately in
`artifacts/artwork-geometric-v1-20261106/`. This is a new geometric/typographic
design, not a retry through a fallback image API. The earlier raster generation
failure remains historical; no image-model CLI/API was used. No upstream images
or official console logos were copied. Editable SVG masters and a local sharp
renderer produce poster 200×283, small poster 64×91, thumbnail 227×129, fanart
640×360 and a proposed 256×256 square icon. Root `default.tbn`, `fanart.jpg` and
`icon.png`, plus `_resources/default.xml`, are staged in that private folder.

Host visual review confirmed the exported title and geometry at poster, fanart,
icon and small-poster sizes. This is **not dashboard-tested or release-accepted**;
confirm the icon shape, text legibility and actual view layouts on the Xbox.
Only [hash-bound textual provenance](test-results/original-vector-artwork-20261106.json)
is committed. Images, SVGs and renderer remain outside the code-only repository.

## Private dashboard install assembly

The local `SMW-nxdk-dashboard-20261106.zip` now assembles the clean candidate,
pinned external game data (FATX exclusions retained), text notices, synopsis and
nine original artwork/resource files. The synopsis folder name matches the
archive's top-level folder. It has 1,544 files; ZIP integrity, all 1,543 listed
checksums and every archive member against the unpacked tree passed. Archive
SHA-256: `01c6b8f9a494a1f94246600bcd786d12212948a0a36df73ed041b2a83632df0d`.

This private local-install archive contains upstream media and must not be pushed
to the code-only source repository. Assembly code and original SVGs remain in
private workspace artifacts. The package has not been installed or visually
reviewed in XBMC4Gamers, and is not release accepted. See
[verification record](test-results/private-dashboard-package-20261106.json).
