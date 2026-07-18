#include <libc.h>

static int longfmt;

static void modestr(mode_t m, char *out) {
    out[0] = S_ISDIR(m) ? 'd' : (((m & S_IFMT) == S_IFCHR) ? 'c' : '-');
    out[1] = (m & 0400) ? 'r' : '-';
    out[2] = (m & 0200) ? 'w' : '-';
    out[3] = (m & 0100) ? 'x' : '-';
    out[4] = (m & 0040) ? 'r' : '-';
    out[5] = (m & 0020) ? 'w' : '-';
    out[6] = (m & 0010) ? 'x' : '-';
    out[7] = (m & 0004) ? 'r' : '-';
    out[8] = (m & 0002) ? 'w' : '-';
    out[9] = (m & 0001) ? 'x' : '-';
    out[10] = 0;
}

static void list_dir(const char *path) {
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        fprintf(2, "ls: %s: cannot open\n", path);
        return;
    }
    struct dirent de;
    char full[512];
    for (uint32_t i = 0; ; i++) {
        int r = getdents(fd, &de, i);
        if (r <= 0) break;
        if (de.d_name[0] == 0) continue;
        if (strcmp(de.d_name, ".") == 0 || strcmp(de.d_name, "..") == 0)
            continue;
        if (longfmt) {
            strcpy(full, path);
            int l = strlen(full);
            if (l && full[l-1] != '/') { full[l++] = '/'; full[l] = 0; }
            strcat(full, de.d_name);
            struct stat st;
            if (stat(full, &st) == 0) {
                char ms[11];
                modestr(st.st_mode, ms);
                printf("%s %u %s\n", ms, (uint32_t)st.st_size, de.d_name);
            } else {
                printf("?????????? 0 %s\n", de.d_name);
            }
        } else {
            printf("%s\n", de.d_name);
        }
    }
    close(fd);
}

int main(int argc, char **argv, char **envp) {
    (void)envp;
    int start = 1;
    if (argc > 1 && strcmp(argv[1], "-l") == 0) { longfmt = 1; start = 2; }
    if (start >= argc) {
        list_dir(".");
    } else {
        for (int i = start; i < argc; i++) {
            if (argc - start > 1) printf("%s:\n", argv[i]);
            list_dir(argv[i]);
        }
    }
    return 0;
}
