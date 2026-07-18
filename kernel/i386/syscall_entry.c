#include <types.h>
#include <hal.h>

int hal_syscall0(int n) {
    int r;
    __asm__ volatile("int $0x90" : "=a"(r) : "a"(n) : "memory");
    return r;
}

int hal_syscall3(int n, uint32_t a, uint32_t b, uint32_t c) {
    int r;
    __asm__ volatile("int $0x90"
                     : "=a"(r)
                     : "a"(n), "b"(a), "c"(b), "d"(c)
                     : "memory");
    return r;
}
