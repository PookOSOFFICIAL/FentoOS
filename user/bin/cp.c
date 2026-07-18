#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc != 3) {
        fprintf(2, "usage: cp SRC DST\n");
        return 1;
    }
    int in = open(argv[1], O_RDONLY, 0);
    if (in < 0) { fprintf(2, "cp: %s: cannot open\n", argv[1]); return 1; }
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { fprintf(2, "cp: %s: cannot create\n", argv[2]); close(in); return 1; }
    char buf[512];
    int n;
    while ((n = read(in, buf, sizeof(buf))) > 0)
        write(out, buf, n);
    close(in);
    close(out);
    return 0;
}
