# Xbox testing approach and current results

Updated 2026-10-25. This is a private, playable test candidate, not a fully
accepted release. We distinguish compilation, host checks, captured output,
emulator tests and physical Xbox tests rather than treating them as equivalent.

## Exact candidate tested

- Game source: `0476190f5a1827a49befacb5dd6b777ddff94005` on `port/nxdk-xbox`.
- nxdk source: `c5c574c6ba5778595b782064044ff56b407e835b`, plus the local
  patches recorded in build provenance. No proprietary Microsoft XDK is used.
- `default.xbe` SHA-256:
  `8df74f3854f99fcebcc8487bf0e042d65e161c749bd3a4ba402fe3d1afb12b19`.
- Hardware: upgraded 128 MB Stellar Xbox; one KVM virtual controller, with
  three CPU players for local-match tests. No emulator test is recorded.
- Installed candidate: `F:\SMW-nxdk-final-test\default.xbe`.
- [Candidate index](test-results/opaque-copy-candidate.json) binds the tested
  executable, package, source archive and individual result records.

Documentation commits after the game-source revision do not change the tested
XBE. Older diagnostic builds have their own hashes and results; a passing test
on one executable does not automatically qualify another.

## How we test

1. **Build and identify the artifact.** Compile with nxdk, validate the XBE,
   record effective flags, dependency revisions and local patches, and hash the
   executable. A full clean SDK/game rebuild reproduced this candidate's XBE.
   Compilation establishes neither successful boot nor gameplay correctness.
2. **Deploy a verifiable package.** Use a separate writable test folder. On the
   fresh install, upload and read back all 1,512 packaged files and verify their
   hashes before launching. Preserve prior logs and saves before a new run.
3. **Observe the console.** Launch through XBMC FTP, send bounded controller
   input through the IP KVM, and confirm screens with captured frames. A launch
   timeout, a sent button command or an advancing counter alone is not proof
   that the intended screen or action occurred.
4. **Exercise complete flows.** Check menu navigation and previews, start a
   match, pause/resume, reach results, return to the menu and exit normally.
   Relaunch to check saved settings and complete another match. Keep CPU-player
   coverage distinct from simultaneous physical-controller coverage.
5. **Collect independent evidence.** After normal exit, read back the exact XBE
   hash, game log, stderr, save records and any failure markers. Compare screenshots
   with callback, underrun, performance, memory and shutdown observations.
   Following a failure, preserve evidence before overwriting or relaunching.
6. **Capture actual audio output.** Record KVM PCM into WAV files, retain format,
   duration, hashes and level measurements, and compare recordings with shipped
   Ogg/WAV assets. Keep game underruns separate from capture transport loss.
   Waveform matching does not substitute for subjective listening.
7. **Record the scope.** Save exact-hash JSON results and a readable account of
   what passed, failed or remains inconclusive. Keep the artifact unchanged while
   testing unless a concrete defect requires another candidate.

The [port README](README.md) documents build, provenance, deployment and recovery
tools. [AUDIO.md](AUDIO.md) includes the reproducible audio-capture command and
source-comparison method. Raw local evidence is retained in the parent workspace's
`logs/hardware/`; the versioned JSON records retain hashes and result summaries.

## What has passed on this candidate

| Area | Observed result | Evidence |
| --- | --- | --- |
| Build reproducibility | Full clean SDK/game rebuild produced the same XBE hash. | [Clean rebuild](test-results/opaque-copy-full-clean-rebuild.json) |
| Menu rendering | Correct borders/shading; steady menu samples about 60 FPS. | [Menu](test-results/opaque-copy-menu.json) |
| Complete local matches | Classic, five lives, on `0smw`; full 600-second Time Limit on `NMcCoy_1-3`, with Extra Time off; results and normal exit. | [Classic and timed matches](test-results/opaque-copy-classic-and-ten-minute.json) |
| Pause and return | Paused/resumed the timed match, then returned through results to the menu and exited. | [Match flow](test-results/opaque-copy-classic-and-ten-minute.json) |
| Saves | Reloaded five-life Classic, 600-second Time Limit, Extra Time off and player configuration; completed another Classic match. | [Reload and match](test-results/opaque-copy-reload-and-match.json) |
| Save recovery | Rejected a newer invalid-controller record in favor of the older valid slot; both bad-CRC slots fell back to defaults. Original files restored. | [Invalid controller](test-results/opaque-copy-invalid-controller.json), [bad CRC](test-results/opaque-copy-both-bad-crc.json) |
| Audio output | Captured menu and match music in both channels; identified jump/death effects against source files. | [Captured audio](test-results/opaque-copy-captured-audio.json) |
| Music controls | Half-volume source-matched gain about 0.475; mute produced six seconds of digital silence; unmute restored output. | [Captured audio](test-results/opaque-copy-captured-audio.json) |
| Audio during game flow | Music continued while paused, audio was present after resume, a Classic match completed, and menu music returned. | [Captured audio](test-results/opaque-copy-captured-audio.json) |

The Classic/timed session recorded zero audio underruns and at least 15,940 KiB
available memory, with its last eight memory samples stable. The later captured-
audio session recorded zero underruns, four decoder loops, 27,273 callbacks,
empty stderr and clean audio shutdown. These numbers describe those runs, not
all possible maps, packs, modes or startup conditions.

## Audio capture limitation found on 2026-10-17

The long menu recording contains 165 seconds of stereo samples and a strong
repeated-music match (correlation about 0.990). However, the KVM stream log
reports **39,600 missing frames, or 0.825 seconds**, while its status endpoint
reports zero overruns. Subscriber transport gaps and I2S overruns are different
counters. This explains why captured repeat timing is shorter than the source;
it prevents accepting a gapless loop from this recording.

A separate transition capture ended after 16.818 of the requested 24 seconds
while video/control were also used. An overlapping recording request returned
HTTP 409 and contains no usable audio. Neither is counted as a game-audio pass
or proof of a game underrun. Subsequent short recordings completed.

The original menu music itself contains some full-scale samples when decoded,
and mixed gameplay also reaches full scale. Source matching and counters cannot
establish that clipping, crackling or tonal quality is acceptable. We have
confirmed captured output, not completed a human listening acceptance test.

Raw audio-session evidence: `logs/hardware/captured-audio-20261017/`, including
WAVs, screenshots, comparison scripts/results, KVM stream diagnostics and
hash-verified logs collected after exit. No game/SDK code was changed for this
session; the existing package remains unchanged.

## Four-player hardware session, 2026-10-25

Four separate S3 XID controller routes opened on physical ports 1–4, despite
SDL enumeration order 3,4,1,2. All four humans readied, moved independently,
and reached a five-life Classic result on `0smw`. Each route paused/resumed
the subsequent timed match; some short presses needed retries. Routes 1 and 2
currently control game players 2 and 1; the user has not confirmed cabling.
Disconnecting the KVM control session neutralized all four S3 reports within
150 ms, but physical USB unplug/replug and game-side release remain unqualified.

The 600-second four-human match is **inconclusive**. Capture stopped changing
at timer 372; the user later saw the dashboard while capture retained the game
frame. This proves stale KVM capture. However, recovered game logging ends near
the repeated-frame interval without a clean-exit record, so a concurrent game
stall remains possible. The final log reports about 61 FPS, 17,404 KiB available
memory and zero music underruns. These values do not establish match completion.

[Result record](test-results/four-player-20261025.json) binds this session to the
original executable. [Runtime investigation](RUNTIME-STALL.md) describes the
separate, optional watchdog candidate; its results must remain separate.

## What remains unverified

- Human listening review, seamless-loop acceptance through a reliable capture
  path, separate effects-volume/mute checks, and coverage of all effects/packs.
- Complete four-human timed/team matches, confirmed physical cabling, conventional
  gamepads and USB reconnect. The four-S3 session establishes partial simultaneous
  coverage, with an unresolved possible runtime stall.
- A physical stock-64-MB Xbox. The 64 MB application-budget allocation probe and
  XBE `LIMIT64MB` flag are useful evidence, but do not replace stock-hardware tests.
- Broader cold-start and firmware reliability. Six heterogeneous healthy starts
  on this candidate are not an automated repeated-launch qualification; earlier
  audio-startup failures and their SDK fixes are documented in [AUDIO-STARTUP.md](AUDIO-STARTUP.md).
- Broader map, mode, pack and preference coverage, plus emulator testing if used.

Use [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) to track acceptance work. A fully
tested or shipped-game claim remains premature.
