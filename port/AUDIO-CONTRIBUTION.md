# Audio lifecycle evidence for nxdk

Local preparation only; no issue comment, pull request or release has been posted.
Checked 2026-10-16 using the GitHub read API and remote Git refs.

## Existing reports and current source

- [nxdk #362](https://github.com/XboxDev/nxdk/issues/362): intermittent loss of
  audio interrupts after returning through a dashboard and relaunching. This
  resembles our game failures, but the common root cause is not established.
- [nxdk #363](https://github.com/XboxDev/nxdk/issues/363): missing audio
  deinitialization. Its discussion explicitly includes applications using SDL.
- [nxdk #20](https://github.com/XboxDev/nxdk/issues/20): broader audio design
  discussion, distinct from qualifying a bounded lifecycle correction.

The nxdk remote HEAD is `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`, identical to
our selected SDK. The SDL submodule remains pinned at
`2554902f7bbf469449216ee25f88016421271cd8`. The standalone nxdk-sdl `nxdk` branch
HEAD is `7f2ecef0c22b79dc8fc5b0c79b9e334f915c8ff8`; its change after the pinned
revision adds logging, and the Xbox audio source is identical between these two
revisions. Fetching that branch did not change our checkout or dependency pins.
The archived API results are in workspace `logs/nxdk-audio-*-20261016.json`.

## What has actually been observed

Game XBE `6d283fa3...`, built from `fcdf1376`, includes the selected HAL unchanged
inside a diagnostic interposer. The same binary selects legacy or clean close
through a pre-initialization sidecar; no SDK HAL library/source was changed.

In the native legacy run the initial interrupt connection succeeded. On normal
Exit, SDL called XAudioInit again with NULL callback/data. The existing interrupt
was connected before this call; the next KeConnectInterrupt returned zero. The
object's Connected field was zero after reinitialization. No watchdog fired.
This establishes a rejected reconnect in the shutdown path, not that all later
startup failures have this cause.

The first clean close instead disconnected the interrupt, removed any queued
DPC, stopped/reset DMA engines without resetting the codec link, cleared DMA
bases and balanced the observed descriptor-page lock. It completed without a
watchdog or cleanup-failure marker. Two following startups and closes also
passed at the time of the first sequence report; additional evidence is tracked
separately. Earlier baseline launches also sometimes succeeded, so this small
sequence does not demonstrate prevention of intermittent warm-launch failure.
See `test-results/audio-close-first-native.json` and
`test-results/audio-close-sequence-native.json`.

The independent `probes/audio-reopen` fixture exercises 20 silent SDL device
open/progress/close cycles in one process, records connection returns and
available pages, and uses an independent watchdog. It passed 20 native clean-mode cycles on XBE `608d7833...`: all interrupt
connections succeeded, callbacks advanced, closes took 68 ms each, stderr was
empty, and available pages stayed at 15,931 throughout. No watchdog or cleanup
failure marker was present. See `test-results/audio-reopen-clean-native.json`.
The same-image legacy control then stalled on cycle six: callbacks stayed at
one and close entered its reinitialization path without returning. Its
45-second watchdog wrote a fresh marker and rebooted. Cycles 2–5 still had
callback progress despite rejected interrupt reconnects; do not assume every
rejected reconnect immediately silences output. This is a reproduced
in-process lifecycle failure, not yet proof of the cause of #362. See
`test-results/audio-reopen-legacy-native.json`. The clean reversal then stalled on cycle four despite a successful initial
interrupt connection; clean close returned in 146 ms without a watchdog or
fallback. Cleanup alone does not fix startup reliability. See
`test-results/audio-reopen-clean-reversal-native.json`. The same diagnostic can
compare legacy and clean modes without music decoding or gameplay. A cycle-one
startup failure is inconclusive about reopening. Watchdog recovery is a failure,
not a successful close.

## Requirements before proposing an SDK correction

1. Qualify repeated in-process clean reopen, retaining a legacy control and every
   failed trial. Confirm callbacks advance, connections succeed, memory does not
   decline each cycle, and no watchdog/cleanup fallback masks failure.
2. Keep a proper shutdown operation separate from XAudioInit: NULL callbacks
   legitimately support manually feeding samples, so a general API must preserve
   that behavior. The diagnostic's NULL/NULL interception is SDL-specific.
3. Define ownership, idempotence and failure semantics. Do not reinitialize a
   connected kernel interrupt or unlock descriptor pages while hardware can
   still reference them. Do not free buffers after an unverified shutdown.
4. Test the actual proposed HAL/SDL patch, not just the interposer: repeated
   opens/closes, notification-loss close, game matches and dashboard relaunches.
5. Keep the intermittent startup issue open until failed controls and adequate
   repeat/cold-start evidence support prevention. Human listening, other BIOSes,
   physical 64 MB hardware and four actual controllers are separate coverage.

A focused contribution to #363 could pair a public HAL teardown operation with
SDL's use of it and the silent fixture. It must not be presented as resolving
#362 solely because shutdown bookkeeping now passes.


## Initialization comparison and candidate follow-up

The later protocol probe holds clean teardown constant and compares the original
cold reset against preserving a link already enabled/ready. Three short preserving
arms passed 60 cycles; three controls included one stall and two passing arms.
The 200-cycle binary `2d9e3a9c...` then stalled in reset mode on cycle eight, while
its preserving mode completed all 200 cycles. It logged zero actual cold resets,
200 preserved links, successful connections, empty stderr and constant 15,931
available pages. See `test-results/audio-init-protocol-200-native.json`.

The uninstrumented localized guard is now applied on SDK branch
`smw/ac97-ready-link` for actual game qualification. This does not inherit the
probe's acceptance, demonstrate physical cold-start fallback, or establish
compatibility with other BIOSes. The old probe source guard intentionally rejects
the changed SDK audio source; its baseline remains documented `14d5ee97...` and
its exact old executables/provenance remain preserved. No public change is posted.
