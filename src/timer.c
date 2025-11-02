/* File: src/timer.c
 *
 * PIT timer driver (x86, channel 0, mode 3) + simple sleep helpers.
 * No 64-bit division is used (avoids __udivdi3 in freestanding builds).
 */

#include <stdint.h>
#include "io.h"
#include "timer.h"

#define PIT_CMD     0x43
#define PIT_CH0     0x40
#define PIT_BASE_HZ 1193182u      /* ~1.193182 MHz */

static volatile uint64_t s_ticks = 0;  /* bumped by IRQ0 */
static uint32_t          s_pit_hz = 100;

/* Initialize PIT to approx 'hz' using mode 3 (square wave). */
void timer_init(uint32_t hz) {
    if (hz == 0) hz = 100;

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor == 0)        divisor = 1;       /* highest possible freq */
    if (divisor > 65535u)    divisor = 65535u;  /* lowest possible freq */

    /* Effective frequency after integer rounding. */
    s_pit_hz = PIT_BASE_HZ / divisor;
    if (s_pit_hz == 0) s_pit_hz = 1;

    /* Channel 0, access lo/hi, mode 3, binary */
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
}

/* Called by IRQ0 handler. */
void timer_tick(void) { ++s_ticks; }

/* Monotonic tick count since timer_init(). */
uint64_t timer_jiffies(void) { return s_ticks; }

/* Query effective PIT rate. */
uint32_t timer_hz(void) { return s_pit_hz; }

/* Sleep for 'ticks_to_wait' timer interrupts.
 * Uses 'sti; hlt' so CPU idles and always wakes on next IRQ. */
void sleep_ticks(uint64_t ticks_to_wait) {
    if (ticks_to_wait == 0) return;
    uint64_t start = timer_jiffies();
    while ((timer_jiffies() - start) < ticks_to_wait) {
        asm volatile("sti; hlt");
    }
}

/* Sleep for approximately 'ms' milliseconds, without 64-bit division.
 *
 * We compute:
 *   ticks = floor(ms/1000) * hz + ceil((ms%1000) * hz / 1000)
 *
 * All operations are 32-bit; the final sum is promoted to 64-bit for sleep_ticks.
 */
void sleep_ms(uint32_t ms) {
    if (ms == 0) return;

    uint32_t hz = s_pit_hz;

    /* Whole-second component: (ms/1000) * hz */
    uint32_t sec  = ms / 1000u;
    uint32_t frac = ms % 1000u;

    /* Guard against overflow if someone passes a huge 'ms'. */
    uint32_t whole_ticks;
    if (sec == 0) {
        whole_ticks = 0;
    } else if (sec > (0xFFFFFFFFu / hz)) {
        whole_ticks = 0xFFFFFFFFu;   /* saturate */
    } else {
        whole_ticks = sec * hz;
    }

    /* Fractional component: ceil(frac * hz / 1000) = (x + 999) / 1000 */
    /* (frac < 1000, hz <= 1193182) => product < 1.2e9 fits in uint32_t */
    uint32_t frac_prod = frac * hz;
    uint32_t frac_ticks = (frac_prod + 999u) / 1000u;

    uint64_t total = (uint64_t)whole_ticks + (uint64_t)frac_ticks;
    if (total == 0) total = 1;  /* ensure at least one tick */

    sleep_ticks(total);
}
