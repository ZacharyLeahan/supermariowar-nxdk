# Xbox testing approach and current results

Updated 2026-10-25. This is a private, playable test candidate, not a fully
accepted release. We distinguish compilation, host checks, captured output,
emulator tests and physical Xbox tests rather than treating them as equivalent.

## Current UI test build, 2026-10-25

The current private Xbox install is `10910209...`, source `d82d9af1`, with the
thumbnail-cache fix and optional runtime monitor enabled. It was hardware tested
under `ui-audit.xbe` and then installed as the identical `default.xbe`; the prior
binary is retained as `default-before-thumbs-fix.xbe`. See
[thumbnail investigation](THUMBNAILS.md) and its exact-hash result records.
It passed the deliberately invalid-cache browser test and a Classic result/
normal-exit regression. The six additional mode smokes used the earlier
`4b895c87...` directory-fix candidate. Neither run is complete release acceptance.
The older tables below continue to describe their original `8df74f38...` artifact.

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

The separate diagnostic executable `c1dd5ced...` subsequently completed both a
120-second and a full 600-second four-human Time Limit match on `0smw`, plus a
2v2 five-life Classic match. The long session included 185 mixed-input commands
across all four routes, followed by a neutral interval at the end. Normal exit
committed saves and completed clean audio shutdown. The minimum available
memory was 17,336 KiB; music underruns stayed zero and stderr was empty.
Steady game-state samples were about 60.5–62.3 FPS; two reporting windows
immediately following match initialization were 45.9 and 48.1 FPS.
An eight-second stereo capture completed with nonzero output and zero reported
KVM overruns; listening and gapless-capture acceptance remain separate.
See [long match and teams](test-results/four-player-runtime-600s-and-teams-20261025.json).
These successful runs do not establish the cause of the original incident or
transfer acceptance to the original executable or a future release build.

## What remains unverified

- Human listening review, seamless-loop acceptance through a reliable capture
  path, separate effects-volume/mute checks, and coverage of all effects/packs.
- Repeat four-human timed/team qualification on the final executable, confirm
  physical cabling, conventional gamepads and USB reconnect. The diagnostic
  candidate completed timed/team matches; the original possible stall remains
  unresolved, and watchdog failure recovery has not been exercised.
- A physical stock-64-MB Xbox. The 64 MB application-budget allocation probe and
  XBE `LIMIT64MB` flag are useful evidence, but do not replace stock-hardware tests.
- Broader cold-start and firmware reliability. Six heterogeneous healthy starts
  on this candidate are not an automated repeated-launch qualification; earlier
  audio-startup failures and their SDK fixes are documented in [AUDIO-STARTUP.md](AUDIO-STARTUP.md).
- Broader map, mode, pack and preference coverage, plus emulator testing if used.

Use [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) to track acceptance work. A fully
tested or shipped-game claim remains premature.

## Current-build audio audibility, 25 October 2026

The user confirms hearing game audio through the Xbox speakers on candidate
`10910209be2ef92fe5f9f3d1b38e06f0ced0ea86d453d9100d13ee519bb3c83c`.
The current KVM firmware returns HTTP 503, “No usable I2S samples”, for PCM
capture. This establishes audibility only; mixing, all effects and seamless
loops still require review. Host mixer/decoder, save-record and controller
routing checks passed independently. See
[audio confirmation](test-results/audio-user-confirmation-20261025.json).

## November 2026 exact-candidate regression

Candidate `84a132cdd1f16613dcefb6e3fddff415bee31d8c8ddb9398ca651ad9d32b1855`
passed initial launch/input/exit coverage across all 22 modes using direct P1
and Wi-Fi S3 P2–P4. Mode-specific observations and untested objectives are
recorded in [mode smokes](test-results/mode-smokes-20261106.json); these brief
checks do not qualify every mode objective or a complete match in each mode.
The Health CPU match completed, four-human simultaneous movement was observed,
and two thumbnail pages (18 previews) plus map selection passed. Existing
Health limits and a non-default extra-heart rate survived native reload;
the temporary rate was then restored to 20 in the UI. Healing is still open.
See [candidate baseline](test-results/health-candidate-baseline-20261106.json).

A preserved native batch exited normally with zero reported music underruns,
empty error output and no watchdog marker. Minimum reported free memory was
14804 KiB. FPS reports cover ten-second windows and label only the final app
state; short menu/game transitions cannot establish per-mode steady-state
performance. See [native logs](test-results/health-mode-batch-native-logs-20261106.json).
The KVM routing interruption was repaired in a separate chat; delivery-counter
qualification is distinct from the actual game observations above.


## Current mixed-controller run, 10 November 2026

Exact candidate `84fbddeb6e17820c80a1262456632a743260e4ae73c44d1a5df6974b1adc6339` booted, accepted independent ready actions from wired P1 and Wi-Fi P2–P4, and ran four-human Classic on 0smw. After 44 completed simultaneous-input batches, the KVM reported no HDMI signal and Xbox FTP timed out. The match result and cause remain unverified; native logs must be preserved before another launch. Wi-Fi send errors occurred despite all four routes remaining configured and advancing in bounded samples.

The recovered KVM audio service captured eight seconds of non-silent menu audio and 30 seconds of gameplay stereo PCM. Gameplay had zero reported capture overruns and zero full-scale samples; the menu capture had two full-scale samples per channel. This is measured capture evidence, not listening or complete audio acceptance. See [exact-candidate results](test-results/mixed-current-20261110.json).


The subsequent same-XBE 60-second Time Limit match on 0smw completed with four human players tied at two points. Normal menu Exit returned to a fresh XBMC screen, with clean native audio teardown, zero music underruns and CRC-valid saves. Both volume controls Off produced exact silence; restoring music with effects Off restored stereo signal. Original Max volumes were restored and confirmed in the native save log. See [timed completion and volume checks](test-results/timed-current-20261110.json).

Music-only capture still contains a few full-scale samples. Host float decoding of the pinned upstream menu track also exceeds unity; source overshoot is a plausible contributor, not a proven attribution. Effects-only menu focus movement was silent and did not isolate an effect trigger. Listening, isolated effects and error-free controller transport remain open. The earlier interrupted Classic session has now been retrieved: empty error log, no watchdog marker, and a gameplay log ending without a normal shutdown tail; its cause remains unresolved.
