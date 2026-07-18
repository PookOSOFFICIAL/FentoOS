#include <types.h>
#include <proc.h>
#include <sched.h>
#include <hal.h>
#include <elf.h>
#include <minixfs.h>
#include <file.h>
#include <vmm.h>
#include <string.h>
#include <console.h>
#include <syscall.h>

#define PAGE_SIZE 4096
#define MAX_ARGS  32
#define ARG_MAX   2048

static void user_bounce(void) {
    struct task *t = sched_current();
    hal_enter_user(t->entry, t->user_stack);
}

static void copy_strings(char *const src[], char *dst[], char *buf, uint32_t *used, int *count) {
    int n = 0;
    uint32_t off = 0;
    if (src) {
        while (src[n] && n < MAX_ARGS - 1) {
            size_t len = strlen(src[n]) + 1;
            if (off + len > ARG_MAX) break;
            memcpy(buf + off, src[n], len);
            dst[n] = buf + off;
            off += len;
            n++;
        }
    }
    dst[n] = NULL;
    *used = off;
    *count = n;
}

static virtaddr_t setup_user_stack(physaddr_t dir, char *argv[], int argc,
                                   char *envp[], int envc, char *strbuf) {
    (void)envc;
    for (virtaddr_t va = USER_STACK_TOP - USER_STACK_SIZE; va < USER_STACK_TOP; va += PAGE_SIZE)
        hal_addrspace_map_user(dir, va, 0);

    uint32_t total_str = 0;
    for (int i = 0; i < argc; i++) total_str += strlen(argv[i]) + 1;
    for (int i = 0; i < envc; i++) total_str += strlen(envp[i]) + 1;

    virtaddr_t sp = USER_STACK_TOP;
    sp -= total_str;
    sp &= ~3u;
    virtaddr_t str_base = sp;

    virtaddr_t uargv[MAX_ARGS];
    virtaddr_t uenvp[MAX_ARGS];
    uint32_t soff = 0;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]) + 1;
        hal_addrspace_copy_to(dir, str_base + soff, argv[i], len);
        uargv[i] = str_base + soff;
        soff += len;
    }
    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]) + 1;
        hal_addrspace_copy_to(dir, str_base + soff, envp[i], len);
        uenvp[i] = str_base + soff;
        soff += len;
    }

    sp -= (envc + 1) * 4;
    virtaddr_t envp_base = sp;
    for (int i = 0; i < envc; i++)
        hal_addrspace_copy_to(dir, envp_base + i * 4, &uenvp[i], 4);
    virtaddr_t nullv = 0;
    hal_addrspace_copy_to(dir, envp_base + envc * 4, &nullv, 4);

    sp -= (argc + 1) * 4;
    virtaddr_t argv_base = sp;
    for (int i = 0; i < argc; i++)
        hal_addrspace_copy_to(dir, argv_base + i * 4, &uargv[i], 4);
    hal_addrspace_copy_to(dir, argv_base + argc * 4, &nullv, 4);

    sp -= 4;
    hal_addrspace_copy_to(dir, sp, &envp_base, 4);
    sp -= 4;
    hal_addrspace_copy_to(dir, sp, &argv_base, 4);
    sp -= 4;
    hal_addrspace_copy_to(dir, sp, &argc, 4);
    sp -= 4;
    hal_addrspace_copy_to(dir, sp, &nullv, 4);

    (void)strbuf;
    return sp;
}

int proc_exec(struct task *t, const char *path, char *const argv[], char *const envp[]) {
    ino_t ino = minixfs_resolve(path);
    if (ino == 0) return -1;
    if (minixfs_is_dir(ino)) return -1;

    char argbuf[ARG_MAX];
    char envbuf[ARG_MAX];
    char *kargv[MAX_ARGS];
    char *kenvp[MAX_ARGS];
    uint32_t au, eu;
    int argc, envc;
    copy_strings(argv, kargv, argbuf, &au, &argc);
    copy_strings(envp, kenvp, envbuf, &eu, &envc);
    (void)au; (void)eu;

    physaddr_t newdir = hal_addrspace_create();
    if (!newdir) return -1;

    virtaddr_t entry, brk;
    if (elf_load(newdir, ino, &entry, &brk) < 0) {
        hal_addrspace_destroy(newdir);
        return -1;
    }

    virtaddr_t sp = setup_user_stack(newdir, kargv, argc, kenvp, envc, argbuf);

    physaddr_t olddir = t->addrspace;
    t->addrspace = newdir;
    t->entry = entry;
    t->user_stack = sp;
    t->brk = brk;
    t->user = true;

    if (olddir && olddir != hal_paging_kernel_dir())
        hal_addrspace_destroy(olddir);

    return 0;
}

void proc_start_init(const char *path) {
    struct task *t = sched_alloc("init");
    if (!t) return;
    t->user = true;
    t->uid = 0;
    t->gid = 0;
    strcpy(t->cwd, "/");
    t->addrspace = hal_paging_kernel_dir();

    char *argv[] = { (char *)path, NULL };
    char *envp[] = { "PATH=/bin", "HOME=/", NULL };
    if (proc_exec(t, path, argv, envp) < 0) {
        console_printf("[init] failed to exec %s\n", path);
        return;
    }
    struct filp *cons = filp_alloc();
    cons->type = FT_CONS;
    t->files[0] = cons;
    filp_ref(cons);
    t->files[1] = cons;
    filp_ref(cons);
    t->files[2] = cons;

    extern void proc_bounce_entry(void);
    hal_context_init(&t->ctx, (uint8_t *)t->kstack + 32768, proc_bounce_entry, false, 0);
    sched_add(t);
}

void proc_bounce_entry(void) {
    user_bounce();
}

int sys_fork_impl(struct hal_trapframe *tf) {
    struct task *parent = sched_current();
    struct task *child = sched_alloc(parent->name);
    if (!child) return -1;

    child->ppid = parent->pid;
    child->parent = parent;
    child->uid = parent->uid;
    child->gid = parent->gid;
    child->user = true;
    strcpy(child->cwd, parent->cwd);
    child->brk = parent->brk;

    child->addrspace = hal_addrspace_clone(parent->addrspace);
    if (!child->addrspace) return -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (parent->files[i]) {
            child->files[i] = parent->files[i];
            filp_ref(parent->files[i]);
        }
    }

    struct hal_trapframe *ctf = (struct hal_trapframe *)
        ((uint8_t *)child->kstack + 32768 - sizeof(struct hal_trapframe));
    memcpy(ctf, tf, sizeof(struct hal_trapframe));
    ctf->eax = 0;

    extern void proc_fork_return(void);
    struct hal_context {
        uint32_t edi, esi, ebx, ebp, eip;
    } *c;
    uint32_t *sp = (uint32_t *)ctf;
    *(--sp) = (uint32_t)ctf;
    *(--sp) = 0;
    c = (struct hal_context *)((uint32_t)sp - sizeof(struct hal_context));
    c->eip = (uint32_t)proc_fork_return;
    c->ebp = 0; c->ebx = 0; c->esi = 0; c->edi = 0;
    child->ctx = (void *)c;

    sched_add(child);
    return child->pid;
}

void proc_fork_return_c(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    hal_paging_switch(t->addrspace);
    hal_tss_set_kernel_stack((virtaddr_t)((uint8_t *)t->kstack + 32768));
    hal_return_to_user(tf);
}

int sys_execve_impl(struct hal_trapframe *tf) {
    struct task *t = sched_current();
    const char *path = (const char *)tf->ebx;
    char *const *argv = (char *const *)tf->ecx;
    char *const *envp = (char *const *)tf->edx;

    char kpath[256];
    strncpy(kpath, path, sizeof(kpath) - 1);
    kpath[255] = 0;

    char full[256];
    if (kpath[0] == '/') {
        strcpy(full, kpath);
    } else {
        strcpy(full, t->cwd);
        size_t l = strlen(full);
        if (l == 0 || full[l-1] != '/') { full[l++] = '/'; full[l] = 0; }
        strncpy(full + l, kpath, 255 - l);
    }

    char argbuf[ARG_MAX];
    char *kargv[MAX_ARGS];
    uint32_t used;
    int argc;
    copy_strings(argv, kargv, argbuf, &used, &argc);

    char envbuf[ARG_MAX];
    char *kenvp[MAX_ARGS];
    int envc;
    copy_strings(envp, kenvp, envbuf, &used, &envc);

    ino_t ino = minixfs_resolve(full);
    if (ino == 0 || minixfs_is_dir(ino)) return -1;

    physaddr_t newdir = hal_addrspace_create();
    if (!newdir) return -1;
    virtaddr_t entry, brk;
    if (elf_load(newdir, ino, &entry, &brk) < 0) {
        hal_addrspace_destroy(newdir);
        return -1;
    }
    virtaddr_t sp = setup_user_stack(newdir, kargv, argc, kenvp, envc, argbuf);

    physaddr_t olddir = t->addrspace;
    t->addrspace = newdir;
    t->entry = entry;
    t->user_stack = sp;
    t->brk = brk;

    hal_paging_switch(newdir);
    if (olddir && olddir != hal_paging_kernel_dir())
        hal_addrspace_destroy(olddir);

    hal_tss_set_kernel_stack((virtaddr_t)((uint8_t *)t->kstack + 32768));
    hal_enter_user(entry, sp);
    return 0;
}

int sys_wait4_impl(struct hal_trapframe *tf) {
    struct task *parent = sched_current();
    int *status = (int *)tf->ecx;

    for (;;) {
        bool have_child = false;
        for (int i = 0; i < MAX_TASKS; i++) {
            struct task *t = sched_find_slot(i);
            if (!t) continue;
            if (t->parent != parent) continue;
            have_child = true;
            if (t->state == TASK_ZOMBIE) {
                pid_t pid = t->pid;
                if (status) {
                    int st = (t->exit_code & 0xFF) << 8;
                    hal_addrspace_copy_to(parent->addrspace, (virtaddr_t)status, &st, 4);
                }
                sched_reap(t);
                return pid;
            }
        }
        if (!have_child) return -1;
        parent->waiting = true;
        sched_block();
    }
}

void proc_page_fault(struct hal_trapframe *tf, uint32_t addr) {
    struct task *t = sched_current();
    if (t && t->user && (tf->cs & 3) == 3) {
        console_printf("\n[SEGV] pid=%d addr=%x eip=%x err=%x -> killing\n",
                       t->pid, addr, tf->eip, tf->err_code);
        sched_exit(139);
        return;
    }
    console_printf("\n[PAGE FAULT] addr=%x eip=%x err=%x\n", addr, tf->eip, tf->err_code);
    for (;;) hal_hlt();
}
