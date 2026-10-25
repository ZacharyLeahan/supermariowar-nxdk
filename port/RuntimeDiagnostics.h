#pragma once
// Optional runtime-stall diagnostics. No game behavior change when disabled.
enum class NxdkRuntimeStage : long {
    frame_start=1, state_update=2, before_flip=3, flip=4, after_flip=5,
    input=10, world_begin=11, player_collision=12, platforms=13,
    player_move=14, objects=15, player_object_collision=16,
    object_collision=17, player_commit=18, cleanup=19,
    eyecandy=20, mode_think=21, map_update=22, render=23
};
#ifdef NXDK_RUNTIME_WATCHDOG_MS
void nxdk_runtime_watchdog_start();
void nxdk_runtime_watchdog_stop();
void nxdk_runtime_checkpoint(NxdkRuntimeStage stage, long appstate, long context = -1);
#else
inline void nxdk_runtime_watchdog_start() {}
inline void nxdk_runtime_watchdog_stop() {}
inline void nxdk_runtime_checkpoint(NxdkRuntimeStage, long, long = -1) {}
#endif
