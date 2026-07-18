#ifndef _FENTO_SCHED_H
#define _FENTO_SCHED_H

#include <types.h>

#define TASK_UNUSED   0
#define TASK_READY    1
#define TASK_RUNNING  2
#define TASK_BLOCKED  3
#define TASK_ZOMBIE   4

#define MAX_TASKS 64
#define MAX_FILES 32

struct filp;

struct task {
    pid_t pid;
    pid_t ppid;
    char name[32];
    int state;
    struct hal_context *ctx;
    void *kstack;
    uint32_t time_slice;
    struct task *next;
    uid_t uid;
    gid_t gid;
    int exit_code;
    physaddr_t addrspace;
    struct filp *files[MAX_FILES];
    char cwd[256];
    struct task *parent;
    bool waiting;
    bool user;
    virtaddr_t entry;
    virtaddr_t user_stack;
    virtaddr_t brk;
    bool tty_raw;
    bool tty_echo;
};

void sched_init(void);
struct task *sched_create(const char *name, void (*entry)(void));
struct task *sched_alloc(const char *name);
void sched_add(struct task *t);
void sched_start(void);
void sched_yield(void);
void sched_tick(void);
struct task *sched_current(void);
struct task *sched_find(pid_t pid);
struct task *sched_find_slot(int i);
void sched_reap(struct task *t);
void sched_exit(int code);
void sched_block(void);
void sched_unblock(struct task *t);
void sched_wakeup_parent(struct task *t);

#endif
