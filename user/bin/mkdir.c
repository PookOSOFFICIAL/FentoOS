#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        fprintf(2, "usage: mkdir DIR...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (mkdir(argv[i], 0755) < 0) {
            fprintf(2, "mkdir: %s: failed\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
