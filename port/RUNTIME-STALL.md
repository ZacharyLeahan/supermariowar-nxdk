# Four-player runtime-stall investigation

2026-10-25: candidate `8df74f38...` reached a four-human Classic result, and then
entered a 600-second four-human Time Limit match on `0smw`. All four routed S3
controllers produced independent actions. During mixed movement/jump/run/powerup
stress, subsequent KVM frames became identical with the timer at 372.
The user subsequently saw the dashboard on the TV while the KVM still showed
that game frame, confirming that capture was stale. A user restart restored
current dashboard video. The recovered game log contains 64 ten-second
performance reports and ends at about 61 FPS, 17,404 KiB available memory,
30,100 audio callbacks and zero music underruns. Its end is near the original
repeated-frame interval, without a clean-exit record or stderr output. Thus a
concurrent game stall remains unresolved; stale capture alone does not explain
why the game's logging stopped. Reset attempts made through the stale viewer
cannot be judged by their screenshots.

Logs, both preferences and the exact installed XBE hash were archived and
verified before further launches. All four controllers opened with unique
instance IDs and physical ports 1–4; SDL enumeration order was 3,4,1,2.
This is not a passed ten-minute match. Later inputs during the repeated-frame
period are inconclusive. Four-player release qualification remains open.
No gameplay fix is claimed.

## Optional diagnostic candidate

`RUNTIME_WATCHDOG_MS=45000` adds a main-loop progress monitor and stage tags.
It is off by default. On a 45-second lack of checkpoint progress, an independent
thread attempts to create `D:\runtime-watchdog.log` with the last stage, app state,
player context and frame counter, then reboot to the dashboard. It uses raw
file/kernel calls, not SDL, stdout or the save system. An existing marker is
not overwritten. A marker is a failure, never a successful-exit result.

The thread cannot recover a total kernel/interrupt failure, and successful
recovery was initially unqualified. A later missing-thumbnail fatal UI test
verified marker creation and dashboard recovery after 45,041 ms; see
[thumbnail investigation](THUMBNAILS.md). This does not qualify kernel hangs.
Scalar stage snapshots can straddle a
checkpoint and are localization hints, not a crash stack. Long loading work
can also exceed the timeout; every firing needs review. This guard is diagnostic
instrumentation, not a repair for the observed freeze or a production release
feature. Its extra stack and checkpoint overhead apply only when enabled.

Stage values are in RuntimeDiagnostics.h: input/world/collisions/platforms,
each player's move/commit, objects, cleanup/effects, mode/map update, rendering
and the frame limiter/present boundary. Normal pauses still advance main-loop
checkpoints. Normal game-loop exit disarms the guard before audio teardown.

Build only into a separate preserved candidate; keep the existing XBE and its
native results bound to their original hash. After a timeout, retrieve the
runtime marker, normal logs, stderr and provenance before retrying. Do not
infer hardware acceptance from compilation alone.

## Short diagnostic run

Candidate `c1dd5ced6a9e370d7c1b62628d758179dcc15fbc4995d31ce63d09bb6d22d196`
(source `20da2dee`) booted on the physical Xbox and completed a 120-second
four-human Time Limit match on `0smw`, including 80 simultaneous mixed-input
commands. The result was a tie, with player scores 4,5,9,9. All four human settings
reloaded. Normal exit committed preferences and completed clean audio shutdown;
stderr was empty, music underruns stayed zero, and no watchdog marker appeared.
See [bound result](test-results/four-player-runtime-120s-20261025.json).

This does not reproduce or resolve the earlier long-run incident. Watchdog arming
was logged, but its failure path has not been exercised. No game code changed
between the compiled candidate and this hardware test.

## Longer repeat and teams

The same diagnostic XBE completed a full 600-second four-human Time Limit match
on `0smw` (Player 2 won; player scores 9,12,11,5), including 185 mixed four-route
commands and the earlier 80-command fast pattern. A subsequent 2v2 Classic
match also completed: players 1–2 versus 3–4, with Team 1 winning on one life.
Normal exit committed preferences and completed clean audio shutdown. All logs
were preserved and hash-verified before further launches; no watchdog marker
or stderr output appeared. The Xbox was left at the dashboard with neutral input.
[Bound results](test-results/four-player-runtime-600s-and-teams-20261025.json).

This is a successful longer repeat on the diagnostic executable, not a diagnosis
or repair of the original incident. The watchdog's failure path remains
unqualified, and conventional-pad USB unplug/replug and stock 64 MB hardware
are still untested. The original executable was retained during that session; the
separate tested diagnostic executable was `runtime-watchdog.xbe`. The later
[thumbnail audit](THUMBNAILS.md) promoted its separately tested fix and preserved
the original binary under a rollback filename.

## Later verified fatal-UI recovery

The thumbnail audit independently reproduced a missing-cache fatal error on
`c1dd5ced...`. The runtime guard wrote a phase-2 marker and returned the Xbox to
the dashboard after 45,041 ms. This qualifies that blocked fatal-UI path only;
it does not establish the cause of the original timer-372 incident or guarantee
recovery from kernel/interrupt failures. See
[bound reproduction](test-results/thumbs-missing-cache-repro-20261025.json).
