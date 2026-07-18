#include <libc.h>

static void dump(int fd) {
    char buf[512];
    int n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        write(1, buf, n);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        dump(0);
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            fprintf(2, "cat: %s: cannot open\n", argv[i]);
            rc = 1;
            continue;
        }
        dump(fd);
        close(fd);
    }
    return rc;
}
