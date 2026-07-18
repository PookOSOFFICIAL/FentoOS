#include <libc.h>
#include <syscall_nr.h>

void _exit(int code) {
    syscall1(SYS_exit, (uint32_t)code);
    for (;;) ;
}

pid_t fork(void) {
    return syscall0(SYS_fork);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return syscall3(SYS_execve, (uint32_t)path, (uint32_t)argv, (uint32_t)envp);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    return syscall3(SYS_wait4, (uint32_t)pid, (uint32_t)status, (uint32_t)options);
}

pid_t wait(int *status) {
    return syscall3(SYS_wait4, (uint32_t)-1, (uint32_t)status, 0);
}

ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, (uint32_t)fd, (uint32_t)buf, (uint32_t)count);
}

int open(const char *path, int flags, mode_t mode) {
    return syscall3(SYS_open, (uint32_t)path, (uint32_t)flags, (uint32_t)mode);
}

int close(int fd) {
    return syscall1(SYS_close, (uint32_t)fd);
}

int creat(const char *path, mode_t mode) {
    return syscall2(SYS_creat, (uint32_t)path, (uint32_t)mode);
}

int unlink(const char *path) {
    return syscall1(SYS_unlink, (uint32_t)path);
}

int chdir(const char *path) {
    return syscall1(SYS_chdir, (uint32_t)path);
}

int chmod(const char *path, mode_t mode) {
    return syscall2(SYS_chmod, (uint32_t)path, (uint32_t)mode);
}

off_t lseek(int fd, off_t off, int whence) {
    return syscall3(SYS_lseek, (uint32_t)fd, (uint32_t)off, (uint32_t)whence);
}

pid_t getpid(void) {
    return syscall0(SYS_getpid);
}

pid_t getppid(void) {
    return syscall0(SYS_getppid);
}

uid_t getuid(void) {
    return syscall0(SYS_getuid);
}

gid_t getgid(void) {
    return syscall0(SYS_getgid);
}

int setuid(uid_t uid) {
    return syscall1(SYS_setuid, (uint32_t)uid);
}

int setgid(gid_t gid) {
    return syscall1(SYS_setgid, (uint32_t)gid);
}

int kill(pid_t pid, int sig) {
    return syscall2(SYS_kill, (uint32_t)pid, (uint32_t)sig);
}

int ioctl(int fd, int request, uint32_t arg) {
    return syscall3(SYS_ioctl, (uint32_t)fd, (uint32_t)request, arg);
}

int dup(int fd) {
    return syscall1(SYS_dup, (uint32_t)fd);
}

int dup2(int oldfd, int newfd) {
    return syscall2(SYS_dup2, (uint32_t)oldfd, (uint32_t)newfd);
}

int pipe(int fds[2]) {
    return syscall1(SYS_pipe, (uint32_t)fds);
}

int mkdir(const char *path, mode_t mode) {
    return syscall2(SYS_mkdir, (uint32_t)path, (uint32_t)mode);
}

int rmdir(const char *path) {
    return syscall1(SYS_rmdir, (uint32_t)path);
}

int stat(const char *path, struct stat *st) {
    return syscall2(SYS_stat, (uint32_t)path, (uint32_t)st);
}

int fstat(int fd, struct stat *st) {
    return syscall2(SYS_fstat, (uint32_t)fd, (uint32_t)st);
}

int getdents(int fd, struct dirent *out, uint32_t index) {
    return syscall3(SYS_getdents, (uint32_t)fd, (uint32_t)out, index);
}

int yield(void) {
    return syscall0(SYS_yield);
}

int sync(void) {
    return syscall0(SYS_sync);
}

char *getcwd(char *buf, size_t size) {
    int r = syscall2(SYS_getcwd, (uint32_t)buf, (uint32_t)size);
    if (r < 0) return NULL;
    return buf;
}

void *sbrk(int incr) {
    uint32_t cur = (uint32_t)syscall1(SYS_brk, 0);
    if (incr == 0) return (void *)cur;
    uint32_t nw = (uint32_t)syscall1(SYS_brk, cur + (uint32_t)incr);
    if (nw != cur + (uint32_t)incr) return (void *)-1;
    return (void *)cur;
}
