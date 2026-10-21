# Silent HAL queue-drain experiment

This bypasses SDL and the game. It submits one silent 4 KiB PCM buffer to
nxdk's XAudio backend every 100 ms, deliberately allowing each 21 ms buffer to
drain before resubmission. A DPC callback only increments an interlocked counter;
register logging stays on the main thread. After eight submissions it reboots
to the dashboard. It tests completion notifications, not speaker output.

Build with the same space-free setup as the SDL startup probe, substituting
`ports/nxdk/probes/audio-hal-drain` as the make directory. Then run from the
game checkout:

```sh
python3 ports/nxdk/tools/run-audio-probe.py audio-hal-drain --count 1
```

The runner uses a separate writable `F:\SMW-hal-drain-probe` directory, verifies
the uploaded XBE hash, launches through XBMC FTP and retrieves one fresh log per
run. It expects these probes to auto-reboot; do not use it for arbitrary games.
Credentials use `XBOX_FTP_USER`/`XBOX_FTP_PASSWORD`, defaulting to the dashboard's
`xbox`/`xbox`. No BIOS settings or game files are modified.

Build and record provenance from the game checkout with:

```sh
ports/nxdk/tools/build-probe.sh audio-hal-drain
```

The runner now requires this wrapper's hash-bound `bin/BUILD-INFO.json` and
archives its recursive source patches and SDK library hashes. Manual make alone
does not refresh that metadata. Use the wrapper immediately before launching
when SDK source or libraries have changed.
