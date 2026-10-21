# nxdk audio adapter

The desktop implementation still uses SDL_mixer. The Xbox build instead uses
`ports/nxdk/Audio.cpp` behind the same game sound interface, with nxdk's pinned
SDL2 audio driver. nxdk itself is unchanged.

- Output is 48 kHz stereo signed 16-bit PCM, 1,024 frames per device buffer.
- WAV effects load through SDL_LoadWAV and convert once into device-format PCM.
- A 16-voice integer mixer sums effects with saturation and honors volume,
  repeat counts, stopping and the game's 40 ms retrigger guard.
- Ogg Vorbis music decodes on a worker thread using unchanged stb_vorbis from
  `nothings/stb` commit `2c980bb59875b0d32144a71867fbdebb2f77cd20`.
- SDL_AudioStream resamples music to the device format. A one-second (192,000
  byte) PCM ring isolates decoder work from the callback; playback waits
  for a small prebuffer. Full songs are never retained as decoded PCM.
- The callback performs integer mixing only: no file I/O, allocation, codec or
  game callbacks. Track replacement, looping, pause/resume, EOF and shutdown
  are synchronized with the SDL audio device lock. Music-finished game logic
  runs on the main thread from the regular platform update.

Build from the parent workspace: `scripts/build-game.sh`. Host verification from the game checkout: `python3 ports/nxdk/tests/run.py`.
That runs address/undefined-behavior sanitizers on the mixer, decodes and rewinds
all 42 shipped Ogg files from files and memory with identical PCM, and uses a dummy SDL device to exercise playback,
rapid track replacement, pause/resume, loop, EOF and shutdown. Host SDL is the
installed SDL2 API (Homebrew SDL2-compat here); Xbox remains nxdk SDL 2.0.9.
The vendor codec's pointer-comparison compiler warning is retained unchanged.

The effects-only Xbox run completed a Time Limit match, exercised jump and
pause sounds, and logged 61 effect starts plus 1,119 non-silent output buffers.
No errors were logged; available memory was about 22 MiB under the verified
64 MB title budget. See `logs/hardware/audio-sfx`.

Music-enabled hardware acceptance is recorded separately after retrieving the
streaming counters. KVM firmware does not capture HDMI audio, so non-silent
callback buffers prove PCM generation and output-driver activity, not audible
speaker quality. Listening for correct output, crackles, clicks and balanced
volumes requires a TV/speaker or a separate audio capture path. Do not label
that listening check passed without a human/capture result.

Only the shipped WAV effects and Ogg music formats are supported on Xbox by
this adapter. Custom sound/music packs using other codecs remain unsupported.
Networking remains disabled. Settings persistence is opt-in through `PREFERENCES=1`;
see `SAVES.md` for format, native evidence and remaining coverage.

## Checkpoint status

WAV effects passed the first Xbox match/output-counter test. The first stdio
Vorbis implementation produced severe underruns on hardware; the current fix
loads at most 4 MiB of compressed song bytes and decodes from memory. This
removes per-byte stdio calls without retaining the whole decoded song. All
shipped tracks fit the compressed cap. The fix builds and passes sanitized host
playback tests. Two four-player hardware matches completed visually, but the
audio callback count stayed at zero, so this build failed audio acceptance.
Starting output before the worker and giving its decoder a 256 KiB stack
restored output in the next hardware run (commit `964bb665`). It completed a
30-second four-player match on `0smw`, with 11,900 callbacks, 6,069 non-silent
buffers, 11,479,784 consumed music samples and zero recorded underruns. Error
output was empty. Available memory stayed at or above 19,964 KiB. Sampled
steady gameplay was 52.1–56.1 FPS, slower than the earlier silent build;
performance tuning remains open. This does not isolate which startup change
was responsible. Counter evidence is in `test-results/audio-music3.*`; full
logs/screenshots are in the parent workspace's `logs/hardware/audio-music3`.
A second launch also started output (five startup callbacks), consumed
17,219,584 music samples, crossed one full menu-song loop and logged zero
underruns and no errors. See `test-results/audio-loop.*`. These two launches
do not establish startup reliability across other dashboards or BIOSes.
Audible speaker quality has not been confirmed.

## Listening check on the Xbox

Use a TV or speakers connected to the Xbox's usual audio output. At the main
menu, confirm the music is present and menu movement/select sounds are clean.
Start a local match and check jumping, coin/power-up and collision effects over
the music. Pause/resume, finish a match, and return to the menu; listen for a
missing track, persistent crackling, clipping or a stuck effect. Let a song
repeat once to check the loop boundary. Report the screen/track/action and
whether it was silent, distorted, too loud/quiet, or clean. Output counters and
host tests cannot substitute for this listening check.

## Audio memory audit

`test-results/audio-assets.json` records all 42 shipped tracks, collected during
the sanitized decode/rewind equivalence test. The largest compressed file is
3,766,228 bytes (`SMB_Overground.ogg`), below the 4 MiB cache cap. The largest
codec-reported persistent setup allocation is 218,648 bytes, and decoder
temporary memory is 4,096 bytes. These are host codec reports, not measured
Xbox peak allocations; SDL resampling, allocator overhead, effects, stacks
and game resources are additional. The compressed vector retains its largest
capacity across track changes, bounded by the cap.

The cached-mixing experiment in commit `53292ebf` avoids reading back the
write-combining DMA buffer allocated by nxdk SDL2: it mixes into a 4 KiB stack
block, scans that cached PCM for diagnostics, then copies the output once.
Host tests cover exact PCM and guard bytes across multiple scratch blocks.
Hardware performance acceptance is recorded separately; the earlier listening
candidate remains available for comparison.

## Intermittent startup failure remains open

The first cached-mixing hardware run (`test-results/audio-cached.*`) completed
a match, but startup and all subsequent audio callback counters stayed at zero.
The PCM ring filled without being consumed. Its 61–62 FPS samples cannot be
compared to active-audio runs. This disproves treating the earlier two successful
starts as a general startup fix. The original candidate is preserved; its two
passes likewise do not guarantee a working start tomorrow. A diagnostic build
logs analog/digital DMA registers and, only on zero callbacks after 100 ms,
tries one pause/restart through nxdk's backend API. This experiment is not an
accepted fix. No nxdk source modifications or BIOS changes are involved.

See [the startup investigation draft](AUDIO-STARTUP.md) for pinned revisions,
contradicting runs, the standalone probe and the remaining hypotheses.

## Combined preferences/match/exit regression

`test-results/exit-match-regression.*` records a four-player timed match with
active audio and zero underruns, followed by clean explicit Exit with no SDL
assertions and no diagnostic watchdog marker. Steady gameplay samples were
55.9, 53.9 and 52.4 FPS; the cached-mixing change has not demonstrated a frame
rate gain. Minimum reported free memory was 17,592 KiB under the previously
verified 64 MB application budget. Listening and physical four-controller/
retail-64-MB checks remain unperformed. Startup reliability is still open.

## Loop resampler continuity

The worker now retains SDL_AudioStream history across an Ogg repeat. It seeks
the decoder to the start without flushing or clearing the converter; one-shot
EOF still flushes pending samples. A host regression pauses the dummy device,
drains actual production-worker PCM, and compares about 1.5 laps with one
uninterrupted converter fed the same source chunks. The previous worker failed
at sample 640872; the corrected worker matches through the seam. Existing WAV,
track replacement, pause, EOF and shutdown sanitizer tests still pass.

This is a host PCM comparison using SDL2-compat, not a listening test or a
native nxdk SDL2 loop acceptance result. Audible source-track seams and the
independent intermittent Xbox startup stall remain open.

## Captured Xbox output, 2026-10-17

The current `8df74f38` candidate was tested through ESPKVM's new
`/audio.pcm` endpoint (48 kHz, signed 16-bit little-endian stereo). This adds
actual captured Xbox output to the earlier callback/decoder evidence.
`test-results/opaque-copy-captured-audio.json` records the scope, hashes and
limitations. Raw recordings and screenshots are retained locally under
`logs/hardware/captured-audio-20261017` in the workspace.

To capture without changing KVM settings or interrupting another subscriber:

```sh
python3 ports/nxdk/tools/capture-kvm-audio.py \
  --url http://192.168.1.169 --seconds 10 \
  --output /absolute/path/to/new-audio-evidence
```

Use a new directory for every recording. The command rejects an already-active
subscriber, verifies the declared PCM format, writes a playable WAV and JSON
metrics, and returns failure for an incomplete capture. Avoid concurrent video
streaming while recording; a combined video/audio attempt in this session ended
early. A short recording or HTTP 409 is capture evidence, not proof of a game
underrun. The endpoint's `streaming` value can briefly remain true after a
successful bounded client disconnect.

Source comparisons use FFmpeg to decode the shipped Ogg/WAV files to the same
48 kHz stereo format, followed by normalized cross-correlation. This identifies
music/effects and checks volume behavior; it does not prove subjective sound
quality, exact phase/sample identity, analog output quality or all sound packs.
The original menu Ogg itself decodes to some full-scale samples, so isolated
full-scale samples in a recording must not automatically be blamed on the port.
The SDL2/game pause behavior leaves music playing and emits a pause effect.

The 165-second menu recording received the requested sample count, but source
alignment found a repeated segment 152.506 seconds apart instead of the source's
153.339-second duration. The KVM's retained stream log independently reports
39,600 missing frames (0.825 seconds), with `epochs=0`; its status endpoint still
reported `overruns=0`. Those counters describe different layers. The stream
also reports queue drops, some of which can occur during bounded client
shutdown. The strong repeated-waveform correlation (0.990) establishes a music
repeat, but this capture cannot qualify a seamless loop or locate an audible
seam in the game. The game log records zero underruns, four decoder loops,
27,273 callbacks, empty stderr, and normal clean audio shutdown. No game or SDK
code was changed for these checks; the tested XBE and existing package remain
unchanged. Sound-effects volume/mute and human listening remain open checks.
