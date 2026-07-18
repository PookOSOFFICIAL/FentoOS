#include <types.h>
#include <hal.h>
#include <string.h>

struct hal_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebx;
    uint32_t ebp;
    uint32_t eip;
};

extern void context_switch_asm(struct hal_context **old_ctx, struct hal_context *new_ctx);
extern void context_trampoline(void);

void hal_context_init(struct hal_context **ctx, void *stack_top,
                      void (*entry)(void), bool user, virtaddr_t user_stack) {
    (void)user;
    (void)user_stack;
    uint32_t *sp = (uint32_t *)stack_top;

    *(--sp) = (uint32_t)entry;
    *(--sp) = 0;

    struct hal_context *c = (struct hal_context *)((uint32_t)sp - sizeof(struct hal_context));
    c->eip = (uint32_t)context_trampoline;
    c->ebp = 0;
    c->ebx = 0;
    c->esi = 0;
    c->edi = 0;
    *ctx = c;
}

void hal_context_switch(struct hal_context **old_ctx, struct hal_context *new_ctx) {
    context_switch_asm(old_ctx, new_ctx);
}

void hal_enter_user(virtaddr_t entry, virtaddr_t user_stack) {
    __asm__ volatile(
        "cli\n"
        "movw $0x23, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        "pushl $0x23\n"
        "pushl %0\n"
        "pushl $0x202\n"
        "pushl $0x1B\n"
        "pushl %1\n"
        "iret\n"
        :
        : "r"(user_stack), "r"(entry)
        : "eax", "memory");
}

void hal_return_to_user(struct hal_trapframe *tf) {
    __asm__ volatile(
        "movl %0, %%esp\n"
        "popl %%gs\n"
        "popl %%fs\n"
        "popl %%es\n"
        "popl %%ds\n"
        "popa\n"
        "addl $8, %%esp\n"
        "iret\n"
        :
        : "r"(tf)
        : "memory");
}
