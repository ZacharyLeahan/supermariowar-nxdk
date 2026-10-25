# Thumbnail browser and UI audit

On 2026-10-25, opening **Thumbs** on diagnostic candidate `c1dd5ced...`
reproduced a fatal error loading `D:\data\maps\cache\0smw.png`.
FTP inspection confirmed that the `cache` directory did not exist. The package
copies files and omits empty directories. The browser attempted to save a preview,
ignored the save failure, and immediately loaded the nonexistent PNG as a required
asset. This is a port/runtime cache assumption, not a missing nxdk PNG feature.

The port now creates the cache directory before saving thumbnails and reports
save errors. On Xbox, the browser can generate a preview directly in memory when
a cached PNG cannot be loaded or has the wrong dimensions. Disk caches are an
optimization, not required game assets. Existing desktop browser behavior remains.

## Exact artifacts and evidence

- Reproduction: `c1dd5ced6a9e370d7c1b62628d758179dcc15fbc4995d31ce63d09bb6d22d196`,
  source `20da2dee5cbee6c0c988d10d1fddd057fec523b4`.
- Directory fix: `4b895c877f733a38a02cbfc16973f117fdd044042f784321cfb22e9a8f79a9bc`,
  source `8ab33f82`.
- Cache-validation extension: `10910209be2ef92fe5f9f3d1b38e06f0ced0ea86d453d9100d13ee519bb3c83c`,
  source `d82d9af1`; separate executable and acceptance record.

All three compiled with nxdk and the existing pinned dependencies, using
`PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1 AUDIO_CLEAN_SHUTDOWN=1
STARTUP_WATCHDOG_MS=45000 EXIT_WATCHDOG_MS=10000 RUNTIME_WATCHDOG_MS=45000`
and `SOURCE_DATE_EPOCH=1792148400`. No emulator run is recorded.
Hardware is the upgraded 128 MB Stellar Xbox with four S3 XID routes.

The directory-fix candidate displayed two pages of nine thumbnails, selected
`1-3` into the map preview, reopened the browser and cancelled back to settings.
Its six short gameplay checks used Frag Limit, Yoshi's Eggs, Tag, Capture the Flag,
Survival and Health on `1-3`, with four human players and mixed input. They are
startup/gameplay/exit checks, not completed matches or complete mode acceptance.
Map Filters also opened and closed. A Health mode-options attempt remained in the
goal editor and is not counted as a successful options-panel check.

The first longer mode-selector presses repeated, so filenames labelled `coins`
and `ctf` in the private input journal do not establish the actual mode. The
screens show Yoshi's Eggs and Tag for those runs; Capture the Flag was selected
and exercised separately. Versioned records use the observed mode names.

## Recovery and capture limitations

The reproduced fatal error caused the runtime watchdog to write a marker after
45,041 ms, at frame 3,816, phase 2 (state update), app state 1, then return to XBMC.
Logs, saves, executable hash and marker were archived before modification. The
old remote marker was renamed and retained so it cannot be mistaken for a new
candidate failure. This verifies recovery from this blocked fatal UI path, not
from arbitrary kernel hangs.

KVM disconnects interrupted the audit. Its firmware changed from the earlier
`g34b8936` build to `g118eac1`; later uptime readings also restarted. After reconnect,
the Xbox was still running Capture the Flag and could exit normally. Capture
loss and sent inputs during disconnects are inconclusive, not game failures.
Private raw evidence is under `logs/hardware/ui-modes-thumbs-20261025/` in the
parent development workspace. Screenshots, audio and generated PNGs stay out
of the asset-free sharing repository.

## Invalid-cache regression and current install

The cache-validation candidate was hardware tested with a deliberately empty
`0smw.png`. The browser still displayed its correct preview. Readback after exit
confirmed the fixture remained zero bytes, establishing the in-memory fallback.
The original PNG was then restored and its original hash verified. A five-life
four-human Classic match on `0smw` completed in a tie after mixed input and an
idle finish, followed by clean audio shutdown. There were zero music underruns,
empty stderr and no new failure marker. The minimum available memory in that
shorter run was 16,952 KiB. See
[bound regression](test-results/thumbs-invalid-cache-and-classic-20261025.json).

The six-mode session on the earlier directory-fix candidate also exited cleanly
with zero music underruns, empty stderr and no new failure marker. Its minimum
available memory was 9,892 KiB; broader mode cycling and browsing still need
memory qualification. See [mode results](test-results/thumbs-and-mode-smokes-20261025.json).

The validated `10910209...` binary is now installed and readback-verified as
`F:\SMW-nxdk-final-test\default.xbe`. Hardware testing used the identical bytes
under `ui-audit.xbe` in the same title folder. The earlier `8df74f38...` binary
is preserved as `default-before-thumbs-fix.xbe`. This is the current diagnostic
test candidate with runtime monitoring enabled, not a fully qualified release.
No public executable or media release was created.

One minor UI mismatch remains: the title says “Press Any Key,” but controller
Start does not advance it; A does. Source checks the menu select/cancel/random
bindings there. This is recorded as a prompt/usability issue, not a failed
controller route. Health's dedicated options panel, batch thumbnail generation,
all remaining game modes and longer full-match coverage remain unqualified.
