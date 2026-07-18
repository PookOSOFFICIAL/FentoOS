#include <types.h>
#include <syscall.h>
#include <hal.h>
#include <minixfs.h>
#include <sched.h>
#include <vmm.h>
#include <file.h>
#include <proc.h>
#include <string.h>
#include <console.h>
#include <keyboard.h>

static int fd_alloc(struct task *t) {
    for (int i = 0; i < MAX_FILES; i++)
        if (!t->files[i]) return i;
    return -1;
}

static void build_path(struct task *t, const char *path, char *out) {
    if (path[0] == '/') {
        strncpy(out, path, 255);
        out[255] = 0;
        return;
    }
    strcpy(out, t->cwd);
    size_t len = strlen(out);
    if (len == 0 || out[len - 1] != '/') { out[len++] = '/'; out[len] = 0; }
    strncpy(out + len, path, 255 - len);
    out[255] = 0;
}

static int sys_exit(struct hal_trapframe *tf) {
    sched_exit((int)tf->ebx);
    return 0;
}

static int sys_write(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    const char *buf = (const char *)tf->ecx;
    size_t count = (size_t)tf->edx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    struct filp *f = t->files[fd];
    if (f->type == FT_CONS) {
        for (size_t i = 0; i < count; i++) console_putc(buf[i]);
        return (int)count;
    }
    if (f->type == FT_PIPE) {
        if (f->pipe_end != 1) return -1;
        return pipe_write(f->pipe, buf, count);
    }
    if (f->type == FT_INODE) {
        int n = minixfs_write(f->ino, buf, count, f->offset);
        if (n > 0) f->offset += n;
        return n;
    }
    return -1;
}

static int sys_read(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    void *buf = (void *)tf->ecx;
    size_t count = (size_t)tf->edx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    struct filp *f = t->files[fd];
    if (f->type == FT_CONS) {
        extern int keyboard_getchar(void);
        char *out = (char *)buf;
        size_t got = 0;
        while (got < count) {
            int c = keyboard_getchar();
            if (c < 0) { hal_sti(); sched_yield(); continue; }
            if (t->tty_echo) console_putc((char)c);
            out[got++] = (char)c;
            if (t->tty_raw || c == '\n') break;
        }
        return (int)got;
    }
    if (f->type == FT_PIPE) {
        if (f->pipe_end != 0) return -1;
        return pipe_read(f->pipe, buf, count);
    }
    if (f->type == FT_INODE) {
        int n = minixfs_read(f->ino, buf, count, f->offset);
        if (n > 0) f->offset += n;
        return n;
    }
    return -1;
}

static int sys_open(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    const char *path = (const char *)tf->ebx;
    int flags = (int)tf->ecx;
    mode_t mode = (mode_t)tf->edx;
    char full[256];
    build_path(t, path, full);
    int r = minixfs_open(full, flags, mode);
    if (r < 0) return -1;
    int fd = fd_alloc(t);
    if (fd < 0) return -1;
    struct filp *f = filp_alloc();
    if (!f) return -1;
    f->type = FT_INODE;
    f->ino = (ino_t)r;
    f->offset = 0;
    f->flags = flags;
    f->is_dir = minixfs_is_dir((ino_t)r);
    if (flags & O_APPEND) f->offset = minixfs_size((ino_t)r);
    t->files[fd] = f;
    return fd;
}

static int sys_close(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    filp_close(t->files[fd]);
    t->files[fd] = NULL;
    return 0;
}

static int sys_creat(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    const char *path = (const char *)tf->ebx;
    mode_t mode = (mode_t)tf->ecx;
    char full[256];
    build_path(t, path, full);
    int r = minixfs_creat(full, mode);
    if (r < 0) return -1;
    int fd = fd_alloc(t);
    if (fd < 0) return -1;
    struct filp *f = filp_alloc();
    f->type = FT_INODE;
    f->ino = (ino_t)r;
    f->offset = 0;
    f->flags = O_WRONLY;
    t->files[fd] = f;
    return fd;
}

static int sys_unlink(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char full[256];
    build_path(t, (const char *)tf->ebx, full);
    return minixfs_unlink(full);
}

static int sys_chmod(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char full[256];
    build_path(t, (const char *)tf->ebx, full);
    return minixfs_chmod(full, (mode_t)tf->ecx);
}

static int sys_lseek(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    off_t off = (off_t)tf->ecx;
    int whence = (int)tf->edx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    struct filp *f = t->files[fd];
    if (f->type != FT_INODE) return -1;
    if (whence == 0) f->offset = off;
    else if (whence == 1) f->offset += off;
    else if (whence == 2) f->offset = minixfs_size(f->ino) + off;
    return f->offset;
}

static int sys_getpid(struct hal_trapframe *tf) {
    (void)tf;
    struct task *t = sched_current();
    return t ? t->pid : 0;
}

static int sys_getppid(struct hal_trapframe *tf) {
    (void)tf;
    struct task *t = sched_current();
    return t ? t->ppid : 0;
}

static int sys_getuid(struct hal_trapframe *tf) {
    (void)tf;
    struct task *t = sched_current();
    return t ? t->uid : 0;
}

static int sys_getgid(struct hal_trapframe *tf) {
    (void)tf;
    struct task *t = sched_current();
    return t ? t->gid : 0;
}

static int sys_setuid(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    if (t->uid != 0) return -1;
    t->uid = (uid_t)tf->ebx;
    return 0;
}

static int sys_setgid(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    if (t->uid != 0) return -1;
    t->gid = (gid_t)tf->ebx;
    return 0;
}

static int sys_yield(struct hal_trapframe *tf) {
    (void)tf;
    sched_yield();
    return 0;
}

static int sys_mkdir(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char full[256];
    build_path(t, (const char *)tf->ebx, full);
    return minixfs_mkdir(full, (mode_t)tf->ecx);
}

static int sys_rmdir(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char full[256];
    build_path(t, (const char *)tf->ebx, full);
    return minixfs_rmdir(full);
}

static int sys_stat(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char full[256];
    build_path(t, (const char *)tf->ebx, full);
    return minixfs_stat(full, (struct stat *)tf->ecx);
}

static int sys_fstat(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    struct stat *st = (struct stat *)tf->ecx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    struct filp *f = t->files[fd];
    memset(st, 0, sizeof(*st));
    if (f->type == FT_INODE) {
        st->st_ino = f->ino;
        st->st_size = minixfs_size(f->ino);
        st->st_mode = minixfs_is_dir(f->ino) ? S_IFDIR : S_IFREG;
    } else {
        st->st_mode = S_IFCHR;
    }
    return 0;
}

static int sys_getdents(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    struct dirent *out = (struct dirent *)tf->ecx;
    uint32_t index = (uint32_t)tf->edx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    struct filp *f = t->files[fd];
    if (f->type != FT_INODE) return -1;
    return minixfs_readdir(f->ino, index, out);
}

static int sys_chdir(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    const char *path = (const char *)tf->ebx;
    char full[256];
    build_path(t, path, full);
    ino_t ino = minixfs_resolve(full);
    if (ino == 0 || !minixfs_is_dir(ino)) return -1;
    strncpy(t->cwd, full, sizeof(t->cwd) - 1);
    t->cwd[sizeof(t->cwd) - 1] = 0;
    return 0;
}

static int sys_getcwd(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    char *buf = (char *)tf->ebx;
    size_t size = (size_t)tf->ecx;
    size_t l = strlen(t->cwd);
    if (l + 1 > size) return -1;
    strcpy(buf, t->cwd);
    return (int)l;
}

static int sys_sync(struct hal_trapframe *tf) {
    (void)tf;
    return 0;
}

static int sys_dup(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd]) return -1;
    int nfd = fd_alloc(t);
    if (nfd < 0) return -1;
    t->files[nfd] = t->files[fd];
    filp_ref(t->files[fd]);
    return nfd;
}

static int sys_dup2(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int oldfd = (int)tf->ebx;
    int newfd = (int)tf->ecx;
    if (oldfd < 0 || oldfd >= MAX_FILES || !t->files[oldfd]) return -1;
    if (newfd < 0 || newfd >= MAX_FILES) return -1;
    if (oldfd == newfd) return newfd;
    if (t->files[newfd]) filp_close(t->files[newfd]);
    t->files[newfd] = t->files[oldfd];
    filp_ref(t->files[oldfd]);
    return newfd;
}

static int sys_pipe(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int *fds = (int *)tf->ebx;
    int rfd = fd_alloc(t);
    if (rfd < 0) return -1;
    struct filp *rf = filp_alloc();
    t->files[rfd] = rf;
    int wfd = fd_alloc(t);
    if (wfd < 0) { filp_close(rf); t->files[rfd] = NULL; return -1; }
    struct pipe *p = pipe_create();
    if (!p) { filp_close(rf); t->files[rfd] = NULL; return -1; }
    struct filp *wf = filp_alloc();
    rf->type = FT_PIPE; rf->pipe = p; rf->pipe_end = 0;
    wf->type = FT_PIPE; wf->pipe = p; wf->pipe_end = 1;
    t->files[wfd] = wf;
    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}

static int sys_brk(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    virtaddr_t addr = (virtaddr_t)tf->ebx;
    if (addr == 0) return (int)t->brk;
    virtaddr_t old = t->brk;
    if (addr > old) {
        virtaddr_t va = (old + 0xFFF) & ~0xFFFu;
        for (; va < ((addr + 0xFFF) & ~0xFFFu); va += 0x1000)
            hal_addrspace_map_user(t->addrspace, va, 0);
    }
    t->brk = addr;
    return (int)addr;
}

static int sys_kill(struct hal_trapframe *tf) {
    pid_t pid = (pid_t)tf->ebx;
    struct task *t = sched_find(pid);
    if (!t) return -1;
    if (t->state != TASK_ZOMBIE) {
        t->exit_code = 137;
        t->state = TASK_ZOMBIE;
        sched_wakeup_parent(t);
    }
    return 0;
}

static int sys_ioctl(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    int fd = (int)tf->ebx;
    int request = (int)tf->ecx;
    uint32_t arg = tf->edx;
    if (fd < 0 || fd >= MAX_FILES || !t->files[fd] || t->files[fd]->type != FT_CONS) return -1;
    if (request == TTY_SET_RAW) t->tty_raw = arg != 0;
    else if (request == TTY_SET_ECHO) t->tty_echo = arg != 0;
    else if (request == TTY_SET_FOREGROUND) keyboard_set_foreground((pid_t)arg);
    else return -1;
    return 0;
}

static int sys_stub(struct hal_trapframe *tf) {
    (void)tf;
    return -1;
}

static int dispatch(struct hal_trapframe *tf) {
    switch (tf->eax) {
        case SYS_exit:     return sys_exit(tf);
        case SYS_fork:     return sys_fork_impl(tf);
        case SYS_read:     return sys_read(tf);
        case SYS_write:    return sys_write(tf);
        case SYS_open:     return sys_open(tf);
        case SYS_close:    return sys_close(tf);
        case SYS_wait4:    return sys_wait4_impl(tf);
        case SYS_wait:     return sys_wait4_impl(tf);
        case SYS_creat:    return sys_creat(tf);
        case SYS_link:     return sys_stub(tf);
        case SYS_unlink:   return sys_unlink(tf);
        case SYS_chdir:    return sys_chdir(tf);
        case SYS_mknod:    return sys_stub(tf);
        case SYS_chmod:    return sys_chmod(tf);
        case SYS_lseek:    return sys_lseek(tf);
        case SYS_getpid:   return sys_getpid(tf);
        case SYS_getppid:  return sys_getppid(tf);
        case SYS_getuid:   return sys_getuid(tf);
        case SYS_kill:     return sys_kill(tf);
        case SYS_dup:      return sys_dup(tf);
        case SYS_dup2:     return sys_dup2(tf);
        case SYS_pipe:     return sys_pipe(tf);
        case SYS_getgid:   return sys_getgid(tf);
        case SYS_setuid:   return sys_setuid(tf);
        case SYS_setgid:   return sys_setgid(tf);
        case SYS_ioctl:    return sys_ioctl(tf);
        case SYS_execve:   return sys_execve_impl(tf);
        case SYS_fstat:    return sys_fstat(tf);
        case SYS_stat:     return sys_stat(tf);
        case SYS_getdents: return sys_getdents(tf);
        case SYS_getcwd:   return sys_getcwd(tf);
        case SYS_brk:      return sys_brk(tf);
        case SYS_yield:    return sys_yield(tf);
        case SYS_sync:     return sys_sync(tf);
        case SYS_mkdir:    return sys_mkdir(tf);
        case SYS_rmdir:    return sys_rmdir(tf);
        default:           return -1;
    }
}

void syscall_init(void) {
    hal_syscall_set_dispatcher(dispatch);
    hal_syscall_init();
}
