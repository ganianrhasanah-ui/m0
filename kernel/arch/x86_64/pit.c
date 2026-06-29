#include "io.h"
#include "pit.h"

#define PIT_CHANNEL0 0x40u
#define PIT_COMMAND  0x43u

static volatile uint64_t g_timer_ticks = 0;

void pit_configure_hz(uint32_t hz) {
    if (hz == 0u) {
        return;
    }

    uint32_t divisor = PIT_BASE_FREQUENCY_HZ / hz;

    outb(PIT_COMMAND, 0x36u);
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFFu));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFFu));
}

void timer_on_irq0(void) {
    ++g_timer_ticks;
}

uint64_t timer_ticks(void) {
    return g_timer_ticks;
}
