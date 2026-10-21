# Silent SDL output reopen probe

This diagnostic repeatedly opens a silent SDL output, requires callbacks to
advance over two 200-ms windows, and closes it, for 20 cycles in one process.
It uses the same pinned HAL close interposer as the game. It isolates output
lifecycle behavior from music decoding and game state. It does not test audible
quality or prove prevention of the game's intermittent warm-launch stalls.

`D:\audio-close.txt` chooses `legacy` or `clean` before the first open. Missing
configuration selects legacy; verify the logged selection before classifying a
trial. Use the versioned sidecar tool to preserve and restore this file in the
dedicated `/F/SMW-audio-reopen-probe` folder. Do not alter the game's sidecar.
The runner creates that folder, or it may be created empty in advance.

```sh
ports/nxdk/tools/build-probe.sh audio-reopen
python3 ports/nxdk/tools/audio-close-mode.py --directory /F/SMW-audio-reopen-probe --state /absolute/new/evidence/sidecar --mode clean
python3 ports/nxdk/tools/run-audio-probe.py audio-reopen --count 1 --output /absolute/new/evidence/run
python3 ports/nxdk/tools/audio-close-mode.py --directory /F/SMW-audio-reopen-probe --state /absolute/new/evidence/sidecar --mode restore
```

The probe has a 45-second independent kernel-thread watchdog and returns using
XReboot after recording results. A startup failure on cycle one is inconclusive
about reopening. A later zero-progress cycle is a reopen failure. Require all
20 cycles, matching executable provenance, no watchdog, no cleanup-failure marker
and empty stderr for success. A cleanup failure can also reboot independently;
never count dashboard return alone as success. Kernel faults that prevent the
watchdog thread from running can still require a physical restart.

After native collection, run the read-only qualifier:

```sh
python3 ports/nxdk/tools/analyze-audio-reopen.py --evidence /absolute/evidence/run --output /absolute/new/result.json
```

It checks the archived executable/log hashes and all 20 callback/connection/close
records, the terminal completion line, empty stderr and absence of failure
markers. Clean mode additionally requires the ownership and shutdown ledgers.
It reports memory samples without inferring leak freedom. Host fixtures:
`python3 ports/nxdk/tests/run-audio-reopen-analysis.py`.
