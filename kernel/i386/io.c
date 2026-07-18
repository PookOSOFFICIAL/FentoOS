#include <types.h>
#include <hal.h>

uint8_t hal_inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

uint16_t hal_inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

uint32_t hal_inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

void hal_outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void hal_outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

void hal_outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

void hal_insw(uint16_t port, void *buf, uint32_t count) {
    __asm__ volatile("cld; rep insw"
                     : "+D"(buf), "+c"(count)
                     : "d"(port)
                     : "memory");
}

void hal_outsw(uint16_t port, const void *buf, uint32_t count) {
    __asm__ volatile("cld; rep outsw"
                     : "+S"(buf), "+c"(count)
                     : "d"(port));
}

void hal_io_wait(void) {
    hal_outb(0x80, 0);
}

void hal_cli(void) { __asm__ volatile("cli"); }
void hal_sti(void) { __asm__ volatile("sti"); }
void hal_hlt(void) { __asm__ volatile("hlt"); }

uint32_t hal_irq_save(void) {
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

void hal_irq_restore(uint32_t flags) {
    __asm__ volatile("push %0; popf" : : "r"(flags) : "memory", "cc");
}
