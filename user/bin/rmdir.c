#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        fprintf(2, "usage: rmdir DIR...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) < 0) {
            fprintf(2, "rmdir: %s: failed\n", argv[i]);
            rc = 1;
        }
    }
    return rc;
}
