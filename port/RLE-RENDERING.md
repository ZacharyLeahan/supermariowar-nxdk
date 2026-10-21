# Unkeyed RGB565 RLE regression

Pinned SDL2 revision2554902f7bbf469449216ee25f88016421271cd8 (2.0.9) encodes
an unkeyed blended RGB565 surface using RLEColorkeySurface. That function reads
map->info.colorkey even though SDL_COPY_COLORKEY is absent. Black is skipped,
leaving previous scene pixels. The menu shade also loses its black pixels.

The game workaround preserves RLE for explicitly keyed sprites, and uses normal
blits for unkeyed surfaces without a pixel alpha mask. It is NXDK-only. No SDL
rendering source is patched. See gfxSprite.cpp and tests/rle-unkeyed.c.

Hardware observera016 recorded7532 differing pixels between initial and returned
Main, all initially black. With workaround observer822c, both full screen BMPs
are byte-identical. Both runs used a30s Time Limit match with forced spin;
offsets returned to0, full clip640x480, alpha255, blend1. This is independent of
KVM scaling. The initial diagnostic that saved the backdrop itself froze;
screen-only capture completed and exited normally.

Standalone host reproduction (Apple Silicon, no video/audio initialization):

- Current Homebrew SDL2 reported2.32.74: ordinary and RLE both write black, exit0.
- Pinned2.0.9: ordinary writes0; RLE leaves31 (blue RGB565), exit1 as expected.
- Host results are not emulator or Xbox results. The actual hardware observations
  are recorded separately in test-results/menu-*-observer.json.

Host commands used a git archive of the pinned SDL2 tree in a path without spaces:

```sh
mkdir /tmp/smw-sdl209-repro-source
git -C repos/nxdk/lib/sdl/SDL2 archive 2554902f7bbf469449216ee25f88016421271cd8 | tar -x -C /tmp/smw-sdl209-repro-source
cmake -S /tmp/smw-sdl209-repro-source -B /tmp/smw-sdl209-repro-build \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DSDL_SHARED=OFF -DSDL_STATIC=ON \
  -DSDL_TEST=OFF -DSDL_AUDIO=OFF -DSDL_VIDEO=OFF -DSDL_JOYSTICK=OFF \
  -DSDL_HAPTIC=OFF -DSDL_RENDER=OFF \
  -DHAVE_GCC_WERROR_DECLARATION_AFTER_STATEMENT=FALSE
```

In that temporary host-only source copy, change the fallback
`#define SDL_DYNAMIC_API 1` in src/dynapi/SDL_dynapi.h to0. This matches Xbox's
existing static API selection; it avoids linking disabled subsystem functions
from the host dynamic dispatch table. No pixel, surface, RLE or blit source is
changed. The warning override is required for old Objective-C code on modern
Clang; these are host fixture accommodations, not SDK/game build modifications.

```sh
cmake --build /tmp/smw-sdl209-repro-build --target SDL2-static -j4
cc -I/tmp/smw-sdl209-repro-build/include -I/tmp/smw-sdl209-repro-source/include \
  repos/modern/ports/nxdk/tests/rle-unkeyed.c /tmp/smw-sdl209-repro-build/libSDL2.a \
  -framework Cocoa -framework IOKit -liconv -o /tmp/smw-rle-unkeyed-209
/tmp/smw-rle-unkeyed-209
```

Expected pinned result: exit1 and `rle=1 expected_black=0 actual=31 opaque=0`.
A newer library fixing the regression should exit0 with both checks opaque.
No upstream issue or PR has been posted. This file is evidence for a future
contribution, not a claim that the Xbox SDL2 dependency has been upgraded.

## Existing upstream fix

SDL already fixed this in commit
[c93e3b36074ddb2ed986cbacc5e9d4f3a60db343](https://github.com/libsdl-org/SDL/commit/c93e3b36074ddb2ed986cbacc5e9d4f3a60db343)
on2021-04-28, referencing bug2881. The guard requires a color key or an actual
pixel alpha mask when blending. The pinned fork predates it. This is an SDL2
backport/update candidate, not an SDL3 requirement or a newly discovered
upstream SDL bug. The current game keeps its local workaround rather than
changing the SDK during qualification. A backport must retain initialization
of the local flags variable; do not blindly copy an isolated hunk from a
revision with different surrounding code. Full commit metadata was archived
read-only in workspace logs/sdl2-rle-upstream-fix.json. No upstream post made.

`patches/sdl2-rle-unkeyed.candidate.patch` backports the upstream guard to
the pinned source while retaining flags initialization. It applies cleanly.
The host baseline exits1, and the separate patched host copy exits0; both
executables and outputs are hash-recorded in test-results/rle-unkeyed-host.json.
This candidate SDK patch is not applied to the SDK or included in88385. It has
not been tested natively as an SDK patch. Do not add it to release reproduction
commands without separately qualifying a new executable.

## Opaque blit performance

Disabling the invalid RLE path fixed pixels but exposed slow generic alpha
blending (~12FPS menus) in88385. `gfxSprite::setalpha` now selects NONE only
when alpha255 and pixel Amask0; translucent/pixel-alpha surfaces retain BLEND.
Candidate8df74 recorded steady60.1-60.2FPS menus, correct borders/shading,
zero underruns and clean Exit. See opaque-copy-menu.json.

`tests/opaque-rgb565.c` independently checks actual SDL2.0.9 surface output for
black/red/magenta pixels with and without a color key. Opaque BLEND and NONE
are byte-identical and preserve color-key transparency (RLE disabled in this
semantic test). Compile it with the same host command as rle-unkeyed.c,
substituting the source/output paths. See test-results/opaque-rgb565-host.json.
