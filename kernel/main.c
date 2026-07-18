#include <types.h>
#include <hal.h>
#include <console.h>
#include <serial.h>
#include <keyboard.h>
#include <pmm.h>
#include <vmm.h>
#include <sched.h>
#include <ata.h>
#include <minixfs.h>
#include <syscall.h>
#include <proc.h>
#include <string.h>

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
} __attribute__((packed));

extern uint32_t kernel_end;

void kernel_main(uint32_t magic, struct multiboot_info *mbi) {
    (void)magic;

    serial_init();
    console_init();
    console_set_color(0x0A, 0x00);
    console_printf("FentoOS v0.01 booting...\n");
    console_set_color(0x07, 0x00);

    hal_cpu_init();
    console_printf("[ok] GDT + TSS\n");

    hal_interrupts_init();
    keyboard_init();
    serial_enable_input();
    console_printf("[ok] IDT + PIC + serial input\n");

    uint32_t mem_upper = 32768;
    if (mbi && (mbi->flags & 1)) mem_upper = mbi->mem_upper;
    pmm_init(mem_upper, (physaddr_t)&kernel_end);
    console_printf("[ok] PMM: %u frames total, %u free\n",
                   pmm_total_count(), pmm_free_count());

    hal_paging_init();
    console_printf("[ok] VMM paging enabled\n");

    vmm_init();
    console_printf("[ok] kernel heap ready\n");

    console_printf("[ok] PS/2 keyboard\n");

    syscall_init();
    console_printf("[ok] syscalls (int 0x90, BSD style)\n");

    if (ata_init() == 0)
        console_printf("[ok] ATA PIO: %u sectors\n", ata_sector_count());
    else
        console_printf("[--] ATA PIO: no disk\n");

    sched_init();
    hal_timer_init(100);
    console_printf("[ok] PIT @ 100Hz + scheduler\n");

    if (minixfs_mount() < 0) {
        console_printf("[!!] failed to mount minixfs root\n");
        for (;;) hal_hlt();
    }

    console_printf("[ok] mounted MINIX v3 root\n");
    console_printf("\nFentoOS v0.01 ready. Launching userland /bin/sh (ring 3)...\n\n");

    proc_start_init("/bin/sh");

    hal_sti();
    sched_start();

    for (;;) hal_hlt();
}
