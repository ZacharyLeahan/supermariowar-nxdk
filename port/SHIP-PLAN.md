# Shipping qualification, November 2026

Goal: a reproducible default.xbe qualified on the available original Xbox,
with a locally installable package and Rocky5/XBMC4Gamers submission materials.
Public source remains asset-free; prepare packaging locally before deciding
which data/artwork may be publicly distributed. Downloader inclusion requires
its maintainer accepting a submission and is not under this repository's control.

## Current baseline

- Installed candidate SHA-256: `10910209be2ef92fe5f9f3d1b38e06f0ced0ea86d453d9100d13ee519bb3c83c`.
- Prior logs preserved and individually hash-verified on 2026-11-06 in the
  private workspace `logs/hardware/release-qualification-20261106/preserved-prior-run`.
- Rig: direct physical P1, Wi-Fi S3 P2–P4; qualify anew rather than reusing
  older four-S3 results. KVM `v.0.56.1-180-g4d170a7-dirty`, discovered at
  `192.168.1.218` (`esp.local`). Physical power recovery remains unavailable.
- Real console is 128 MB; LIMIT64MB and application-budget tests do not prove
  physical stock-64-MB acceptance. No emulator qualification is claimed.

## Work and evidence

1. Establish fresh capture and verify isolated plus simultaneous input for all
   four players, including explicit release and missing-controller behavior.
2. Recheck missing/corrupt thumbnail fallback, page navigation, map selection,
   filters and clean return. Cover representative maps with different hazards.
3. Fix Health start/max-life inconsistency with legacy-save migration tests;
   verify healing, preferences and reload on the resulting candidate.
4. Audit all 22 modes with short input/exit checks, then complete representative
   timed, survival and team matches. Bind screenshots and logs to the XBE hash.
5. Capture menu, match, effects, pause/results, volume controls and loop boundaries;
   distinguish game audio defects from capture/transport gaps.
6. Measure game-side FPS/frame timing and free memory during representative
   one/four-player loads and a sustained session. Capture FPS is not game FPS.
7. Exercise native save/reload, malformed-slot fallback, repeated launch/exit,
   and fresh-install cache generation. Preserve logs before every new launch.
8. Freeze an accepted build, reproduce it, validate FATX-safe package paths,
   checksums, provenance, notices and install instructions. Prepare artwork in
   confirmed XBMC4Gamers formats and a maintainer-reviewable submission.

Use the shared ESP-KVM scenario runner with reviewed dry runs and fresh streaming
capture. Its generic INCONCLUSIVE outcome requires game-specific evidence review.
Keep private media under shared ignored observations; commit text summaries only.

## Downloader references

- https://github.com/Rocky5/XBMC4Gamers
- https://github.com/Rocky5/XBMC4Gamers/blob/master/Mod%20Files/system/scripts/URLDownloader/default.py

Inspect current source for archive layout and artwork requirements before final
packaging. Existing release-checklist blockers remain open until exact-candidate
results establish acceptance; box art alone is not a shipping gate completion.
