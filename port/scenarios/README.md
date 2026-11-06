# Reviewed game smoke scenarios

These plans use the shared ESP-KVM `tools/run_xbox_scenario.mjs` runner.
They assume four human players, map `1986`, Start focused in Single Game Menu,
and the exact previous mode named by each plan. Do not run them from another
screen or repeat them blindly. Launch/exit checkpoints require visual review;
the runner's INCONCLUSIVE exit status is expected for ordinary game scenarios.

From the shared ESP-KVM checkout, first dry-run the chosen JSON using its
absolute path and `--scenario`. Then execute with explicit `--url`, a new
ignored `observations/` directory, `--build-id` set to the candidate XBE SHA-256,
and `--kvm-build-id` identifying the firmware (record its binary SHA separately
when the version ends in dirty). Keep a fresh `/stream` consumer active while
taking screenshots; close it and release controls when done. Do not interrupt
another chat's maintenance or take controller ownership during its checks.

Each plan selects the next mode, captures the selected mode and live game,
exercises every player separately, captures the exit dialog, then returns to
Start in settings. These are short launch/input/exit checks, not full-match,
all-objective, audio, latency or sustained-performance qualification. Use the
established four-route tool for simultaneous held states; this runner releases
all players between individual steps.

The 2026-11-06 Phanto run also showed points for P3 and P4. Hash-bound observed
results live in `../test-results/mode-smokes-20261106.json`. Planned scenarios
must never be treated as passed merely because their JSON is present here.
