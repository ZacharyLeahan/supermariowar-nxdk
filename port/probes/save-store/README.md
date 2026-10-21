# Xbox two-slot file-store probe

This tests `SaveFileSlots.h` and `SaveStore.h` on the Xbox filesystem, separately
from game settings. It uses write-through file creation, checks write/flush/close
results and readback, damages the newer slot deliberately, checks fallback to
the older valid copy, repairs the damaged slot and tests an unwritable path.
Only `probe-save0.bin`/`probe-save1.bin` in its own folder are cleared. No game
save or dashboard file is modified. It auto-reboots after logging results.

Build with the space-free workspace setup documented by the audio probes,
using `ports/nxdk/probes/save-store` as the make directory. Then run twice:

```sh
python3 ports/nxdk/tools/run-audio-probe.py save-store --count 2
```

The dedicated directory is `F:\SMW-save-probe`. The second run must report a
valid previous record, generation 3, matching the final payload. This checks
ordinary reboot persistence, not sudden-power-loss durability. Interrupted
writes are simulated by deliberately truncating a newer copy; no console power
cut or firmware modification is involved. Results must be inspected before
claiming success. Settings integration into the game remains separate work.

Build and record provenance from the game checkout with:

```sh
ports/nxdk/tools/build-probe.sh save-store
```

The runner now requires this wrapper's hash-bound `bin/BUILD-INFO.json` and
archives its recursive source patches and SDK library hashes. Manual make alone
does not refresh that metadata. Use the wrapper immediately before launching
when SDK source or libraries have changed.
