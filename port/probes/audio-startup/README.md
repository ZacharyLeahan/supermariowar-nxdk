# Silent audio startup probe

This standalone nxdk SDL2 program opens the same 48 kHz/stereo/1,024-frame
output used by the game. Its callback writes silence and counts calls. It has
no music decoder, game mixer, assets or worker thread. It logs callback and
analog/digital DMA register progress at one-second intervals through ten seconds, then reboots to the
dashboard. It does not test audible output. It never modifies BIOS settings.

Build from the parent workspace using its existing space-free symlink:

```sh
export NXDK_DIR=/tmp/smw-xbe-workspace/repos/nxdk
export PATH="$NXDK_DIR/bin:/opt/homebrew/opt/llvm/bin:$PATH"
probe=/tmp/smw-xbe-workspace/repos/modern/ports/nxdk/probes/audio-startup
make -C "$probe" CURDIR="$probe" -j4
```

Copy `bin/default.xbe` into a separate writable Xbox folder and launch from
XBMC. Retrieve `audio-startup.log` afterward. Preserve individual logs and the
XBE SHA256 for each launch; one successful run cannot disprove an intermittent
failure. Zero callbacks after ten seconds is a failure even when SDL reports
that the device opened. Status registers are diagnostic observations, not
proof that speakers work. SDK baseline: `14d5ee97e73347c973f1f57b68b79ec08c9e77f2`
with its recursive SDL2 submodule revision; use workspace revision records.

The probe also logs raw global control/status and DMA base/count/prefetch state
before SDL initialization, after device opening, and at every timed sample.
These read-only snapshots provide an inherited-state and healthy-run comparison
for the game's failure-only sampler; no register is changed by the logger.
Different executables have different descriptor addresses, so compare state and
progress rather than expecting base addresses to match across images.

Build and record provenance from the game checkout with:

```sh
ports/nxdk/tools/build-probe.sh audio-startup
```

The runner now requires this wrapper's hash-bound `bin/BUILD-INFO.json` and
archives its recursive source patches and SDK library hashes. Manual make alone
does not refresh that metadata. Use the wrapper immediately before launching
when SDK source or libraries have changed.
