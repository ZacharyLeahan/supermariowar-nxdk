# Xbox release acceptance

Target: modern SDL2 Super Mario War using nxdk, a stock 64 MB application
budget, four local players, networking disabled, `default.xbe` plus pinned data.
“Shipped” means a reproducible, installable package with hash-bound results and
release notes. A build or callback counter alone is not full game acceptance.
The sharing plan is an asset-free source recipe; game assets stay external and
game media stays outside the public tree. Executable-only publication and the
external-data downloader workflow remain unresolved; no public upload has occurred.

## Open blockers

- [ ] Qualify cold boots and broader firmware behavior. The preserve-ready AC97
  probe passed200 cycles and current guarded game starts have advanced callbacks;
  this does not establish every cold-start path. Preserve failures by XBE hash.
- [ ] Hear/capture menu music, effects, match music, results and loop boundaries
  on the Xbox. KVM video and PCM counters do not perform this check.
- [ ] Qualify the complete current save format: non-default mode settings,
  goals, skins/pack choices and native malformed-record recovery on the final
  executable. Bounded records, alternating slots, controller persistence and
  host corruption/interruption checks are implemented.
- [ ] Complete repeated local-match/performance/memory regression with audio
  working throughout. Silent failed-audio runs cannot validate performance.
- [ ] Verify four actual controllers, ports, simultaneous input, reconnect and
  sensible behavior when a controller is missing. Four routed S3 XID devices
  now provide partial native coverage: Classic results on `8df74f38...`, and a
  completed 120-second four-human match/normal exit on diagnostic `c1dd5ced...`.
  Conventional pads, USB reconnect and the earlier possible long-run stall
  remain unqualified. See [current investigation](RUNTIME-STALL.md).
- [ ] Finalize the asset-free build recipe and dependency/checksum manifest,
  and verify the privately installed executable matches the accepted candidate.
  Do not include upstream media, upstream asset history or release binaries in
  the sharing repository.

- [ ] Finish exact-clean-candidate Health qualification. Native healing from three
  to five hearts and saved 6/10 limits passed on `84a132cd`; a deliberate pickup
  while already at full health remains unisolated. Clean `4addeb4e` retains the
  same runtime source but still needs its targeted Health regression. See
  [healing result](test-results/health-healing-native-20261106.json).

## Player-selector fix candidate (compiled; hardware qualification pending)

Candidate `58c7e48d8bc21fe9df46fc752ff7c16e66d203a56c2080aa087c1dd6b7eb7ff5`
(source `d1273c51e37d`) fixes duplicate actions in the shared Players selector
when an available pad aliases missing P1 for menu recovery. One P3 Down on the
previous candidate cycled Human→CPU→None, and Right skipped a player slot.
An ASan/UBSan regression against production selector code failed before the fix
and passed after it. The new clean XBE is compiled and independently reconstructed byte-for-byte
from the code-only recipe on the same host/toolchain, without game assets. Native
acceptance is still pending. Team/game input code is unchanged. All earlier native mode, save and
performance results remain bound to their original XBE hashes.

The separately observed P2 transition from configured in XBMC to unconfigured
after launch has not been isolated to nxdk, game code or controller firmware.
Do not merge that transport issue with the reproduced selector defect. See
[selector regression](test-results/player-selector-recovery-alias-20261109.json)
and [controller transition](test-results/controller-dashboard-game-transition-20261109.json).

## Clean baseline with native evidence

Candidate `4addeb4e...` is reconstructed byte-for-byte on the same host/toolchain.
Its installed-session XBE has passed boot, isolated P1–P4 readiness, a completed
four-human Classic match, two bounded simultaneous-input batches, non-silent
8-second PCM capture, empty stderr, zero logged music underruns, normal shutdown,
and corrupt/missing thumbnail-entry recovery. These are bounded results; they
are not all-mode, endurance, reconnect or listening acceptance. See
[match/audio/logs](test-results/clean-candidate-native-session-20261106.json),
[cache recovery](test-results/clean-candidate-cache-recovery-20261106.json) and
[exact build](test-results/clean-reconstruction-exact-20261106.json).

November 9 adds a completed four-human 120-second Time match on 1986, short
Frag/Jail/Coin smokes, and a completed two-on-two 60-second Time match on 0smw
with wired P1 plus Wi-Fi P2–P4. The team match ended in a reviewed 2–2 tie;
its concurrent 14.55-second transport sample had zero new send errors on all
four routes. Both match sessions exited normally with empty stderr, zero logged
audio underruns and clean audio shutdown. The earlier all-Wi-Fi Time sample
had ten new P1 send errors; retain that limitation separately.

The mixed-controller follow-up completed 17 further mode launch/input/exit
checks on 0smw (Stomp through Shy Guy Tag), with 17 native game entries, empty
stderr, zero logged music underruns and clean shutdown. Combined with the
prior clean-candidate Classic/Frag/Time/Jail/Coin records, all 22 modes now have
basic clean-candidate coverage. Chicken, Tag, Domination, Hill, Owned, Frenzy,
Survival and Phanto showed scoring or life changes. This is not full-objective
acceptance. An eight-second PCM capture overlapping Card Collection contained
56 left and 2 right full-scale samples (longest run six samples), despite zero
capture overruns; listening/clipping investigation remains open. See
[mixed-mode audit](test-results/mixed-mode-smokes-20261109.json).

A fresh packaged install into `/F/SMW-nxdk-dashboard-20261106` verified all
1,544 files by FTP readback, generated nine valid 160×120 thumbnail PNGs from an
absent cache directory, and saved/reloaded native preferences. Health defaults
were 6/10; temporary Extra Life 25% survived reload before being restored to
20%. This does not close objective-depth, reconnect, listening, cold-boot or
physical-stock-64-MB acceptance. See [fresh install](test-results/private-package-install-20261109.json),
[full Time match](test-results/clean-four-human-time120-20261109.json), and
[mixed team match](test-results/mixed-team-time60-20261109.json).

The interrupted November 6 session has been archived. On November 9 the KVM
initially failed WebSocket handshakes; restarting only the KVM restored control.
The initial reload navigation during a P1 handoff ended unexpectedly in gameplay;
its cause remains unassigned. Subsequent checks re-established and reviewed each
starting screen. Do not infer a game freeze from those rig interruptions.

## Existing evidence (scope matters)

- [x] Pinned nxdk SDL2 example compiled before porting the game.
- [x] Historical Xbox and desktop baselines audited; modern SDL2 base pinned.
- [x] Separate game port branch; no proprietary Microsoft XDK required.
- [x] Menu, input, one human + three CPU local matches and match results have
  run on a 128 MB Stellar Xbox. See per-executable `test-results` and workspace
  `logs/hardware`; these are milestones, not acceptance of every later XBE.
- [x] 64 MB **application budget** allocation probe passed on that upgraded
  console. Physical retail 64 MB hardware remains untested.
- [x] Schema-7 controller bindings survived save/reboot/load on XBE `0146f93a...`.
  Keyboard-prompt cancellation and absent saved-device menu recovery also
  passed with one virtual pad. See `test-results/controller-save-v7-*`.
  Four physical controller/device-port behavior remains unverified.
- [x] Initial volume preferences survived an actual in-game save/reboot/load
  round trip. The Xbox file-store probe also recovered a deliberately truncated
  newer slot. Non-default mode/goal/pack qualification remains open.
- [x] A guarded four-player match/normal-exit regression passed with active audio,
  zero recorded underruns, zero stderr and no watchdog recovery. The final
  packaged XBE must repeat that check. The subsequent clean optimized
  candidate also completed one match and normal Exit with zero stderr;
  its steady gameplay samples were 61.2, 61.2 and 60.9 FPS. See
  `test-results/optimized-match-exit.*`; broader release regression remains open.
- [x] Host sanitizer checks cover effects mixing, callback buffer boundaries,
  Ogg decode equivalence for 42 tracks and playback lifecycle.
- [x] Two audio-enabled game starts passed output-counter checks, including
  a menu-song loop. Later runs failed startup; that issue is still open.

- [x] Corrected startup watchdog completed a native stage-3 injected wait,
  wrote its fresh marker and returned to XBMC without controller reset. This
  proves a schedulable wait, not arbitrary kernel-hang recovery.
- [x] XBE `99884277...` loaded schema-8 generation 23, restored the AntiGuy skin,
  completed Classic five-life play with one virtual human + three CPU players,
  and exited normally with advancing audio, zero underruns and empty stderr.
  Five native malformed-newer-slot fixtures then loaded the older valid slot;
  every original save was restored byte-for-byte. See `guarded-v8-match-exit`
  and `save-v8-recovery-progress` results. Final-build broader save acceptance
  remains open.

## Final regression matrix

For one exact release XBE/hash and pinned dependency/assets revisions, record:

1. Repeated launches, menu navigation, player/team/character selection, map
   preview and options. Capture error logs and available memory.
2. Classic and timed local matches on multiple standard maps, including one
   human + three CPU matches, repeated returns/results and track changes.
3. Movement/jump, run/grab, stored items, pause/resume and recovery. Record
   which controls were exercised physically and which through virtual input.
4. Sustained gameplay/audio, full music loops, volume/mute combinations, no
   unbounded memory growth, callback stalls or unexplained underruns.
5. Settings/save round trips and failure recovery, followed by a reboot and
   another match. Preserve evidence of corrupt/truncated-file handling.
6. Four-controller hardware session and audible output review. Do not silently
   replace those checks with CPU players or host/emulator execution.
7. Clean rebuild, install into a separate FATX-safe folder, verify all package
   hashes and reproduce launch from the documented commands.

Report compiler, emulator and real-hardware outcomes separately. Emulator
coverage is useful but is not currently performed and cannot substitute for
hardware-specific acceptance. Physical stock-64-MB testing should be recorded
when available; the upgraded-console memory-budget probe is a narrower claim.


Recovery policy for the next candidate: retain the 45-second startup and
10-second exit watchdogs, remove on-screen diagnostic counters, and preserve
failure-only logs. Bounded recovery is useful release behavior; a watchdog
activation is still a failed test. Qualify the exact resulting XBE rather than
requiring recovery guards to be removed. Hearing and four actual controllers
remain independent acceptance checks.

## Clean-screen candidate ec21c4f7

Exact source8d6fcf89 and SDKc5c574c6 passed five observed warm starts, a full
600-second Time Limit match with one virtual human and three CPU players,
pause/resume, results and normal Exit. Recorded zero underruns, empty stderr,
18 decoder loops and 20424 KiB minimum available memory. Invalid-controller
newer-slot fallback and both-bad-CRC defaults recovery passed; originals were
restored exactly. A full SDK/game clean rebuild reproduced the exact XBE.
See the hash-bound `test-results/clean-ui-ready-*` records.

The packaged candidate and refreshed source bundles have complete local
checksum/archive readback verification. Fresh-folder Xbox installation remains
pending. The separate raw-frame observer6141 froze at initial Main; its result
is failed/unqualified, with logs pending physical recovery. It does not change
ec21 acceptance. Menu-border investigation remains open. Hearing, four physical
controllers, cold boots/other firmware and physical stock64MB remain unverified.

## Opaque-copy candidate8df74f38

Source0476190f, SDKc5c574c6, five production flags (preferences, failure-only
DMA logging, compiled clean audio teardown,45s startup/10s exit guards).
Capture, forced spin, HUD and initialization-state dumps are off.

- [x] Rendering defect reproduced independently in pinned SDL2.0.9; unkeyed
  RGB565 RLE bypass removes skipped black pixels. Forced-spin observer frames
  match exactly after returning to Main. That observer is a separate hash.
- [x] Opaque no-pixel-alpha sprites use NONE at alpha255. Host pixel comparison
  passes both keyed/unkeyed cases; exact8df74 records60.1-60.2FPS menus.
- [x] Full SDK/game clean rebuild reproduces exact8df74; named branch restored.
- [x] Exact8df74 invalid-controller fallback and both-corrupt-slot defaults
  recovery pass on hardware, original saves restored byte-for-byte.
- [x] Fresh/F/SMW-nxdk-final-test installation verifies every1512 files by
  readback before launching. Classic5 on0smw reached Player2Wins with2 lives.
- [x] Completed600s Time1-3 match and normal Exit, exact logs archived:0
  underruns,7 decoder loops, empty stderr, clean audio close. Final8 memory
  samples stable15940KiB; steady returned menu60.2FPS.
- [x] Reloaded goals5/600, extra time0 and onehuman/3CPU slots; completed
  another Classic5 match, normal Exit,0 underruns and empty stderr. See
  opaque-copy-classic-and-ten-minute and opaque-copy-reload-and-match results.

Human listening, four physical controllers, physical stock64MB and broader
cold/firmware coverage remain unverified. No emulator run or public release
has been performed. Candidate88385's10-minute run passed audio/gameplay but
had slow menus; its acceptance must not be silently transferred to8df74.

## November 6 save fallback follow-up

Candidate `84a132cd...` rejected a deliberately bad CRC in newest save generation
72 and loaded valid generation 71 on the real Xbox. Normal menu exit completed
with empty stderr; both original save slots were restored and readback verified.
This covers one-slot CRC recovery, not both-invalid or semantic corruption. See
[test result](test-results/save-bad-crc-native-20261106.json). A clean rebuild with
explicit original display stamp and watchdog flags reproduced the candidate XBE
SHA-256 exactly; independent recipe reconstruction remains to be qualified.

Both-slot CRC rejection also passed on candidate `84a132cd...`: defaults loaded,
normal exit committed generation 1, empty stderr, originals restored/readback.
See [result](test-results/save-both-bad-crc-native-20261106.json).

### Clean-build correction

The original-workspace matching rebuild above retained stale JPEG objects: nxdk
`lib/libjpeg/Makefile` used `LIBJPEG_OBJS` instead of `LIBJPEG_TURBO_OBJS` in its
clean recipe. A fresh independent recipe build succeeded with XBE SHA-256
`177753bb5d7d389d8d500b6bc8ee316f662007f7b9444e60263ef1ec199c938b`,
3,473,408 bytes; it is **compiled only**, not hardware-qualified. The original
3,584,000-byte candidate remains the source of recorded native results. The SDK
clean-rule fix is included in the lean recipe; clean reproducibility and native
regression must be repeated before promotion. Embedded fmt assertion paths also
differ between workspace locations. No byte-identical independent-build claim
is made.

### Exact clean reconstruction verified

After fixing JPEG cleanup and normalizing compiler macro paths (with explicit
forward-slash behavior), the original clean build and independent recipe build
produce identical `default.xbe` SHA-256
`4addeb4e4e32def7a77125edf1815d3c1065835239fd431a1afca39f844803a9`.
Both are 3,473,408 bytes. This is same-host/toolchain reproducibility, not
cross-host qualification. The clean candidate has a real-Xbox title boot;
prior detailed gameplay results belong to older executables. See
[exact-build result](test-results/clean-reconstruction-exact-20261106.json).
