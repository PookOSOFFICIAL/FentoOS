#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)envp;
    if (argc < 2) {
        fprintf(2, "usage: stat FILE...\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) < 0) {
            fprintf(2, "stat: %s: not found\n", argv[i]);
            rc = 1;
            continue;
        }
        printf("  File: %s\n", argv[i]);
        printf("  Size: %u\n", (uint32_t)st.st_size);
        printf("  Inode: %u\n", (uint32_t)st.st_ino);
        printf("  Mode: %x\n", (uint32_t)st.st_mode);
        printf("  Type: %s\n", S_ISDIR(st.st_mode) ? "directory" :
                               (S_ISREG(st.st_mode) ? "regular file" : "special"));
        printf("  Uid: %u  Gid: %u\n", (uint32_t)st.st_uid, (uint32_t)st.st_gid);
    }
    return rc;
}
