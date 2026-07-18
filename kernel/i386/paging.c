#include <types.h>
#include <hal.h>
#include <string.h>
#include <pmm.h>
#include <console.h>

#define PAGE_SIZE  4096
#define ENTRIES    1024
#define USER_BASE  0x40000000u

static uint32_t *kernel_dir;
static physaddr_t kernel_dir_phys;
static physaddr_t current_dir_phys;

static inline uint32_t pd_index(virtaddr_t va) { return (va >> 22) & 0x3FF; }
static inline uint32_t pt_index(virtaddr_t va) { return (va >> 12) & 0x3FF; }

static void map_in_dir(uint32_t *dir, virtaddr_t va, physaddr_t pa, uint32_t flags) {
    uint32_t di = pd_index(va);
    uint32_t ti = pt_index(va);
    uint32_t *table;
    if (!(dir[di] & HAL_PG_PRESENT)) {
        table = (uint32_t *)pmm_alloc_frame();
        memset(table, 0, PAGE_SIZE);
        dir[di] = ((uint32_t)table) | HAL_PG_PRESENT | HAL_PG_WRITE | (flags & HAL_PG_USER);
    } else {
        table = (uint32_t *)(dir[di] & ~0xFFF);
        if (flags & HAL_PG_USER) dir[di] |= HAL_PG_USER;
    }
    table[ti] = (pa & ~0xFFF) | (flags & 0xFFF) | HAL_PG_PRESENT;
}

void hal_paging_init(void) {
    kernel_dir = (uint32_t *)pmm_alloc_frame();
    kernel_dir_phys = (physaddr_t)kernel_dir;
    current_dir_phys = kernel_dir_phys;
    memset(kernel_dir, 0, PAGE_SIZE);

    for (virtaddr_t addr = 0; addr < 0x8000000; addr += PAGE_SIZE)
        map_in_dir(kernel_dir, addr, addr, HAL_PG_PRESENT | HAL_PG_WRITE);

    hal_paging_switch(kernel_dir_phys);
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void hal_paging_map(virtaddr_t va, physaddr_t pa, uint32_t flags) {
    map_in_dir(kernel_dir, va, pa, flags);
    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void hal_paging_unmap(virtaddr_t va) {
    uint32_t di = pd_index(va);
    uint32_t ti = pt_index(va);
    if (!(kernel_dir[di] & HAL_PG_PRESENT)) return;
    uint32_t *table = (uint32_t *)(kernel_dir[di] & ~0xFFF);
    table[ti] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
}

physaddr_t hal_paging_resolve(virtaddr_t va) {
    uint32_t di = pd_index(va);
    uint32_t ti = pt_index(va);
    if (!(kernel_dir[di] & HAL_PG_PRESENT)) return 0;
    uint32_t *table = (uint32_t *)(kernel_dir[di] & ~0xFFF);
    if (!(table[ti] & HAL_PG_PRESENT)) return 0;
    return (table[ti] & ~0xFFF) | (va & 0xFFF);
}

void hal_paging_switch(physaddr_t dir_phys) {
    current_dir_phys = dir_phys;
    __asm__ volatile("mov %0, %%cr3" : : "r"(dir_phys) : "memory");
}

physaddr_t hal_paging_kernel_dir(void) {
    return kernel_dir_phys;
}

physaddr_t hal_addrspace_current(void) {
    return current_dir_phys;
}

physaddr_t hal_addrspace_create(void) {
    uint32_t *dir = (uint32_t *)pmm_alloc_frame();
    if (!dir) return 0;
    memset(dir, 0, PAGE_SIZE);
    uint32_t kernel_pdes = USER_BASE >> 22;
    for (uint32_t i = 0; i < kernel_pdes; i++)
        dir[i] = kernel_dir[i];
    return (physaddr_t)dir;
}

int hal_addrspace_map_user(physaddr_t dir_phys, virtaddr_t va, uint32_t flags) {
    uint32_t *dir = (uint32_t *)dir_phys;
    physaddr_t frame = pmm_alloc_frame();
    if (!frame) return -1;
    map_in_dir(dir, va, frame, HAL_PG_PRESENT | HAL_PG_WRITE | HAL_PG_USER | flags);
    if (dir_phys == current_dir_phys)
        __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory");
    return 0;
}

static physaddr_t dir_resolve(uint32_t *dir, virtaddr_t va) {
    uint32_t di = pd_index(va);
    uint32_t ti = pt_index(va);
    if (!(dir[di] & HAL_PG_PRESENT)) return 0;
    uint32_t *table = (uint32_t *)(dir[di] & ~0xFFF);
    if (!(table[ti] & HAL_PG_PRESENT)) return 0;
    return (table[ti] & ~0xFFF);
}

void hal_addrspace_copy_to(physaddr_t dir_phys, virtaddr_t va, const void *src, uint32_t len) {
    uint32_t *dir = (uint32_t *)dir_phys;
    const uint8_t *s = (const uint8_t *)src;
    uint32_t done = 0;
    while (done < len) {
        virtaddr_t cur = va + done;
        uint32_t off = cur & 0xFFF;
        uint32_t chunk = PAGE_SIZE - off;
        if (chunk > len - done) chunk = len - done;
        physaddr_t frame = dir_resolve(dir, cur);
        if (frame == 0) {
            hal_addrspace_map_user(dir_phys, cur & ~0xFFF, 0);
            frame = dir_resolve(dir, cur);
            if (frame == 0) return;
        }
        memcpy((void *)(frame + off), s + done, chunk);
        done += chunk;
    }
}

void hal_addrspace_memset(physaddr_t dir_phys, virtaddr_t va, int c, uint32_t len) {
    uint32_t *dir = (uint32_t *)dir_phys;
    uint32_t done = 0;
    while (done < len) {
        virtaddr_t cur = va + done;
        uint32_t off = cur & 0xFFF;
        uint32_t chunk = PAGE_SIZE - off;
        if (chunk > len - done) chunk = len - done;
        physaddr_t frame = dir_resolve(dir, cur);
        if (frame == 0) {
            hal_addrspace_map_user(dir_phys, cur & ~0xFFF, 0);
            frame = dir_resolve(dir, cur);
            if (frame == 0) return;
        }
        memset((void *)(frame + off), c, chunk);
        done += chunk;
    }
}

physaddr_t hal_addrspace_clone(physaddr_t src_dir_phys) {
    uint32_t *src = (uint32_t *)src_dir_phys;
    physaddr_t new_phys = hal_addrspace_create();
    if (!new_phys) return 0;
    uint32_t *dst = (uint32_t *)new_phys;
    uint32_t start_pde = USER_BASE >> 22;
    for (uint32_t di = start_pde; di < ENTRIES; di++) {
        if (!(src[di] & HAL_PG_PRESENT)) continue;
        uint32_t *src_table = (uint32_t *)(src[di] & ~0xFFF);
        uint32_t *dst_table = (uint32_t *)pmm_alloc_frame();
        memset(dst_table, 0, PAGE_SIZE);
        dst[di] = ((uint32_t)dst_table) | (src[di] & 0xFFF);
        for (uint32_t ti = 0; ti < ENTRIES; ti++) {
            if (!(src_table[ti] & HAL_PG_PRESENT)) continue;
            physaddr_t src_frame = src_table[ti] & ~0xFFF;
            physaddr_t dst_frame = pmm_alloc_frame();
            memcpy((void *)dst_frame, (void *)src_frame, PAGE_SIZE);
            dst_table[ti] = dst_frame | (src_table[ti] & 0xFFF);
        }
    }
    return new_phys;
}

void hal_addrspace_destroy(physaddr_t dir_phys) {
    if (dir_phys == 0 || dir_phys == kernel_dir_phys) return;
    uint32_t *dir = (uint32_t *)dir_phys;
    uint32_t start_pde = USER_BASE >> 22;
    for (uint32_t di = start_pde; di < ENTRIES; di++) {
        if (!(dir[di] & HAL_PG_PRESENT)) continue;
        uint32_t *table = (uint32_t *)(dir[di] & ~0xFFF);
        for (uint32_t ti = 0; ti < ENTRIES; ti++) {
            if (table[ti] & HAL_PG_PRESENT)
                pmm_free_frame(table[ti] & ~0xFFF);
        }
        pmm_free_frame((physaddr_t)table);
    }
    pmm_free_frame(dir_phys);
}

void mm_page_fault(struct hal_trapframe *tf) {
    uint32_t addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(addr));
    extern void proc_page_fault(struct hal_trapframe *tf, uint32_t addr);
    proc_page_fault(tf, addr);
}
