#ifndef _FENTO_SERIAL_H
#define _FENTO_SERIAL_H

#include <types.h>

void serial_init(void);
void serial_enable_input(void);
void serial_putc(char c);
void serial_puts(const char *s);
int  serial_received(void);
char serial_getc(void);
int  serial_getc_nb(void);

#endif
