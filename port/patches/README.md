# Experimental SDK patches

`sdl2-close-after-stall.patch` targets nxdk SDL2 revision
`2554902f7bbf469449216ee25f88016421271cd8`. It replaces the Xbox driver's
unbounded semaphore wait with a timed wait that checks SDL's shutdown flag.
Timeouts keep waiting and never count as consumed buffers. A real semaphore
error disconnects the device through SDL's standard internal API.

This patch addresses shutdown after lost completion notifications. It does not
recover playback or explain the intermittent startup stall. It is not applied
by the normal game build. Compare the guarded `probes/audio-close` baseline
and patched result before accepting it.

From the workspace root, with the pinned SDL tracked source otherwise clean:

```sh
git -C repos/nxdk/lib/sdl/SDL2 apply --check \
  "$PWD/repos/modern/ports/nxdk/patches/sdl2-close-after-stall.patch"
git -C repos/nxdk/lib/sdl/SDL2 apply \
  "$PWD/repos/modern/ports/nxdk/patches/sdl2-close-after-stall.patch"
```

Rebuild the affected probe/game to link the rebuilt SDK archive. Preserve the
SDL diff and its hash with each native result. Reverse with `git apply -R` using
the same path after experiments if returning to the unmodified SDK baseline.
Do not discard unrelated SDK edits. No upstream issue or PR has been submitted.

Native baseline `test-results/audio-close-before.*` confirms the fault fixture:
10 initial application callbacks, two withheld backend completions, no subsequent
callback progress, and close exceeding three seconds before a fresh watchdog
marker/reboot. This demonstrates the unmodified driver's shutdown hang under
the injected fault; it does not reproduce the game's separate startup failure.

Patched native results (`test-results/audio-close-after.*`) contain two valid
fault trials: each returned from close in 121 ms, with two withheld completions
and no watchdog marker. The third trial is inconclusive (one initial application
callback, no withheld completions); it cannot count as a successful fault trial.
The patch is currently applied in this workspace's SDL source and archive.
Subsequent builds use that patched archive until the patch is explicitly
reversed and the SDK rebuilt; the game wrapper does not silently undo it.

## Ready-link initialization candidate

`ac97-preserve-ready-link.candidate.patch` applies to nxdk `14d5ee97...`.
It retains an enabled/ready AC97 link and otherwise keeps the original cold
reset; DMA engine reset and descriptor setup remain unchanged. It passed
`git apply --check` against the selected HAL, and is now applied on the isolated SDK branch `smw/ac97-ready-link` for
game qualification. The existing cxbe and SDL close-wait edits remain separate.

The short instrumented comparison completed three preserving arms (60 cycles)
and three reset controls (one stall and two 20-cycle passes). A longer same-XBE
comparison then stalled in cycle eight with reset, while preserving passed all
200 cycles with constant available pages and no watchdog. See
`test-results/audio-init-protocol-200-native.json`. Actual game tests are pending; this uninstrumented patch cannot inherit the probe's
acceptance. Clean teardown is a separate concern and is retained in both arms.
No physical cold-start/fallback, audible-output or other-BIOS claim is made.
See `probes/audio-init-protocol/README.md` and
`test-results/audio-init-protocol-progress.json`.
