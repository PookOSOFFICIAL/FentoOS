#include <libc.h>

int main(void) {
    char buf[256];
    if (getcwd(buf, sizeof(buf)))
        printf("%s\n", buf);
    return 0;
}
