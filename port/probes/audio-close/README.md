# Silent SDL output-close fault fixture

After normal callback progress, this fixture replaces the HAL completion callback
with an integer counter that deliberately withholds the SDL semaphore post.
Once application callback progress stops, it calls SDL_CloseAudioDevice. An
independent kernel thread reboots after three seconds if close does not return,
writing `close-watchdog.log` through WinAPI rather than SDL or stdio.

This is a controlled notification-loss test, not a reproduction or explanation
of the game's intermittent startup failure. It never supplies fake audio
completions. All PCM is silence. A normal-start prerequisite failure is logged
as inconclusive. The probe and any driver changes require native verification;
compilation alone is not a pass.

Build with the standard space-free nxdk setup and this directory as make's
working directory. Run from the game checkout with:

```sh
python3 ports/nxdk/tools/run-audio-probe.py audio-close --count 1 --wait-seconds 180
```

The runner uses a separate `F:\SMW-audio-close-probe` folder, clears old logs,
verifies the executable before launch, and retrieves both the main log and any
watchdog marker. `stage=close-enter` without `stage=close-return`, plus a fresh
watchdog marker, demonstrates the guarded close failure. Compare exact SDK/SDL
revisions, patches and XBE hashes before calling a driver change effective.

Build and record provenance from the game checkout with:

```sh
ports/nxdk/tools/build-probe.sh audio-close
```

The runner now requires this wrapper's hash-bound `bin/BUILD-INFO.json` and
archives its recursive source patches and SDK library hashes. Manual make alone
does not refresh that metadata. Use the wrapper immediately before launching
when SDK source or libraries have changed.
