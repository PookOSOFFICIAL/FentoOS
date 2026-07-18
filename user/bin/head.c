#include <libc.h>

static void head_fd(int fd, int nlines) {
    char buf[512];
    int n;
    int printed = 0;
    while (printed < nlines && (n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            write(1, &buf[i], 1);
            if (buf[i] == '\n') {
                printed++;
                if (printed >= nlines) return;
            }
        }
    }
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int nlines = 10;
    int start = 1;
    if (argc > 2 && strcmp(argv[1], "-n") == 0) {
        nlines = atoi(argv[2]);
        start = 3;
    }
    if (start >= argc) {
        head_fd(0, nlines);
        return 0;
    }
    for (int i = start; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) { fprintf(2, "head: %s: cannot open\n", argv[i]); continue; }
        head_fd(fd, nlines);
        close(fd);
    }
    return 0;
}
