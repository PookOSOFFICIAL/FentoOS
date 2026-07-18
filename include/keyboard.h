#ifndef _FENTO_KEYBOARD_H
#define _FENTO_KEYBOARD_H

#include <types.h>

void keyboard_init(void);
void keyboard_enqueue(char c);
void keyboard_set_foreground(pid_t pid);
int  keyboard_getchar(void);
int  keyboard_available(void);

#endif
