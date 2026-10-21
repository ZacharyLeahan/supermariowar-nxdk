# Main-menu render-state fixture

A small post-match border difference was noticed in KVM images. Do not infer a
game defect or repair camera state without distinguishing the capture path from
actual rendered pixels.

`MENU_FRAME_CAPTURE=1` is default-off. On each entry to the main menu after a
1.5-second settling interval it records camera offsets, surface/clip dimensions,
backdrop alpha/blend mode and destination identity, and writes a copy of the
screen to `D:\mr-<build-id>-<index>-s.bmp`.
Existing output files cause a logged skip, not overwrite. Archive both pairs and
logs with the exact executable hash before rerunning the same observer.

`MENU_TEST_SPIN=1` requires capture and intentionally enables the existing
screen-spin effect at gameplay entry. This is a controlled fixture, not normal
candidate behavior. Complete a short match, return to the main menu, wait for its
second capture and exit normally. Compare raw screen/backdrop bytes and logged
state before interpreting KVM border pixels. Neither option is in clean-screen
candidate ec21c4f7.

```sh
SOURCE_DATE_EPOCH=1792148400 ports/nxdk/tools/build-game.sh \
  PREFERENCES=1 AUDIO_FAILURE_DMA_LOG=1 AUDIO_CLEAN_SHUTDOWN=1 \
  STARTUP_WATCHDOG_MS=45000 EXIT_WATCHDOG_MS=10000 \
  MENU_FRAME_CAPTURE=1 MENU_TEST_SPIN=1
```

This command compiles a separate observer. Compilation does not qualify a
rendering result or transfer native acceptance from another executable.

## First native attempt: stalled, not qualified

Observer `6141c697ae8ce3d76eda91cc1b030b3252ea8e3fa6109398b069a43b4a45327c`
(source `dda2a59127489941d63b2f909bdc3f7a76beefa4`) displayed the initial
main menu, then did not respond to A, Down or Start. Back+Start did not
recover it. The capture call is a suspect, not an established cause; retrieve
logs and any partial BMP files after physical recovery before changing it.
No before/after render comparison passed. Do not deploy this observer as the
playable candidate. The clean-screen ec21 executable does not enable this code.
Launch record: `logs/hardware/menu-render-observer` in the workspace.

Physical recovery completed and read-only archives verified the observer XBE.
The screen BMP is complete (921654 bytes); the backdrop BMP is empty. No
capture-completion line reached the log. This narrows the stall to backdrop
capture; precise cause remains unproven. Recovery gate cleared after archival.
See `test-results/menu-frame-observer-first.json`.

The next observer saves only the screen surface, which completed in the first
attempt. It logs state before and completion after the call, flushing both. It
does not save or modify the cached backdrop. Compare static screen borders and
state before/after; cached backdrop byte equality is no longer a claimed check.

## RLE workaround comparison

Observer822c completed the same forced-spin30s match and returned to Main.
Both 921654-byte screen BMPs are byte-identical (SHA d8c7d851...). Camera and
clip state match. Normal Exit, empty stderr and no failure markers were
recorded. The previous observer differed at7532 initially black pixels.
The application workaround disables RLE only for unkeyed RGB565 surfaces;
keyed sprites retain it. Shading is now visible as intended.
See `test-results/menu-rle-workaround-observer.json`.
