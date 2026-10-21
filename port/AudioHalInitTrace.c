// SPDX-License-Identifier: MIT
/* Diagnostic interposition around the selected nxdk HAL source. The original
 * driver is compiled unchanged, so all baseline register writes are retained.
 * Logging changes timing; this is not a recovery or release implementation. */
#include <stdint.h>
#include <stdio.h>
#include <xboxkrnl/xboxkrnl.h>

static void trace_state(const char *phase)
{
    const volatile uint32_t *regs = (const volatile uint32_t *)0xfec00000;
    printf("[audio-hal-init] phase=%s global_control=%08lx global_status=%08lx\n",
           phase, (unsigned long)regs[0x12c / 4], (unsigned long)regs[0x130 / 4]);
}
static NTSTATUS NTAPI trace_delay(KPROCESSOR_MODE mode, BOOLEAN alertable, PLARGE_INTEGER interval)
{
    trace_state("reset-asserted-before-delay");
    const NTSTATUS result = KeDelayExecutionThread(mode, alertable, interval);
    trace_state("reset-asserted-after-delay");
    printf("[audio-hal-init] reset_delay_status=%08lx requested_100ns=%lld\n",
           (unsigned long)result, (long long)interval->QuadPart);
    return result;
}
static BOOLEAN NTAPI trace_connect(PKINTERRUPT interrupt)
{
    const BOOLEAN result = KeConnectInterrupt(interrupt);
    printf("[audio-hal-init] interrupt_connected=%u\n", (unsigned)result);
    trace_state("interrupt-connect-returned");
    return result;
}
#define KeDelayExecutionThread trace_delay
#define KeConnectInterrupt trace_connect
#include <hal/audio.c>
#undef KeConnectInterrupt
#undef KeDelayExecutionThread
