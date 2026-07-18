#include <types.h>
#include <hal.h>
#include <string.h>

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;
    uint32_t ss0;
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

static struct gdt_entry gdt[6];
static struct gdt_ptr   gdt_ptr;
static struct tss_entry tss;

extern void gdt_flush(uint32_t ptr);
extern void tss_flush(void);

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

static void tss_init(void) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss);
    gdt_set(5, base, limit, 0x89, 0x00);
    memset(&tss, 0, sizeof(tss));
    tss.ss0 = 0x10;
    tss.esp0 = 0;
    tss.cs = 0x0B;
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;
    tss.iomap_base = sizeof(tss);
}

void hal_cpu_init(void) {
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base  = (uint32_t)&gdt;

    gdt_set(0, 0, 0, 0, 0);
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF);
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF);
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF);
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF);
    tss_init();

    gdt_flush((uint32_t)&gdt_ptr);
    tss_flush();
}

void hal_tss_set_kernel_stack(virtaddr_t stack_top) {
    tss.esp0 = stack_top;
}
