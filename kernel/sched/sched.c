#include <types.h>
#include <sched.h>
#include <hal.h>
#include <vmm.h>
#include <file.h>
#include <string.h>
#include <console.h>

#define KSTACK_SIZE 32768
#define QUANTUM     5

static struct task *tasks[MAX_TASKS];
static struct task *current;
static struct task *task_list;
static pid_t next_pid;
static bool started;

static struct task idle_task;
static uint8_t idle_stack[16384] __attribute__((aligned(16)));

static void idle_entry(void) {
    for (;;) {
        hal_sti();
        hal_hlt();
    }
}

void sched_init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_list = NULL;
    current = NULL;
    next_pid = 1;
    started = false;

    memset(&idle_task, 0, sizeof(idle_task));
    idle_task.pid = 0;
    strcpy(idle_task.name, "idle");
    idle_task.state = TASK_READY;
    idle_task.kstack = idle_stack;
    idle_task.time_slice = QUANTUM;
    idle_task.addrspace = hal_paging_kernel_dir();
    hal_context_init(&idle_task.ctx, idle_stack + sizeof(idle_stack), idle_entry, false, 0);
}

struct task *sched_alloc(const char *name) {
    uint32_t flags = hal_irq_save();
    struct task *t = (struct task *)kmalloc(sizeof(struct task));
    if (!t) { hal_irq_restore(flags); return NULL; }
    memset(t, 0, sizeof(struct task));
    t->pid = next_pid++;
    strncpy(t->name, name, sizeof(t->name) - 1);
    t->state = TASK_READY;
    t->time_slice = QUANTUM;
    t->uid = 0;
    t->gid = 0;
    t->addrspace = hal_paging_kernel_dir();
    t->tty_echo = true;
    t->tty_raw = false;
    strcpy(t->cwd, "/");
    t->kstack = kmalloc(KSTACK_SIZE);

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (!tasks[i]) { slot = i; break; }
    }
    if (slot >= 0) tasks[slot] = t;
    hal_irq_restore(flags);
    return t;
}

void sched_add(struct task *t) {
    uint32_t flags = hal_irq_save();
    if (!task_list) {
        task_list = t;
        t->next = t;
    } else {
        struct task *p = task_list;
        while (p->next != task_list) p = p->next;
        p->next = t;
        t->next = task_list;
    }
    hal_irq_restore(flags);
}

struct task *sched_create(const char *name, void (*entry)(void)) {
    struct task *t = sched_alloc(name);
    if (!t) return NULL;
    hal_context_init(&t->ctx, (uint8_t *)t->kstack + KSTACK_SIZE, entry, false, 0);
    sched_add(t);
    return t;
}

struct task *sched_find(pid_t pid) {
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i] && tasks[i]->pid == pid) return tasks[i];
    return NULL;
}

struct task *sched_find_slot(int i) {
    if (i < 0 || i >= MAX_TASKS) return NULL;
    return tasks[i];
}

void sched_reap(struct task *t) {
    uint32_t flags = hal_irq_save();
    struct task *p = task_list;
    if (p) {
        do {
            if (p->next == t) {
                p->next = t->next;
                if (task_list == t) task_list = (t->next == t) ? NULL : t->next;
                break;
            }
            p = p->next;
        } while (p != task_list);
    }
    for (int i = 0; i < MAX_TASKS; i++)
        if (tasks[i] == t) tasks[i] = NULL;
    if (t->addrspace && t->addrspace != hal_paging_kernel_dir())
        hal_addrspace_destroy(t->addrspace);
    if (t->kstack) kfree(t->kstack);
    kfree(t);
    hal_irq_restore(flags);
}

static struct task *pick_next(void) {
    struct task *start = current ? current->next : task_list;
    if (!start) return &idle_task;
    struct task *t = start;
    do {
        if (t->state == TASK_READY || t->state == TASK_RUNNING)
            return t;
        t = t->next;
    } while (t != start);
    return &idle_task;
}

static void switch_to(struct task *next) {
    struct task *prev = current;
    current = next;
    next->state = TASK_RUNNING;
    next->time_slice = QUANTUM;
    hal_tss_set_kernel_stack((virtaddr_t)((uint8_t *)next->kstack + KSTACK_SIZE));
    if (next->addrspace && next->addrspace != hal_addrspace_current())
        hal_paging_switch(next->addrspace);
    hal_context_switch(&prev->ctx, next->ctx);
}

void sched_start(void) {
    started = true;
    current = task_list ? task_list : &idle_task;
    current->state = TASK_RUNNING;
    hal_tss_set_kernel_stack((virtaddr_t)((uint8_t *)current->kstack + KSTACK_SIZE));
    if (current->addrspace) hal_paging_switch(current->addrspace);
    struct hal_context *dummy;
    hal_context_switch(&dummy, current->ctx);
}

void sched_yield(void) {
    if (!started) return;
    uint32_t flags = hal_irq_save();
    struct task *next = pick_next();
    if (next != current) {
        if (current->state == TASK_RUNNING) current->state = TASK_READY;
        switch_to(next);
    }
    hal_irq_restore(flags);
}

void sched_tick(void) {
    if (!started || !current) return;
    if (current->time_slice > 0) current->time_slice--;
    if (current->time_slice == 0) {
        struct task *next = pick_next();
        if (next != current) {
            if (current->state == TASK_RUNNING) current->state = TASK_READY;
            switch_to(next);
        } else {
            current->time_slice = QUANTUM;
        }
    }
}

struct task *sched_current(void) {
    return current;
}

void sched_wakeup_parent(struct task *t) {
    if (t->parent && t->parent->state == TASK_BLOCKED && t->parent->waiting) {
        t->parent->waiting = false;
        t->parent->state = TASK_READY;
    }
}

void sched_exit(int code) {
    uint32_t flags = hal_irq_save();
    current->exit_code = code;
    current->state = TASK_ZOMBIE;

    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i] && tasks[i]->parent == current) {
            tasks[i]->parent = NULL;
            tasks[i]->ppid = 1;
        }
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (current->files[i]) {
            filp_close(current->files[i]);
            current->files[i] = NULL;
        }
    }
    sched_wakeup_parent(current);

    struct task *next = pick_next();
    switch_to(next);
    hal_irq_restore(flags);
    for (;;) hal_hlt();
}

void sched_block(void) {
    uint32_t flags = hal_irq_save();
    current->state = TASK_BLOCKED;
    struct task *next = pick_next();
    if (next != current) switch_to(next);
    hal_irq_restore(flags);
}

void sched_unblock(struct task *t) {
    uint32_t flags = hal_irq_save();
    if (t->state == TASK_BLOCKED) t->state = TASK_READY;
    hal_irq_restore(flags);
}
