# Intermittent SDL2 output startup stall — investigation draft

This is a local report draft, not a submitted GitHub issue. The fault has been
observed in Super Mario War, but attribution to nxdk, SDL2, the application,
BIOS or launch environment is still open. A minimal standalone probe is in
`probes/audio-startup`; preserve per-launch logs, including successful starts.

## Environment

- nxdk `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`, unchanged tracked source.
- nxdk-sdl `2554902f7bbf469449216ee25f88016421271cd8` (SDL 2.0.9).
- Original Xbox, stock 733 MHz CPU, upgraded 128 MB RAM, StellarOS Beta 1.8.7.
- Title 64 MB limit set; allocation probe previously verified the restricted
  application budget. Physical retail 64 MB hardware remains untested.
- Warm launches from XBMC4Xbox Redux 4.0-80c44fdf-py2 using FTP `SITE RunXBE`.
- One ESP KVM virtual Xbox controller. KVM firmware can reboot independently;
  treat this as an environmental variable, not an excluded cause.
- No emulator or speaker-output verification for this investigation.

## Observations

SDL audio initialization and opening succeed with 48 kHz, signed 16-bit stereo,
1,024 frames. In failing game runs, the application callback never executes,
even after minutes. The decoder fills its bounded PCM ring; music consumption,
effect completion and non-silent callback counters remain zero. Video, input
and local matches continue. Zero underruns in this state is not success.

Starting output before the decoder worker and allocating a 256 KiB decoder
stack was followed by two successful runs. A subsequent cached-mixing build
failed the same way, so those changes did not establish startup reliability.
Do not call the later 61–62 FPS measurements an audio optimization result:
there was no active audio callback during that match.

| Executable SHA256 prefix | Source | Result |
| --- | --- | --- |
| `fc5cd6f7fd07` | `964bb665` | Two starts passed; match and menu loop, zero underruns |
| `5ef1a48cf712` | `53292ebf` | Callback zero for the entire four-player match |
| `aa61cfba03e9` | `b25fb8e6` | Menu output passed; startup DMA progress logged |

The full executable hashes and compact logs are in `test-results/audio-*`.
Full game logs/screenshots are in the parent workspace's `logs/hardware`.

## Diagnostic experiment

The diagnostic build logs analog/digital current-index, last-valid-index,
status and control registers after startup. A normal start showed both engines
at CIV=5, LVI=6, status=0, control=29, with five application callbacks after
100 ms. If callbacks are zero, the experiment pauses/restarts through nxdk's
public backend API once and logs another sample. The observed diagnostic run
started normally, so it did not exercise or validate this recovery path.

The pinned SDL driver queues buffers and starts XAudio in `OpenDevice`; its
mixing thread subsequently queues a buffer and waits on a semaphore posted by
the XAudio callback. This explains how lack of semaphore progress could block
future application callbacks, but it does not establish why progress stopped.
Relevant source: `lib/sdl/SDL2/src/audio/xbox/SDL_xboxaudio.c` and
`lib/hal/audio.c` in the pinned SDK checkout.

## Next discriminating tests

Run the standalone silent probe repeatedly from the same dashboard and archive
one log per launch. A zero-callback reproduction without the game/decoder would
narrow the scope substantially. Capture DMA status during a failing start;
compare cold boot and warm launch, then another BIOS or console if available.
Do not reopen/close a stalled SDL device blindly: the driver's unbounded wait
may prevent orderly shutdown. Keep a bounded reboot path and a known fallback.

## Standalone baseline result

The first probe run (`test-results/audio-startup-probe.*`) logged 476 callbacks
at ten seconds with advancing analog and digital DMA indices. It did not
reproduce the intermittent failure and does not establish startup reliability.
The probe rebooted back to XBMC automatically.

Game DMA register logging/restart is opt-in: run the workspace
`scripts/build-game.sh AUDIO_DIAGNOSTICS=1`. The ordinary build does not call
the HAL restart experiment. Keep each variant's XBE SHA256 beside its logs.

The port Makefile now tracks game compiler flags in an ignored configuration
stamp and rebuilds game objects when flags change. SDK library flag changes
still require an SDK clean rebuild. Earlier experiments explicitly removed
affected objects before switching flags.

## Deliberate HAL drain baseline

`test-results/audio-hal-drain.*` records eight silent 4 KiB submissions with
100 ms gaps, bypassing SDL and allowing each 21 ms buffer to drain. All eight
completion notifications arrived. The simple hypothesis that a drained HAL
queue always loses subsequent callbacks was not reproduced; do not patch the
SDK on that assumption. SDL-level startup timing and callback/semaphore progress
remain the next discriminating evidence.

## Repeated starts after adding the goal

Three further standalone SDL probe starts passed (476 callbacks at ten seconds
each), followed by three game observer starts with matching backend/entry
progress and consumed semaphores. See `test-results/audio-startup-repeat.*`
and `audio-observer-repeat.*`. Crucially, the exact earlier failed executable
`5ef1a48cf712...` then passed two title-screen starts without recompilation
(`audio-cached-reference-repeat.*`). The fault can vary between launches of
one executable; a different binary is not itself a fix. These successful
starts do not close the known startup defect.

The backend observer replaces only the HAL callback target with an integer
counter followed by the original SDL callback; this may affect timing. The
separate `AUDIO_FAILURE_DMA_LOG=1` variant makes no callback replacement and
reads DMA registers only after a zero-callback startup. Both are opt-in and
leave ordinary builds free of these experiments. The game configuration stamp tracks flag changes. Preserve a hash for each variant and every failed start.

## Failure captured after optimized SDK rebuild

`test-results/save-recovery-unsupported-schema.*` captures the failure on the
optimized SDK build, XBE `057c8de21cdbc5d9c99e9d1fc0cb08b6f9bb5020c93dfc54e1de9ac5ac82be4d`
(source `89ccc01a`). All four failure-only samples show both analog and digital
CIV=0, LVI=1, status=0 and control=29. SDL reports playing; application callbacks
remain zero through the 30-second title-screen observation. The sampler reads
registers only and does not restart HAL or replace the backend callback.

The concurrent save fixture loaded the correct older generation 13, but the
overall test failed because stderr reported the audio startup stall (73 bytes).
Both original save files were restored with exact readback. Save fallback and
audio startup are recorded separately; this run is not a clean acceptance pass.

This disproves optimization as a startup fix. Register state narrows the next
measurement but does not identify the cause: capture busmaster base addresses,
remaining sample counts, global control/status and queued descriptors before
attributing the stall to missing interrupts, initialization timing or DMA data.

The failure-only logger now also reads global control/status, analog/digital
busmaster base addresses, remaining sample counts, prefetched indices, expected
HAL descriptor addresses and the first two descriptor contents. These reads
remain opt-in and failure-only. It does not write hardware, queue samples, replace
callbacks or claim recovery. A failure with the extended logger has not yet been
captured.

## Related implementation references

The [raylib4Xbox patch notes](https://github.com/Memorix101/raylib4Xbox/blob/main/README.md)
report a different XAudio backend starting DMA with empty descriptors. The pinned
nxdk SDL2 driver already queues one silent buffer before `XAudioPlay`, so this
report is not evidence that the same defect explains this game's stall.

Intel's [ICH7 AC97 register manual](https://www.intel.la/content/dam/doc/manual/io-controller-hub-7-hd-audio-ac97-manual.pdf)
describes an inherited AC-link shutoff control bit. Xbox MCPX behavior must be
verified independently; currently this is a measurement question, not a root
cause or a patch. The standalone probe now records raw global control/status
before and after initialization to compare with captured game failures.

## Extended failure captured

`test-results/audio-extended-failure-repeat.*` records four normal starts followed
by a fifth stalled start of the exact same XBE (`88397100df062ffaddb0ed0bb594423b661fb749a2eb2f4c2fb4529b5271d14d`,
source `8233f2cf`). On the failed start, analog/digital remaining counts stay at
1680/1712 across all four samples. Both current indices remain 0, last-valid
indices 1, status 0 and control 29. The programmed base addresses match the HAL
arrays; both queued descriptors have nonzero addresses, 2048 samples and control
32768. This is not an empty-descriptor start.

Global control is 2, so the proposed inherited bit-3 shutoff state is absent.
Global status is 0x00300000, lacking the 0x100 mask that XAudioInit waited for
before starting DMA. A healthy/inherited-state comparison is the next check.
The evidence shows frozen hardware progress with partially consumed buffers;
it does not yet establish the cause or justify a release fix. No SDK source
was modified for these five launches.

## Healthy register comparison and gated warm-reset experiment

The new standalone native probe progressed to 485 callbacks at ten seconds.
Before SDL, after OpenDevice and throughout playback, global control was 2 and
status 0x00300100. The failed game had status 0x00300000, with frozen nonzero
remaining counts. This is a useful correlation, not proof of why ready state
was lost.

`AUDIO_LINK_RECOVERY=1` is a new default-off experiment. Only after zero startup
callbacks and a missing 0x100 status mask, it pauses the two DMA engines, requests
one AC-link warm reset through control bit 2, waits at most 500 ms for the request
to clear and the ready mask to return, then resumes and records real callback
progress after 100 ms. A timed-out request is canceled to retain the initial
control value. It neither replaces callbacks nor posts fake completions or new
descriptors. The original failure samples and stderr remain preserved even if
progress returns. This protocol requires hardware testing; it is not an accepted
SDK fix or release behavior. The pinned SDK source is still unchanged.

The warm-reset experiment now checks a second 100 ms callback window. A nonzero
initial callback is insufficient if output subsequently stops (the inconclusive
close fixture had one callback and no later notifications). Recovery is still
limited to stagnant startup counts plus the missing ready mask. If callbacks
resume during observation, the reset is skipped. The close fixture now verifies
ongoing progress before injecting its notification fault and logs raw DMA state
when startup stops first.

## Ready-mask condition is not shared by every stall

Run 2 of the preserved `ea938e36...` experiment stalled with global status
0x00300100 throughout all four samples: the ready mask was present. Analog and
digital remaining counts stayed at 1984/2016, with CIV0/LVI1 and control29.
Callbacks remained zero. The conservative prototype skipped its warm reset, so
this run provides no recovery result. See
`test-results/audio-link-recovery-ready-stall.*`. The missing ready bit cannot
serve as a necessary condition for identifying or recovering every startup
stall. The ongoing ten-run batch remains bound to its original executable.

## Revised experiment trigger (v2)

Because the ready mask can be present during a real stall, the next default-off
experiment no longer uses that bit as an entry requirement. It requires
unchanged application callback counts and unchanged analog/digital current-index
plus remaining-sample positions across an additional 50 ms, with both DMA run
bits set. If either callbacks or DMA advances, a device is paused, or a warm
reset is already pending, it skips the protocol. Warm reset remains one bounded
attempt, with real callback and DMA positions logged afterward.

This revision is a separate executable and does not change the active ten-run
v1 batch. No native v2 recovery is claimed until a frozen startup actually
exercises the protocol and subsequent output progress is observed.


Five v2 warm launches completed with ongoing callbacks and zero stderr. None
encountered frozen startup or exercised warm reset, so this batch establishes
no recovery result. See `test-results/audio-link-recovery-v2-repeat.*`. The
startup issue remains open; audible output and cold boot were not tested.

Ten additional warm v2 title-screen launches all progressed with zero stderr
(`test-results/audio-link-recovery-v2-extended.*`). Combined with the earlier
five, this is 15 progressing starts of that exact executable. None exercised
the reset protocol. This does not validate recovery or establish a root cause,
and is not acceptance of later controller-save builds. Startup remains an
open release gate, including cold-boot and exact release-XBE qualification.

## Failure on the controller-save candidate

The fourth interactive launch of XBE `0146f93a...` (source `bfac4a33`, warm-reset
experiment disabled, SDL shutdown patch linked) stalled with zero callbacks.
All four failure samples had ready mask present (global status 0x00300100),
CIV0/LVI1, control29 and both remaining counts fixed at 2048. Descriptor bases
matched the HAL tables; the first two descriptors referenced contiguous audio
buffers with 2048 samples and completion flags. Unlike earlier partial-consume
captures, these samples showed no first-buffer consumption.

Gameplay/settings still ran and normal unguarded Exit returned to XBMC without
an exit-watchdog marker. Stderr contains the startup failure, so the Classic
match in this session does not qualify audio or performance. Independent
non-default setting/skin/goal reload evidence remains scoped separately in
`test-results/controller-v7-preference-reload.*`. This confirms that the prior
15 progressing starts of another executable did not resolve the startup gate.

## Next experiment: v3 startup DMA reset

The v2 warm-reset protocol was finally exercised after the current-candidate
failure: callback counts and both DMA positions stayed frozen despite ready=1
after the request. This is a failed recovery trial, not an unexercised protocol.
Full batch evidence is being preserved separately.

The next `AUDIO_LINK_RECOVERY=1` executable uses protocol v3: reset only the two
bus-master DMA engines, then restore their existing descriptor bases and LVI1.
It does not issue a codec warm reset. Entry requires zero callbacks, unchanged
positions for 50 ms, both RUN bits, CIV0/LVI1/status0 on both engines, HAL
nextDescriptor2, matching physical descriptor bases and the two expected paired
2048-sample startup buffers with IRQ flags. A reset-bit wait is bounded to
500 ms. It changes neither HAL counts/nextDescriptor nor SDL callback/semaphore
accounting, and submits no new buffers or fake completions. It only logs real
callback/position progress after restarting. Initial failure stderr is retained.

This narrow, default-off experiment has not been qualified; it cannot be used
for arbitrary mid-track stalls, a progressed queue, or release acceptance.

The completed three-run v2 batch (`test-results/audio-link-v2-exercised.*`)
contains one actual frozen-startup reset attempt and two healthy starts. The
real attempt remained at zero callbacks after 100 ms and throughout later
observations. Both positions stayed at 2048 despite ready=1. No recovery was
observed. The subsequent v3 executable is a separate hash and protocol.

## Read-only PCI configuration capture

The next failure-logger executable also reads PCI configuration via
`HalReadWritePCISpace(..., FALSE)`. It scans raw HAL slot encodings on bus zero
and logs multimedia-class functions or a function whose memory BAR matches the
SDK's AC97 MMIO base 0xFEC00000. It records IDs, command/status, class/revision
and all six BARs without assuming an unverified device ID or changing any PCI
configuration. In the game, scanning happens only after startup already failed;
healthy-startup timing receives no extra PCI scan. Reads can still affect later
observation timing, so this is a separate diagnostic executable.

The minimal `audio-startup` probe records the same metadata before SDL, after
opening audio and after playback for a healthy/failed comparison. These new
variants require native tests. No PCI setting is changed or presumed to be
the root cause. The extended v3 batch remains bound to its preserved prior XBE.

The first v3 extended batch exercised its reset on completed run 2. Hardware
remaining counts changed from 1696/1728 to 2048/2048, but status stayed
0x00300000 and callbacks stayed zero through later samples. Thus DMA reset
alone did not recover this stall (`test-results/audio-dma-restart-v3-exercised.*`).
The planned batch was stopped after this negative result; the run in dashboard
polling was manually archived and excluded from acceptance statistics. No
recovery protocol is enabled in the normal build.

## Early startup hang and recovery ordering

The first PCI-logging XBE (`e00bb91f...`, source `d453631a`) did not return logs.
Video remained at the early loading screen, and the normal game reset chord
could not bring back dashboard FTP. BIOS reset combinations also did not restore
XBMC; their exact effect is unverified. No PCI values or exact stopped stage
were recovered, so neither the PCI scan nor audio initialization is established
as the cause. The installed `default.xbe` was not replaced.

Before launching any new test after a physical power-cycle, retrieve the old
`F:\SMW-nxdk-test\smw.log` and `smw-error.log` plus both save slots and verify
the `audio-observer.xbe` hash. Do not let another runner clear these old logs
first. Preserve this failed attempt rather than assuming it was a healthy boot.

Future PCI reads require `AUDIO_PCI_LOG=1` explicitly, alongside
`AUDIO_FAILURE_DMA_LOG=1`. Ordinary failure logging does not scan PCI. The next
PCI helper limits reads to candidate raw slots 5/6/0x28/0x30 and individual
four-byte accesses, logs each candidate before reading, and still validates the
MMIO BAR instead of asserting a device identity. This narrower helper has not
been qualified on hardware.

`STARTUP_WATCHDOG_MS=45000` is a new default-off diagnostic. A WinAPI thread
checks progress independently of SDL; after the timeout it records
`startup-watchdog.log` and requests reboot. Stage IDs: 0 setup, 1 resource lists,
2 graphics init, 3 audio init, 4 PCI diagnostic, 5 input init, 6 preferences,
7 initial graphics/sounds, 8 ready. The splash loader marks ready only after
initial asset loading completes; the watchdog then exits. A marker invalidates
boot/clean-exit acceptance. Runners remove stale markers and archive a fresh one.

This is best-effort recovery for ordinary blocked initialization, not a power
controller: interrupt-disabled kernel hangs or blocked filesystem operations can
prevent its thread or marker writes from completing. It is not hardware-tested
yet. Do not deploy another unguarded diagnostic while the console lacks remote
power/reset control. The next diagnostic should start with PCI logging disabled
to observe basic audio initialization first.

The first resumed hardware action must be archival, not a launch:

```sh
python3 ports/nxdk/tools/recover-game-logs.py \
  --output ../../logs/hardware/recovered-early-startup-new
```

The title launch and save-fixture helpers check the workspace recovery record
and refuse to clear old files until this read-only collector archives them and
verifies the expected remote XBE hash. Missing files are recorded explicitly.
A program mismatch leaves the gate set. Host fake-FTP integration checks byte
preservation, no remote writes, missing-file reporting and wrong-XBE refusal.

Before another uncertain startup diagnostic, qualify the watchdog itself using
`STARTUP_WATCHDOG_MS=15000 STARTUP_TEST_HANG_STAGE=3`. This default-off fault
case deliberately sleeps before audio initialization, without changing saves.
Launch it with the interactive helper, send no controller reset, and require a
fresh stage-3 watchdog marker plus autonomous return to XBMC. Its timeout is an
expected diagnostic outcome, not a successful game boot. Then test the normal
observer with `STARTUP_WATCHDOG_MS=45000`, fault injection and PCI reads off.
No hardware qualification of these new safeguards is claimed yet.


## Recovery and watchdog qualification after physical restart

The earlier missing-log state above is superseded by read-only archival in
`logs/hardware/recovered-early-startup-20261016`. The observer hash matched
`e00bb91f...`. Audio opened but startup callbacks were zero; stderr was empty.
PCI reads logged raw slot 5 (`01b010de`, command/status `00b00006`) and slot 6
(`01b110de`, command/status `00b00007`, BAR2 `fec00000`). The latter has PCI bus
mastering enabled at that sample. Logging stops after slot 6; the exact hang
location remains unknown. This does not establish PCI enumeration or the codec
as the cause. No further broad PCI scan has been launched.

The first watchdog fixture returned to XBMC but lacked its marker: its Windows
path had an unescaped backslash. Commit `bdbd3349` fixes that defect. The fixed
fixture XBE `ac332c9c...` wrote `startup watchdog timeout stage=3` and returned
to XBMC without a controller-reset command; both saved preference files stayed
byte-identical. This qualifies one schedulable injected wait, not every kernel
or filesystem failure. See `test-results/startup-watchdog-native.json`.

Normal guarded observer `99884277...` (same source, 45-second watchdog, no fault,
PCI or audio-recovery experiment) completed menu/options/normal Exit and a
subsequent schema-8 reload/Classic match/normal Exit with progressing audio,
zero recorded underruns and empty stderr. Five schema-8 malformed-newer-slot
fixtures also loaded the older valid save and restored both original files.
These successes do not erase prior intermittent startup failures or establish
an audio fix. Speaker output remains unconfirmed for these exact runs.

`AUDIO_INIT_STATE_LOG=1` is a separate optional read-only diagnostic, compiled
as `b7ba8269...` from `8635eef3`. It records AC97 control/status, DMA bases,
positions and remaining counts before SDL audio initialization, after open,
and after the startup callback window. The fields match the existing silent
probe. It performs no PCI scan, descriptor read, register write or callback
replacement. Collect failed and healthy observations before proposing another
reset change; this extra logging can itself alter startup timing.


## Paired pre-start settling experiment (not a fix)

The HAL trace's first healthy run registered its interrupt successfully, returned
success from the reset delay, and retained codec-ready status while reset was
asserted. That status observation alone is not proof of a reset defect. The
SDK starts DMA after resetting the link and polling ready, with no explicit
post-reset settling interval. As a comparison only, Linux v6.12's optional ICH
cold-reset helper sleeps 1 ms after deasserting reset:
https://github.com/torvalds/linux/blob/v6.12/sound/pci/intel8x0.c
This is a different driver and does not establish an Xbox requirement.

`AUDIO_START_SETTLE_EXPERIMENT=1` builds a local copy of the selected unchanged
HAL with wrappers around initialization and first playback. One identical XBE
uses `D:\audio-settle.txt` containing exactly `0` or `1000` (optional LF). The
sidecar is read before audio initialization. Only the 1000-us arm calls
`KeStallExecutionProcessor` before first DMA playback; both arms measure counter
ticks and log after starting playback. No new register writes, buffers,
callbacks or recovery protocols are introduced. Missing/invalid settings select
zero; invalid contents also produce stderr and must not count as a trial.
A watchdog is mandatory; HAL tracing and link recovery cannot be combined.

`run-audio-settle.py` runs 0/1000/1000/0 order per cycle using the same executable,
verifies sidecar bytes and logged selection/actual wait, archives each run and
restores the original sidecar. It stops on missing logs, a watchdog or measured
undershoot; missing logs set the recovery gate before any further launch.
Fake-FTP tests cover ABBA order, same executable bytes, restoration of a present
or absent sidecar, mismatched markers and recovery gating. Baseline timing is
still affected by wrappers/measurement. If the zero-delay arm does not reproduce
the stall, results are inconclusive about prevention regardless of delay-arm
success. This experiment is absent from the packaged game.


## Paired close-path experiment

`AUDIO_CLOSE_EXPERIMENT=1` compiles the selected, unchanged HAL audio source
locally and interposes its callback-NULL close call. It requires startup and exit
watchdogs. The same executable reads `D:\audio-close.txt` before opening audio:
`legacy` retains the SDK behavior; `clean` stops DMA, disconnects the owned
interrupt, removes queued completion work, resets DMA engines only, clears DMA
bases, and balances the HAL's buffer-page locks. It preserves codec global
control. This is a diagnostic for the pinned single-device Xbox implementation,
not a general public HAL shutdown API. A bounded reset or disconnect failure
writes `audio-close-failure.log` and requests a reboot before buffers are freed.
Such a reboot must never count as successful cleanup or prevention.

The first native legacy trial (`6d283fa3...`, source `fcdf1376`) started with
four callbacks during the startup window and an accepted interrupt connection.
On normal Exit it reinitialized that already-connected interrupt; the second
`KeConnectInterrupt` returned zero, and the interrupt object's Connected field
was zero afterward. Dashboard return was observed, stderr was empty, and no
watchdog or close-failure marker was present. Evidence is under
`logs/hardware/audio-close-legacy-first/after-normal-exit`. This demonstrates a
rejected connection during legacy shutdown, not yet the cause of subsequent
intermittent startup stalls. The clean arm still needs native qualification.

Compare the next startup against the *previous* close mode. Require a healthy
starting run, successful cleanup without reboot fallback, and repeated launches.
Normal Exit uses PDCLib's quick reboot path; the game's Back+Start handler asks
for XReboot. Firmware may intercept that chord, so record the observed reset
method rather than assuming precisely which firmware code ran. Prior same-XBE
HUD trials include failures followed by healthy launches after Back+Start, but
also a healthy launch following normal Exit; reset-path causality is unproven.
The read-only platform register at offset 0x17c was 0x02000000 in healthy and
failed samples, which provides no basis for a magic-value write.


The subsequent silent reopen sequence is decisive about the limits of cleanup.
One clean arm completed 20 cycles; the same-XBE legacy arm stalled in cycle six
and hung during close until its watchdog rebooted. A clean reversal then stalled
in cycle four with KeConnectInterrupt returning one; its clean close still
returned in 146 ms without a watchdog. Thus interrupt ownership/teardown and
startup reliability are separate concerns. Do not claim the clean path fixes
intermittent startup. All three logs and qualifications are preserved in the
`audio-reopen-*-native.json` results.

`AUDIO_CLEAN_SHUTDOWN=1` is a separately compiled game candidate that selects
clean close at build time and ignores the diagnostic sidecar. It is mutually
exclusive with `AUDIO_CLOSE_EXPERIMENT=1`; both still require startup and exit
watchdogs. This removes a runtime configuration dependency for future candidate
testing, not the unresolved initialization defect. It has compiled; no native
qualification of that new game executable is inferred from earlier binaries.
