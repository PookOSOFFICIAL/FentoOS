#ifndef _FENTO_HAL_H
#define _FENTO_HAL_H

#include <types.h>

uint8_t  hal_inb(uint16_t port);
uint16_t hal_inw(uint16_t port);
uint32_t hal_inl(uint16_t port);
void     hal_outb(uint16_t port, uint8_t val);
void     hal_outw(uint16_t port, uint16_t val);
void     hal_outl(uint16_t port, uint32_t val);
void     hal_insw(uint16_t port, void *buf, uint32_t count);
void     hal_outsw(uint16_t port, const void *buf, uint32_t count);
void     hal_io_wait(void);

void hal_cli(void);
void hal_sti(void);
void hal_hlt(void);
uint32_t hal_irq_save(void);
void hal_irq_restore(uint32_t flags);

typedef void (*hal_irq_handler_t)(void *frame);

void hal_cpu_init(void);
void hal_interrupts_init(void);
void hal_irq_register(int irq, hal_irq_handler_t handler);
void hal_irq_mask(int irq);
void hal_irq_unmask(int irq);
void hal_irq_eoi(int irq);

void hal_timer_init(uint32_t freq_hz);
uint64_t hal_timer_ticks(void);

void hal_paging_init(void);
void hal_paging_map(virtaddr_t va, physaddr_t pa, uint32_t flags);
void hal_paging_unmap(virtaddr_t va);
physaddr_t hal_paging_resolve(virtaddr_t va);
void hal_paging_switch(physaddr_t dir_phys);
physaddr_t hal_paging_kernel_dir(void);

physaddr_t hal_addrspace_create(void);
void hal_addrspace_destroy(physaddr_t dir_phys);
physaddr_t hal_addrspace_clone(physaddr_t src_dir_phys);
int hal_addrspace_map_user(physaddr_t dir_phys, virtaddr_t va, uint32_t flags);
physaddr_t hal_addrspace_current(void);
void hal_addrspace_copy_to(physaddr_t dir_phys, virtaddr_t va, const void *src, uint32_t len);
void hal_addrspace_memset(physaddr_t dir_phys, virtaddr_t va, int c, uint32_t len);

#define HAL_PG_PRESENT  0x001
#define HAL_PG_WRITE    0x002
#define HAL_PG_USER     0x004

struct hal_trapframe {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

struct hal_context;
void hal_context_init(struct hal_context **ctx, void *stack_top,
                      void (*entry)(void), bool user, virtaddr_t user_stack);
void hal_context_switch(struct hal_context **old_ctx, struct hal_context *new_ctx);

void hal_enter_user(virtaddr_t entry, virtaddr_t user_stack);
void hal_return_to_user(struct hal_trapframe *tf);

void hal_tss_set_kernel_stack(virtaddr_t stack_top);

void hal_syscall_init(void);

int hal_syscall0(int n);
int hal_syscall3(int n, uint32_t a, uint32_t b, uint32_t c);

typedef int (*hal_syscall_handler_t)(struct hal_trapframe *tf);
void hal_syscall_set_dispatcher(hal_syscall_handler_t disp);

#endif
