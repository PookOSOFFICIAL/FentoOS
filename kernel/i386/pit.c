#include <types.h>
#include <hal.h>

static volatile uint64_t timer_ticks;

static void pit_handler(void *frame) {
    (void)frame;
    timer_ticks++;
    extern void sched_tick(void);
    sched_tick();
}

void hal_timer_init(uint32_t freq_hz) {
    timer_ticks = 0;
    uint32_t divisor = 1193182 / freq_hz;
    hal_outb(0x43, 0x36);
    hal_outb(0x40, (uint8_t)(divisor & 0xFF));
    hal_outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    hal_irq_register(0, pit_handler);
    hal_irq_unmask(0);
}

uint64_t hal_timer_ticks(void) {
    return timer_ticks;
}
