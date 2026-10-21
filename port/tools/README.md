# Hardware audio test loops

`run-audio-probe.py` handles the silent, auto-rebooting standalone probes. It
uploads to separate probe folders, verifies the XBE readback hash, removes the
previous probe log before launch, then archives one fresh log and manifest per
run. It stops if a log cannot be recovered. A retrieved log is not automatically
a passing test: inspect callback/DMA progress, completion markers and errors.

`run-game-startup.py` uploads the observer as `audio-observer.xbe` beside the
installed game/data, preserving `default.xbe`. It waits 30 seconds after launch,
uses an explicitly supplied Node/KVM helper for the game's Back+Start reboot,
and retrieves fresh game/error logs. It requires the existing test install at
`F:\SMW-nxdk-test`. Node and reset-helper paths are explicit prerequisites;
the helper must implement the authorized controller reset and release buttons.
It does not configure the KVM, BIOS or network, and it does not listen to audio.

From the workspace, build the game observer with
`scripts/build-game.sh AUDIO_DIAGNOSTICS=1`. The port configuration stamp now
tracks game compiler-command changes. SDK library flag changes still need a
clean SDK rebuild. Run the helper from the game checkout:

```sh
python3 ports/nxdk/tools/run-game-startup.py --count 3 \
  --node /absolute/path/to/node --reset-helper /absolute/path/to/reset-helper.mjs \
  --output /absolute/path/to/evidence
```

To repeat a preserved reference binary rather than the current observer, add
`--xbe /path/to/reference.xbe --reference-manifest /path/to/result.json`.
The result must contain the matching `xbe_sha256` and earlier `source_commit`.
This allows testing the exact previously failed executable without recompiling
it or assuming the current checkout describes its build. Source dirtiness is
unknown for such a historical reference unless its original manifest says so.

Both scripts default to the dashboard's `xbox`/`xbox` FTP credentials, overridable
with `XBOX_FTP_USER`/`XBOX_FTP_PASSWORD`. They use bounded waits and retain failed
runs. They intentionally do not publish issues, releases or test claims.


## Guarded audio initialization observations

`AUDIO_INIT_STATE_LOG=1` adds read-only AC97 register snapshots before SDL audio
initialization, immediately after opening the device, and after the 100 ms
startup callback window. Use with `AUDIO_FAILURE_DMA_LOG=1` and
`STARTUP_WATCHDOG_MS=45000`; leave `AUDIO_PCI_LOG` and `AUDIO_LINK_RECOVERY`
disabled. It changes diagnostic timing, so record it as a distinct XBE.

The controlled watchdog fixture at source `bdbd3349` has native evidence in
`test-results/startup-watchdog-native.json`: a stage-3 wait produced a fresh
marker and autonomous XBMC return. That verifies one schedulable wait, not
recovery from every kernel or filesystem hang. The earlier marker-path bug is
fixed; use the corrected fixture when qualifying a new environment.

Preserved builds and per-run provenance are under workspace `artifacts` and
`logs/hardware`. A compiled observer does not establish that the audio failure
is fixed. A watchdog recovery is a failed boot, never a startup pass.


`AUDIO_HAL_INIT_TRACE=1` additionally compiles the selected SDK's original
`hal/audio.c` as a local object with two call wrappers. It logs the return value
of `KeConnectInterrupt` (normally discarded by the driver) and control/status
around its existing reset delay. It changes no reset writes or return values,
adds no retry, and performs no PCI scan. A startup watchdog is mandatory. The
local object supplies the full XAudio symbol set, so the SDK archive's audio
object is not pulled in. The SDK source/archive stay unchanged. This variant
requires separate native evidence because logging affects timing; do not infer
a cause from the existence of an unchecked return value. Shutdown can call
`XAudioInit` again, so distinguish the first connection from later close calls.

## Fresh package installation

`install-package.py --package PATH --directory /F/NEW-TITLE --output NEW-LOG-DIR`
checks the local inventory and executable manifest, refuses an existing title
folder, and hashes each Xbox file after upload. `--launch` requests execution
only after every file verifies. A partial failure leaves the new folder and
its local installation record for inspection; it never deletes existing titles.
Host fake-FTP tests: `python3 ports/nxdk/tests/run-install.py`.

After returning to XBMC, use `collect-game-logs.py --directory /F/NEW-TITLE
--reference-manifest BUILD-INFO.json --output NEW-LOG-DIR` to retrieve evidence.
It verifies the remote default.xbe before downloading title logs and records
missing markers/save slots. `--program` can select a separately staged observer.
This read-only collector makes no runtime-acceptance claims. For a pending
failed-startup recovery gate, use `recover-game-logs.py` first as documented;
this generic collector does not unlock that gate.

## Paired audio shutdown diagnostics

Build the optional game interposer with the normal versioned game wrapper:

```sh
ports/nxdk/tools/build-game.sh PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1 \
  AUDIO_INIT_STATE_LOG=1 AUDIO_STATUS_HUD=1 AUDIO_CLOSE_EXPERIMENT=1 \
  STARTUP_WATCHDOG_MS=45000 EXIT_WATCHDOG_MS=10000
```

Preserve `bin/default.xbe`, `BUILD-INFO.json` and `provenance` together before
launching. `audio-close-mode.py --state NEW-STATE-DIRECTORY --mode legacy|clean`
sets and reads back only `audio-close.txt`, saving its original contents.
Use that same state directory with `--mode restore` when done. It refuses
outside edits and completed/foreign state directories. `--directory` selects a
dedicated probe folder; the default is the existing game test folder.

The mode is read before audio opens and affects that run's *close*. Attribute
its next launch to the previous run's close mode. Do not use Back+Start for a
normal-close trial: it bypasses the tested game shutdown. Confirm advancing
callbacks before exit, archive logs and verify the cleanup-completion line,
empty stderr, and absence of startup/exit/cleanup failure markers. Dashboard
return by itself is insufficient.

`probes/audio-reopen/README.md` documents a separate silent 20-cycle SDL device
lifecycle fixture. Build it with `build-probe.sh audio-reopen` and use
`run-audio-probe.py audio-reopen --count 1 --output NEW-EVIDENCE-DIRECTORY`.
The runner verifies the executable, archives fresh logs and error/watchdog
markers, stops a batch when a watchdog/cleanup failure appears, and sets a
recovery gate if it cannot retrieve the probe log. Recovery now also preserves
platform probe logs before unlocking that gate.

Host checks:

```sh
python3 ports/nxdk/tests/run-audio-close-mode.py
python3 ports/nxdk/tests/run-hardware-recovery.py
python3 ports/nxdk/tests/run-probe-runner.py
```

The last test requires a compiled reopen probe for fixture provenance; it uses
fake FTP and never contacts the Xbox. Native success and audible quality remain
separate checks. See `AUDIO-CONTRIBUTION.md` for the existing upstream reports
and the evidence required before proposing a HAL/SDL change.

## Repeated game starts with normal Exit

`run-game-warm-launches.py` stages one immutable compiled-clean candidate,
waits for startup, sends the known splash/main-menu sequence through the KVM,
and collects each normal Exit. Start with one trial to qualify timing before a
longer batch:

```sh
python3 ports/nxdk/tools/run-game-warm-launches.py \
  --build-dir /absolute/preserved/build --output /absolute/new/evidence \
  --node /absolute/path/to/node --count 1
```

Only a 45-second startup guard, 10-second exit guard, no fault injection and
compiled clean mode are accepted. Inputs have a deadline before startup recovery
could turn them into dashboard navigation. Child tools and the exact button
helper are archived/hash-bound. Every launch uses the preserved XBE bytes.
A passing row requires early and later callback progress, menu music loading,
successful interrupt ownership, clean shutdown, empty stderr and no failure
markers. Any failed row stops the batch; unavailable logs set the recovery gate.
This does not test matches, hearing, physical cold power or four controllers.
The loop intentionally commits the game's unchanged preferences on normal Exit;
per-launch backups retain both slots.

Host-only protocol check: `python3 ports/nxdk/tests/run-game-warm-runner.py`.
The runner itself still requires native qualification before relying on a batch.
