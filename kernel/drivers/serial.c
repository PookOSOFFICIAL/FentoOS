#include <types.h>
#include <hal.h>
#include <serial.h>
#include <keyboard.h>

#define COM1 0x3F8

void serial_init(void) {
    hal_outb(COM1 + 1, 0x00);
    hal_outb(COM1 + 3, 0x80);
    hal_outb(COM1 + 0, 0x03);
    hal_outb(COM1 + 1, 0x00);
    hal_outb(COM1 + 3, 0x03);
    hal_outb(COM1 + 2, 0xC7);
    hal_outb(COM1 + 4, 0x0B);
}

static int is_transmit_empty(void) {
    return hal_inb(COM1 + 5) & 0x20;
}

void serial_putc(char c) {
    while (!is_transmit_empty());
    hal_outb(COM1, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

int serial_received(void) {
    return hal_inb(COM1 + 5) & 1;
}

char serial_getc(void) {
    while (!serial_received());
    return (char)hal_inb(COM1);
}

int serial_getc_nb(void) {
    if (!serial_received()) return -1;
    return (int)(uint8_t)hal_inb(COM1);
}

static void serial_handler(void *frame) {
    (void)frame;
    while (serial_received()) {
        char c = (char)hal_inb(COM1);
        if (c == '\r') c = '\n';
        keyboard_enqueue(c);
    }
}

void serial_enable_input(void) {
    hal_irq_register(4, serial_handler);
    hal_outb(COM1 + 1, 0x01);
    hal_irq_unmask(4);
    while (serial_received()) {
        char c = (char)hal_inb(COM1);
        if (c == '\r') c = '\n';
        keyboard_enqueue(c);
    }
}
