# Kernel delay measurement

The pinned HAL uses `KeDelayExecutionThread(-10)` for its intended one-microsecond
AC97 cold-reset hold. This probe measures that primitive, longer thread delays,
and processor stalls with `KeQueryPerformanceCounter`, including counter-call
baseline and one tick of uncertainty when counting definite undershoots. It
never initializes SDL/audio or reads/writes AC97, so it does not measure the
actual reset pulse or prove an audio root cause/fix. A ten-second watchdog bounds
unexpected blocking and invalidates a measurement if its marker is present.

Windows driver documentation recommends a processor stall for short hardware
waits ([KeStallExecutionProcessor](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kestallexecutionprocessor)).
That documentation motivates a measurement; it is not proof of Xbox-kernel
behavior. No SDK timing primitive has been changed by this experiment.

From the modern checkout:

```sh
./ports/nxdk/tools/build-probe.sh timer-granularity
python3 ports/nxdk/tools/run-audio-probe.py timer-granularity --count 1 \
  --output ../../logs/hardware/timer-measurement-new
```

Run only after another hardware test has returned to XBMC. The wrapper records
exact build provenance and the runner uses a separate title folder. This is a
silent timing probe, not an emulator or audible game test.
