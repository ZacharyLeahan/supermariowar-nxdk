# Super Mario War: original Xbox / nxdk test port

Branch: `port/nxdk-xbox`. Base: `c7056790ccb485b7309137251167b75fed1482e4`, immediately before the SDL3 conversion. Target: original Xbox, 64 MB runtime limit, up to four local controllers.

## Current verification

See [TESTING.md](TESTING.md) for our testing approach, the exact candidate,
completed hardware checks, captured-audio results and remaining acceptance work.
The [candidate index](test-results/opaque-copy-candidate.json) links the
hash-specific evidence. Older diagnostic builds and investigations below retain
their original scope; they do not supersede that current test record.

The current candidate has completed Classic and full ten-minute local matches,
pause/resume, results, normal exit, saved-setting reload and selected save-recovery
checks on the 128 MB Stellar Xbox. Actual menu/match music and effects have now
been captured through the KVM, with zero game-reported underruns in that session.
The long capture lost 0.825 seconds in the KVM stream, so seamless-loop and human
listening acceptance remain open. Physical stock-64-MB hardware, four physical
controllers, broader cold/firmware reliability and emulator testing remain
unqualified. This is a private test candidate, not a fully accepted release.

## Source publication

The current tree and upstream history contain game assets outside the data
submodule. See [ASSET-PUBLICATION-AUDIT.md](ASSET-PUBLICATION-AUDIT.md) before
creating a public fork or exporting a source-only repository. No GitHub
repository has been created or pushed for this publication request.

## Build from the parent workspace

```sh
./scripts/build-game.sh
CXX=/opt/homebrew/opt/llvm/bin/clang++ ./scripts/test-game-compat.sh
python3 scripts/verify-xbe.py repos/modern/ports/nxdk/bin/default.xbe
python3 repos/modern/ports/nxdk/tools/package-game.py \
  --build-dir repos/modern/ports/nxdk/bin \
  --output artifacts/smw-nxdk-new-candidate
```

The workspace wrapper exports nxdk/LLVM paths and works around spaces in nxdk make rules with a stable temporary-directory symlink and explicit `CURDIR`. For a checkout under a path without spaces, activate nxdk and run `make -C ports/nxdk` with `NXDK_DIR` exported. Initialize recursive submodules first, including `ports/nxdk/vendor/fmt`.

The package includes the XBE, game data, checksums, input revisions and user test instructions. Copy the complete package to a separate writable Xbox HDD directory, then launch its XBE. On the tested dashboard, D: resolves to the executable's directory. Logs are `D:\smw.log` and `D:\smw-error.log`; available-memory observations are emitted at startup and every ten seconds during rendering. Other dashboards/BIOSes remain untested.

## Port boundaries

- `util/Filesystem.h` retains the desktop standard filesystem behind a namespace alias. nxdk uses a deliberately small path interface plus `FindFirstFileA` / `FindNextFileA`, with slash normalization before file access and deterministic directory ordering.
- `util/Format.h` retains desktop `std::format`. nxdk uses pinned fmt 10.2.1, header-only, with exceptions/consteval checking disabled. The adapter excludes unused desktop Win32 console helpers while including fmt; the vendor source is unchanged.
- `util/Error.h` retains desktop throws. nxdk logs and stops on fatal loading/format errors, because nxdk's default runtime cannot unwind C++ exceptions. Desktop catch handlers are guarded off for the Xbox build; they do not silently resume after a fatal error. Config reads/writes use the explicit two-slot format when `PREFERENCES=1`; the initial persistence-disabled path remains available. Optional tileset image absence is handled by an explicit preflight rather than exception recovery.
- `util/CheckedCast.h` retains desktop `dynamic_cast`. nxdk has no `__RTDynamicCast`, so the game uses explicit virtual type queries for its single-inheritance object, mode and platform-path hierarchies. Successful casts preserve the requested subobject; unrelated/null casts return null. This is game-side compatibility, not a general-purpose RTTI runtime.
- `Audio.cpp` implements a native SDL2 PCM mixer and bounded Vorbis decoding behind the game sound interface. Effects output passed a first hardware match; the music underrun fix is under hardware validation. See `AUDIO.md` for exact status and decoder revision.
- `OfflineNetwork.cpp` disables network preferences and file compression; `NETWORK_DISABLED` selects the existing NULL transport. Network UI code remains linked but no network service is enabled.
- `Platform.cpp` selects 640x480, starts diagnostics and assigns controller defaults after game configuration initializes. Connect pads before launch; virtual-controller reconnect is tested; stable physical-port ordering remains unverified. One pad gets a CPU opponent; detected additional pads default to humans. A jump/confirm, B back, X run/grab, Y stored item, Start pause; D-pad movement/navigation.
- `BinaryFile` now owns its diagnostic path string, avoiding a dangling pointer when constructed from a temporary filesystem string.

The pinned nxdk SDL2 is 2.0.9 and SDL_image is 2.0.5. The game uses those forks rather than fetching desktop SDL. fmt is pinned at `e69e5f977d458f2650bb346dadf2ad30c5320281`. See workspace revision records for SDK/data SHAs.

## Remaining acceptance work

1. A 64 MB application budget is verified by allocation testing on the upgraded Xbox. Physical stock-64-MB hardware remains untested.
2. Repeat and broaden the passing menu/match test to more maps and modes.
3. Check four physical controllers. One virtual controller plus three bots has completed matches within the restricted memory budget.
4. Improve recoverable error paths, hotplug and exit behavior from concrete failures.
5. Resolve startup reliability, listen to output, finish save coverage and complete the final exact-XBE regression.

The package omits optional Contest worlds because one of their assets exceeds the FATX 42-character filename limit; core local-match maps remain included. Performance and memory evidence applies only to the recorded maps, modes and hardware configuration. Full CPU/debug symbol integration is not yet set up.

## Hardware loop

The workspace provides `python3 scripts/xbox-test.py logs --label bootN`,
`python3 scripts/xbox-test.py deploy`, and `python3 scripts/xbox-test.py launch`.
Defaults are Xbox `192.168.1.254`, FTP `xbox/xbox`, test folder
`F:\SMW-nxdk-test`. Credentials can be overridden with `XBOX_FTP_USER` and
`XBOX_FTP_PASSWORD`. Deployment replaces only that test executable and verifies
its SHA256 by reading it back. XBMC's FTP `SITE RunXBE` launches the title; a
timeout/disconnect is expected during launch but is not evidence of successful
boot. Video and logs establish runtime results. FTP exists only in XBMC.

BIOS IGR worked in XBMC but did not recover the first two test builds during
these attempts. Current code adds a game-side Back+Start two-second reboot and
a fatal-screen 60-second reboot timeout; Back+Start recovered the fatal-screen test; running-game recovery also passed; the timeout remains unverified.
The game now reopens joystick handles if the connected instance list changes, and the KVM virtual-controller reconnect test passed. RGB565 halves sprite and map-surface allocation sizes on nxdk; desktop remains 32-bit.
Controller configurations store indices, whereas nxdk SDL event IDs derive
from USB device timestamps; current code translates those IDs before player
input processing. Physical-port ordering and four-controller behavior remain
unverified.

## Unattended exit regression guard

`EXIT_WATCHDOG_MS=15000` is an opt-in diagnostic build flag. Selecting Exit
arms a WinAPI thread that reboots after the timeout if normal cleanup hangs.
It uses kernel/file APIs rather than SDL/stdio during teardown and writes
`exit-watchdog.log` if it fires. A marker, an SDL assertion or a watchdog-start
error invalidates clean-exit acceptance. Ordinary builds default to no guard;
this does not replace verification of normal shutdown. The currently installed
original `default.xbe` is preserved while separate candidates are tested.

## Clean release builds and SDK object caches

The versioned wrapper is `ports/nxdk/tools/build-game.sh` in the modern checkout;
the workspace `scripts/build-game.sh` delegates to it. For this workspace layout
(`repos/modern` and sibling `repos/nxdk`), a complete optimized rebuild is:

```sh
./ports/nxdk/tools/build-game.sh clean-build PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1
```

This cleans game and SDK generated objects/libraries, then rebuilds with the
port's explicit `-O2 -DNDEBUG` flags. It preserves sources/data and host tools.
Do not use a library cache built by the initial SDK example as evidence of an
optimized release: nxdk does not track SDK compiler-command changes. The game
configuration stamp handles game-object flag changes, but SDK flag changes need
this clean build. The wrapper keeps a stable space-free workspace symlink so
Make dependency paths remain valid. Its conflict check protects other checkouts.

## Build provenance

The versioned game wrapper writes `ports/nxdk/bin/BUILD-INFO.json` after a
successful build. `tools/build-probe.sh NAME` does the same for each probe.
Records bind the XBE hash to effective game flags, recursive repository pins,
exact tracked source diffs, untracked source hashes, compiler versions and an
SDK archive catalog. Diff filenames are FATX-safe. The catalog includes unused
SDK libraries; it is not a linker map. Runtime acceptance is recorded separately
against the XBE hash. Commit untracked source before a release.

The probe and game-startup runners verify the manifest hash and archive its
provenance. Explicit preserved-reference manifests remain supported for earlier
experimental executables. A fresh make plus metadata snapshot is required after
SDK changes; source revision alone cannot describe a locally patched library.

## Exact-build candidate packaging

`tools/package-game.py` requires an explicit build directory containing the XBE,
its hash-matching `BUILD-INFO.json` and recorded source diffs. It verifies those
diff hashes and the clean data revision before packaging. Output must be new,
so an earlier listening or hardware candidate cannot be overwritten by accident.
Optional `--evidence result.json` files must identify the exact executable hash;
the tool copies them verbatim without treating them as release acceptance.

Every package includes checksums, build provenance and an explicit experimental
status. ZIP timestamps and permissions are fixed for reproducibility from
identical inputs. Packaging is separate from final hardware acceptance and
public distribution; no public release is created by this command.

## Interactive exact-build hardware session

After the console returns to XBMC, launch a preserved candidate with:

```sh
python3 ports/nxdk/tools/launch-game.py \
  --build-dir ../../artifacts/controller-preferences-v7-recorded \
  --output ../../logs/hardware/controller-v7-session
```

This verifies the XBE hash against its build manifest and FTP readback, stages
`hardware-session.xbe`, and preserves the installed `default.xbe`. It backs up
both existing save slots and prior logs before clearing stale logs and requesting
launch. It does not restore saves after intentional in-game edits. Video and
fresh logs must prove runtime behavior; a successful FTP request is not a boot
or acceptance result. Run this only when no other hardware test owns the console.

## Current audio investigation and recovery

The earlier PCI-logging hang has been recovered after the user's restart and
archived read-only, with exact executable verification. No power-cycle or old-log
preservation is currently pending. Broad PCI logging remains disabled. The fixed
startup watchdog passed one schedulable injected-wait fixture, writing its marker
and returning to XBMC; arbitrary kernel-fault recovery is not established.

The silent same-XBE reopen diagnostic passed 20 clean-close cycles, while its
legacy arm stalled at cycle six and required the 45-second watchdog during close.
A clean reversal then stalled at cycle four despite a successful interrupt
connection; clean shutdown still returned without a watchdog. Thus cleanup is
an improvement, but startup remains unresolved. The separately compiled
`AUDIO_CLEAN_SHUTDOWN=1` game candidate selects clean shutdown without a sidecar;
it has compiled and is not yet a hardware-qualified release.

A further initialization-only probe compares the original cold reset against
preserving a link already reporting enabled/ready. It keeps clean teardown in
both arms, uses a guarded local HAL snapshot and leaves the SDK HAL unchanged.
See [audio-init-protocol/README.md](probes/audio-init-protocol/README.md) and
[AUDIO-CONTRIBUTION.md](AUDIO-CONTRIBUTION.md). Existing upstream reports #362
and #363 are tracked there; no issue or PR has been posted.

If any future attempt cannot return logs, preserve it with
`tools/recover-game-logs.py` after dashboard recovery before another launch.
Helpers honor the workspace recovery gate; no watchdog reboot counts as a pass.
