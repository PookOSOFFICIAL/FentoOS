#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int secs = argc > 1 ? atoi(argv[1]) : 1;
    if (secs < 0) secs = 0;
    uint32_t iters = (uint32_t)secs * 200000u;
    for (uint32_t i = 0; i < iters; i++)
        yield();
    return 0;
}
