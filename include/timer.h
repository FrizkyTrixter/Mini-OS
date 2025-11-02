/* File: include/timer.h
 *
 * PIT driver + timing helpers
 * ---------------------------
 * Provides initialization, tick counting, and sleep functions.
 * The PIT runs at ~1.193182 MHz and is typically set to 100 Hz (10 ms per tick).
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Initialize PIT channel 0 to the desired frequency (Hz).
 * Example: timer_init(100) → 100 Hz → one tick every 10 ms.
 */
void timer_init(uint32_t hz);

/* Called by the IRQ0 handler every timer interrupt. */
void timer_tick(void);

/* Returns the number of timer ticks (jiffies) since boot. */
uint64_t timer_jiffies(void);

/* Returns the actual PIT frequency that was programmed. */
uint32_t timer_hz(void);

/* Busy-wait for 'ticks' number of PIT interrupts.
 * Uses sti;hlt so the CPU idles between ticks.
 */
void sleep_ticks(uint64_t ticks_to_wait);

/* Busy-wait for approximately 'ms' milliseconds. */
void sleep_ms(uint32_t ms);

#endif /* TIMER_H */
