#ifndef _FENTO_VMM_H
#define _FENTO_VMM_H

#include <types.h>

void vmm_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kmalloc_aligned(size_t size);

#endif
