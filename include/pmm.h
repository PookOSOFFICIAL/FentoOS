#ifndef _FENTO_PMM_H
#define _FENTO_PMM_H

#include <types.h>

void pmm_init(uint32_t mem_upper_kb, physaddr_t kernel_end);
physaddr_t pmm_alloc_frame(void);
void pmm_free_frame(physaddr_t frame);
uint32_t pmm_free_count(void);
uint32_t pmm_total_count(void);

#define PMM_FRAME_SIZE 4096

#endif
