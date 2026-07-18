#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        fprintf(2, "usage: touch FILE...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            fprintf(2, "touch: %s: failed\n", argv[i]);
            rc = 1;
            continue;
        }
        close(fd);
    }
    return rc;
}
