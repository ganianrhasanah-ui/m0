#include <stdint.h>
#include <mcsos/arch/cpu.h>
#include <mcsos/arch/idt.h>
#include <mcsos/kernel/log.h>
#include <mcsos/kernel/panic.h>
#include <mcsos/kernel/version.h>
#include <mcsos/kmem.h>
#include <mcsos/syscall.h>
#include "pic.h"
#include "pit.h"
extern char __kernel_start[];
extern char __kernel_end[];
#define M8_BOOT_HEAP_SIZE (64u * 1024u)

static unsigned char m8_boot_heap[M8_BOOT_HEAP_SIZE]
    __attribute__((aligned(4096)));

static void m8_heap_bootstrap(void) {
    if (kmem_init(m8_boot_heap, sizeof(m8_boot_heap)) != 0) {
        KERNEL_PANIC("M8 kmem_init failed", 0x4D3848454150ull);
    }

    void *probe = kmem_alloc(128);
    if (!probe) {
        KERNEL_PANIC("M8 kmem_alloc failed", 0x4D38414C4C4Full);
    }

    if (kmem_free_checked(probe) != 0) {
        KERNEL_PANIC("M8 kmem_free failed", 0x4D3846524545ull);
    }

    kmem_stats_t st;
    kmem_get_stats(&st);

    log_writeln("[M8] kernel heap bootstrap OK");
}

static void m4_selftest(void) {
    KERNEL_ASSERT(__kernel_end > __kernel_start);
    KERNEL_ASSERT(sizeof(uintptr_t) == 8u);
    KERNEL_ASSERT(sizeof(x86_64_idt_entry_t) == 16u);
    KERNEL_ASSERT(x86_64_idt_base_for_test() != 0u);
    KERNEL_ASSERT(x86_64_idt_limit_for_test() == 4095u);
    log_writeln("[M4] selftest: IDT invariants passed");
}
static uint64_t k_get_ticks(void) {
    return timer_ticks();
}

static void k_yield_current(void) {
    /* Stub M10 */
}

static void k_exit_current(int code) {
    (void)code;
    /* Stub M10 */
}

extern void serial_putc(char c);

static int64_t k_write_serial(const char *buf, size_t len) {
    if (buf == 0) {
        return -1;
    }

    for (size_t i = 0; i < len; ++i) {
        serial_putc(buf[i]);
    }

    return (int64_t)len;
}
static void m10_syscall_smoke_direct(void)
{
    int64_t r = mcsos_syscall_dispatch(
        MCSOS_SYS_PING,
        0, 0, 0, 0, 0, 0
    );

    if (r != 0x2605020A) {
        KERNEL_PANIC("M10 syscall ping failed", 0x4D313050494E47ULL);
    }

    log_writeln("[M10] syscall ping ok");
}
void kmain(void) {
    log_init();
    log_write(MCSOS_NAME);
    log_write(" ");
    log_write(MCSOS_VERSION);
    log_write(" ");
    log_write(MCSOS_MILESTONE);
    log_writeln(" kernel entered");
    log_key_value_hex64("kernel_start", (uint64_t)(uintptr_t)__kernel_start);
    log_key_value_hex64("kernel_end", (uint64_t)(uintptr_t)__kernel_end);
    log_key_value_hex64("rflags_before_idt", cpu_read_rflags());

    x86_64_idt_init();
pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);
pic_mask_all();
pic_unmask_irq(0);
pit_configure_hz(100);
__asm__ volatile ("sti"); 
   m4_selftest();
m8_heap_bootstrap();
    mcsos_syscall_ops_t ops = {
        .get_ticks = k_get_ticks,
        .yield_current = k_yield_current,
        .exit_current = k_exit_current,
        .write_serial = k_write_serial,
    };

    mcsos_syscall_init(&ops);

    mcsos_syscall_set_user_region((mcsos_user_region_t){
        .base = 0x0000000000400000ULL,
        .limit = 0x0000800000000000ULL,
    });

    m10_syscall_smoke_direct();
#ifdef MCSOS_M4_TRIGGER_BREAKPOINT
    log_writeln("[M4] triggering intentional breakpoint exception");
    x86_64_trigger_breakpoint_for_test();
    log_writeln("[M4] returned from breakpoint handler");
#endif

#ifdef MCSOS_M4_TRIGGER_PANIC
    KERNEL_PANIC("intentional M4 panic test", 0x4D43534F533034u);
#else
    log_writeln("[M4] IDT and exception dispatch path installed");
    log_writeln("[M4] ready for QEMU smoke test and GDB audit");
    cpu_halt_forever();
#endif
}
