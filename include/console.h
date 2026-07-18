#ifndef _FENTO_CONSOLE_H
#define _FENTO_CONSOLE_H

#include <types.h>

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_printf(const char *fmt, ...);
void console_clear(void);
void console_set_color(uint8_t fg, uint8_t bg);

#endif
