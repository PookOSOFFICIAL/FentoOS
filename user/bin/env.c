#include <libc.h>

int main(int argc, char **argv, char **envp) {
    (void)argc; (void)argv;
    if (envp) {
        for (int i = 0; envp[i]; i++)
            printf("%s\n", envp[i]);
    }
    return 0;
}
