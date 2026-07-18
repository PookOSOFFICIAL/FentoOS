#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int nl = 1;
    int start = 1;
    if (argc > 1 && strcmp(argv[1], "-n") == 0) { nl = 0; start = 2; }
    for (int i = start; i < argc; i++) {
        puts(argv[i]);
        if (i < argc - 1) putchar(' ');
    }
    if (nl) putchar('\n');
    return 0;
}
