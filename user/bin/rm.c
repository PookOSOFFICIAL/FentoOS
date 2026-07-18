#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        fprintf(2, "usage: rm FILE...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            fprintf(2, "rm: %s: failed\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
