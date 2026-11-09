# November 9 mixed-controller mode scenarios

These are the exact text plans used by the reviewed 17-mode audit on clean
candidate `4addeb4e...`, map `0smw`, wired P1 and Wi-Fi P2–P4. Results are in
`../../test-results/mixed-mode-smokes-20261109.json`; scenario hashes bind those
records to these files. Plans alone are not evidence of a pass.

Start with four humans ready, Classic mode, and Start focused in Single Game
Menu. The Stomp plan advances five modes from Classic. Each subsequent plan
expects the preceding mode and Start focus. Do not run from an unreviewed
screen. Menu presses are 200 ms with 2 s settling; player gameplay holds are
1 s. These are short launch/input/exit checks, not completed objectives.

Use the shared ESP-KVM runner from its checkout, first with `--scenario` only
for a dry run, then with `--execute`, the actual device `--url`, a new private
`--output observations/<unique-attempt>`, and explicit XBE `--build-id` and
KVM `--kvm-build-id`. Keep a fresh `/stream` viewer active. Review each plan's
selected-mode, live-game, exit-dialog and returned-settings screenshots before
starting the next plan. Generic INCONCLUSIVE does not become a game pass until
that review. Release all controls and close the viewer after each scenario.

Hardware freezes require the shared rig's documented recovery procedure.
Preserve native logs and verify the installed XBE hash before relaunching.
