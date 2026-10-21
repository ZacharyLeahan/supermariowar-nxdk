# Paired AC97 initialization protocol diagnostic

This uses 200 silent reopen cycles with a 180-second watchdog and two startup
arms in one executable. The earlier 20-cycle XBE `e940e40f...` is preserved
separately; its trials cannot establish acceptance of this longer binary. Both use compiled clean teardown. `D:\audio-init.txt` selects:

- `reset`: retain the selected HAL's original cold-reset protocol.
- `preserve-ready`: skip that cold reset only if global control has link-enable
  bit 2 and global status reports codec-ready bit 0x100; otherwise use the
  original protocol. DMA engine initialization remains unchanged in both arms.

The bit values above mean masks 0x2 and 0x100 respectively, not bit indices.
Missing or malformed settings retain reset; qualify only the logged valid mode.
Report actual cold-reset/preserved-link counters and pre/post control/status,
not just the requested setting. Logging occurs after the output sample window;
only the snapshots/counters are taken inside initialization. Both arms incur
that instrumentation, which may itself affect the original failure rate.

The HAL snapshot retains its MIT license and original source except for this
bounded source-level experiment. `verify-source.py` requires original HAL SHA256
`6cf57fc5946c614acdae37194a03cb2b850f722e160bbcf43b84d15646b335ec` and verifies
that the snapshot differs by exactly the archived `hal-audio.patch`. The SDK
checkout and HAL library are not modified. This is not an accepted SDK fix.

Create an empty dedicated `/F/SMW-audio-init-protocol` folder, then:

```sh
ports/nxdk/tools/build-probe.sh audio-init-protocol
python3 ports/nxdk/tools/audio-close-mode.py --setting init --directory /F/SMW-audio-init-protocol --state /absolute/new/state --mode reset
python3 ports/nxdk/tools/run-audio-probe.py audio-init-protocol --count 1 --wait-seconds 360 --output /absolute/new/reset-evidence
python3 ports/nxdk/tools/analyze-audio-reopen.py --evidence /absolute/new/reset-evidence --output /absolute/new/reset-result.json
python3 ports/nxdk/tools/audio-close-mode.py --setting init --directory /F/SMW-audio-init-protocol --state /absolute/new/state --mode preserve-ready
python3 ports/nxdk/tools/run-audio-probe.py audio-init-protocol --count 1 --wait-seconds 360 --output /absolute/new/preserve-evidence
python3 ports/nxdk/tools/analyze-audio-reopen.py --evidence /absolute/new/preserve-evidence --output /absolute/new/preserve-result.json
python3 ports/nxdk/tools/audio-close-mode.py --setting init --directory /F/SMW-audio-init-protocol --state /absolute/new/state --mode restore
```

Keep this exact executable for both arms and preserve each log before changing
settings or launching again. The independent 180-second watchdog can recover a
schedulable wait; it cannot guarantee recovery from all kernel faults. A
watchdog or cleanup fallback is a failed run. Reproduce failure in the reset arm
before inferring prevention, use reversal/repeated trials, and distinguish
in-process reopening from game/dashboard relaunch and actual speaker output.

Host checks: `tests/run-audio-init-source.py`, `tests/run-audio-close-mode.py`,
`tests/run-audio-reopen-analysis.py`, and `tests/run-probe-runner.py` (paths
relative to `ports/nxdk`).
