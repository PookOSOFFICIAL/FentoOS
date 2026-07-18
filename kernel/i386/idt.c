#include <types.h>
#include <hal.h>
#include <string.h>
#include <console.h>

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr   idt_ptr;

extern void idt_flush(uint32_t ptr);

extern void isr0(void); extern void isr1(void); extern void isr2(void);
extern void isr3(void); extern void isr4(void); extern void isr5(void);
extern void isr6(void); extern void isr7(void); extern void isr8(void);
extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void irq0(void); extern void irq1(void); extern void irq2(void);
extern void irq3(void); extern void irq4(void); extern void irq5(void);
extern void irq6(void); extern void irq7(void); extern void irq8(void);
extern void irq9(void); extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void); extern void isr144(void);

static hal_irq_handler_t irq_handlers[16];
static hal_syscall_handler_t syscall_dispatcher;

static const char *exc_msg[] = {
    "Divide by zero", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound range", "Invalid opcode", "Device N/A", "Double fault",
    "Coproc overrun", "Invalid TSS", "Segment not present", "Stack fault",
    "General protection", "Page fault", "Reserved", "x87 FP", "Alignment",
    "Machine check", "SIMD FP", "Virt", "Ctrl protect", "Res", "Res",
    "Res", "Res", "Res", "Res", "Res", "Res", "Res", "Res"
};

static void idt_set(int i, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[i].base_low  = base & 0xFFFF;
    idt[i].base_high = (base >> 16) & 0xFFFF;
    idt[i].sel   = sel;
    idt[i].zero  = 0;
    idt[i].flags = flags;
}

static void pic_remap(void) {
    hal_outb(0x20, 0x11); hal_io_wait();
    hal_outb(0xA0, 0x11); hal_io_wait();
    hal_outb(0x21, 0x20); hal_io_wait();
    hal_outb(0xA1, 0x28); hal_io_wait();
    hal_outb(0x21, 0x04); hal_io_wait();
    hal_outb(0xA1, 0x02); hal_io_wait();
    hal_outb(0x21, 0x01); hal_io_wait();
    hal_outb(0xA1, 0x01); hal_io_wait();
    hal_outb(0x21, 0xFF); hal_io_wait();
    hal_outb(0xA1, 0xFF); hal_io_wait();
}

void hal_interrupts_init(void) {
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint32_t)&idt;
    memset(&idt, 0, sizeof(idt));
    memset(irq_handlers, 0, sizeof(irq_handlers));
    syscall_dispatcher = NULL;

    pic_remap();

    idt_set(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set(1, (uint32_t)isr1, 0x08, 0x8E);
    idt_set(2, (uint32_t)isr2, 0x08, 0x8E);
    idt_set(3, (uint32_t)isr3, 0x08, 0x8E);
    idt_set(4, (uint32_t)isr4, 0x08, 0x8E);
    idt_set(5, (uint32_t)isr5, 0x08, 0x8E);
    idt_set(6, (uint32_t)isr6, 0x08, 0x8E);
    idt_set(7, (uint32_t)isr7, 0x08, 0x8E);
    idt_set(8, (uint32_t)isr8, 0x08, 0x8E);
    idt_set(9, (uint32_t)isr9, 0x08, 0x8E);
    idt_set(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set(15, (uint32_t)isr15, 0x08, 0x8E);
    idt_set(16, (uint32_t)isr16, 0x08, 0x8E);
    idt_set(17, (uint32_t)isr17, 0x08, 0x8E);
    idt_set(18, (uint32_t)isr18, 0x08, 0x8E);
    idt_set(19, (uint32_t)isr19, 0x08, 0x8E);
    idt_set(20, (uint32_t)isr20, 0x08, 0x8E);
    idt_set(21, (uint32_t)isr21, 0x08, 0x8E);
    idt_set(22, (uint32_t)isr22, 0x08, 0x8E);
    idt_set(23, (uint32_t)isr23, 0x08, 0x8E);
    idt_set(24, (uint32_t)isr24, 0x08, 0x8E);
    idt_set(25, (uint32_t)isr25, 0x08, 0x8E);
    idt_set(26, (uint32_t)isr26, 0x08, 0x8E);
    idt_set(27, (uint32_t)isr27, 0x08, 0x8E);
    idt_set(28, (uint32_t)isr28, 0x08, 0x8E);
    idt_set(29, (uint32_t)isr29, 0x08, 0x8E);
    idt_set(30, (uint32_t)isr30, 0x08, 0x8E);
    idt_set(31, (uint32_t)isr31, 0x08, 0x8E);

    idt_set(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set(33, (uint32_t)irq1, 0x08, 0x8E);
    idt_set(34, (uint32_t)irq2, 0x08, 0x8E);
    idt_set(35, (uint32_t)irq3, 0x08, 0x8E);
    idt_set(36, (uint32_t)irq4, 0x08, 0x8E);
    idt_set(37, (uint32_t)irq5, 0x08, 0x8E);
    idt_set(38, (uint32_t)irq6, 0x08, 0x8E);
    idt_set(39, (uint32_t)irq7, 0x08, 0x8E);
    idt_set(40, (uint32_t)irq8, 0x08, 0x8E);
    idt_set(41, (uint32_t)irq9, 0x08, 0x8E);
    idt_set(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set(47, (uint32_t)irq15, 0x08, 0x8E);

    idt_set(144, (uint32_t)isr144, 0x08, 0xEE);

    idt_flush((uint32_t)&idt_ptr);
}

void hal_syscall_set_dispatcher(hal_syscall_handler_t disp) {
    syscall_dispatcher = disp;
}

void hal_syscall_init(void) {
}

void isr_dispatch(struct hal_trapframe *tf) {
    if (tf->int_no == 144) {
        if (syscall_dispatcher)
            tf->eax = (uint32_t)syscall_dispatcher(tf);
        return;
    }
    extern void mm_page_fault(struct hal_trapframe *tf);
    if (tf->int_no == 14) {
        mm_page_fault(tf);
        return;
    }
    console_printf("\n[EXCEPTION] %s (int=%u err=%x eip=%x)\n",
                   exc_msg[tf->int_no & 31], tf->int_no, tf->err_code, tf->eip);
    for (;;) hal_hlt();
}

void irq_dispatch(struct hal_trapframe *tf) {
    int irq = tf->int_no - 32;
    if (irq >= 0 && irq < 16 && irq_handlers[irq])
        irq_handlers[irq](tf);
    hal_irq_eoi(irq);
}

void hal_irq_register(int irq, hal_irq_handler_t handler) {
    if (irq >= 0 && irq < 16) irq_handlers[irq] = handler;
}

void hal_irq_eoi(int irq) {
    if (irq >= 8) hal_outb(0xA0, 0x20);
    hal_outb(0x20, 0x20);
}

void hal_irq_mask(int irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    if (irq >= 8) irq -= 8;
    uint8_t v = hal_inb(port);
    hal_outb(port, v | (1 << irq));
}

void hal_irq_unmask(int irq) {
    uint16_t port = (irq < 8) ? 0x21 : 0xA1;
    if (irq >= 8) irq -= 8;
    uint8_t v = hal_inb(port);
    hal_outb(port, v & ~(1 << irq));
}
