# Silent Xbox decode/resampling cost probe

For the first 15 seconds (or whole shorter file) of three shipped songs, this
measures Vorbis decoding separately and decoding plus SDL's existing 44.1 to
48 kHz stream conversion. File loading and stream setup occur outside the stage
timers. No playback device is opened, and the game/rendering is not running.
The worker uses the same 256 KiB stack budget as game music decoding. This is
a stage-cost experiment, not a gameplay benchmark or audible-quality test.

Build using the documented space-free setup with this folder as the make
working directory. Run from the game checkout:

```sh
python3 ports/nxdk/tools/run-audio-probe.py audio-cost --count 1 --wait-seconds 300
```

The runner copies the three pinned source assets into a separate
`F:\SMW-audio-cost-probe` folder and verifies every hash before launch. The
program auto-reboots when finished; retrieve `audio-cost.log`. Preserve the
XBE, asset, SDK archive and source revisions for before/after comparisons.
SDK library objects do not automatically rebuild when compiler flags change.
A clean SDK build is required when comparing explicitly optimized libraries
with caches produced by the initial SDK example. No audio quality setting or
resampler algorithm is changed by this probe.

## Native comparison

A single run of each build on the same upgraded Xbox measured resampling
for menu / overground / stageclear at 1459 / 1464 / 650 ms with the earlier
SDK cache, versus 742 / 745 / 333 ms after a clean `-O2 -DNDEBUG` SDK build.
Decoded and converted frame counts matched for every track. Combined decode
and resampling time fell from 1852 / 1993 / 834 to 1095 / 1231 / 499 ms.
This measures isolated processing cost, not game FPS, startup reliability or
audible quality. Source pins and assets were unchanged; the prior example
build had left unoptimized SDK objects that make would otherwise reuse.

The hash-bound results and SDK archive hashes are recorded in
`../../test-results/audio-cost-comparison.json`. Reproduce the optimized game
build from the workspace with:

```sh
./scripts/build-game.sh clean-build PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1
```

Then build this probe against those generated SDK libraries and run the
command above. Keep the game and probe executables distinct.

Build and record provenance from the game checkout with:

```sh
ports/nxdk/tools/build-probe.sh audio-cost
```

The runner now requires this wrapper's hash-bound `bin/BUILD-INFO.json` and
archives its recursive source patches and SDK library hashes. Manual make alone
does not refresh that metadata. Use the wrapper immediately before launching
when SDK source or libraries have changed.
