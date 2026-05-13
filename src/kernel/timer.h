/* openkernel - PIT Timer Driver */
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Initialize PIT timer with given frequency (Hz) */
void timer_init(uint32_t frequency);

/* Get current tick count since boot */
uint32_t timer_get_ticks(void);

/* Sleep for a given number of milliseconds (busy-wait) */
void timer_sleep(uint32_t ms);

#endif /* TIMER_H */