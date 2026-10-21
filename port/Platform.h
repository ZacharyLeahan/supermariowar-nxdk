#pragma once
void nxdk_startup();
void nxdk_configure_controllers(bool installDefaults = false);
void nxdk_log_memory(const char* stage);
void nxdk_poll_recovery();
void nxdk_guard_normal_exit();

// Diagnostic startup stages, independent of SDL initialization.
enum class NxdkStartupStage : long { setup=0, lists=1, graphics=2, audio=3, pci=4, input=5, preferences=6, assets=7, ready=8 };
#ifdef NXDK_STARTUP_WATCHDOG_MS
void nxdk_startup_checkpoint(NxdkStartupStage stage);
#else
inline void nxdk_startup_checkpoint(NxdkStartupStage) {}
#endif
