/* openkernel - PIT Timer Implementation */
/* Uses IRQ 0 (PIT channel 0) with a simple tick counter */

#include "timer.h"
#include "idt.h"

/* Externals from io.asm */
extern void io_outb(uint16_t port, uint8_t data);

/* PIT I/O ports */
#define PIT_CMD  0x43
#define PIT_CH0  0x40

/* Default frequency: ~100 Hz (10ms per tick) */
#define TICK_HZ  100

/* Tick counter */
static volatile uint32_t tick_count = 0;

/* Extern scheduler call from process.c */
extern void scheduler_schedule(void);

/* Timer IRQ handler - called on each PIT tick */
static void timer_irq_handler(void)
{
    tick_count++;

    /* Preempt the current process every 10 ticks (approx 100ms) */
    if (tick_count % 10 == 0) {
        scheduler_schedule();
    }
}

/* Initialize PIT timer */
void timer_init(uint32_t frequency)
{
    uint32_t divisor;

    /* Calculate divisor for PIT (base clock 1.193182 MHz) */
    if (frequency == 0) {
        frequency = TICK_HZ;
    }
    divisor = 1193182 / frequency;

    /* Register IRQ handler */
    idt_register_handler(0, timer_irq_handler);

    /* Configure PIT: channel 0, lobyte/hibyte, rate generator mode, 16-bit binary */
    io_outb(PIT_CMD, 0x36);

    /* Send divisor (low byte then high byte) */
    io_outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    io_outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
}

/* Get current tick count */
uint32_t timer_get_ticks(void)
{
    return tick_count;
}

/* Sleep for ms milliseconds (busy-wait) */
void timer_sleep(uint32_t ms)
{
    uint32_t ticks_per_ms = TICK_HZ / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    uint32_t target = tick_count + (ms * ticks_per_ms / 1000) + 1;
    while (tick_count < target) {
        asm volatile ("pause");
    }
}