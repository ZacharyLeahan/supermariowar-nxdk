# Missing-P1 recovery regression

These executed plans are bound to result `../../test-results/menu-recovery-own-slot-20261109.json`
on XBE84fbddeb. P1 was absent, P2 unconfigured after launch, P3/P4 configured.
Review each start state and screenshot before continuing. The selector-retry
plan starts at Main Options focus, not title or Start. It sets P1/P2 to CPU;
P3/P4 remain human. Team-entry then ready plans verify separate player actions.

Use the shared ESP-KVM runner with dry-run first, exact XBE/KVM build IDs, a
fresh stream viewer and a unique private observations directory. Generic
INCONCLUSIVE requires manual screenshot review. These are narrow input checks,
not full-match or all-controller release acceptance. Restore saved settings
after the session and archive native logs before relaunching.
