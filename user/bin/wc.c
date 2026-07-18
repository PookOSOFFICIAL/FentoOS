#include <libc.h>

static void count_fd(int fd, uint32_t *lines, uint32_t *words, uint32_t *bytes) {
    char buf[512];
    int n;
    int inword = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            (*bytes)++;
            char c = buf[i];
            if (c == '\n') (*lines)++;
            if (c == ' ' || c == '\t' || c == '\n') {
                inword = 0;
            } else if (!inword) {
                inword = 1;
                (*words)++;
            }
        }
    }
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    uint32_t l = 0, w = 0, b = 0;
    if (argc < 2) {
        count_fd(0, &l, &w, &b);
        printf("%u %u %u\n", l, w, b);
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY, 0);
        if (fd < 0) { fprintf(2, "wc: %s: cannot open\n", argv[i]); continue; }
        uint32_t fl = 0, fw = 0, fb = 0;
        count_fd(fd, &fl, &fw, &fb);
        close(fd);
        printf("%u %u %u %s\n", fl, fw, fb, argv[i]);
        l += fl; w += fw; b += fb;
    }
    return 0;
}
