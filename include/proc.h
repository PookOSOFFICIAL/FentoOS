#ifndef _FENTO_PROC_H
#define _FENTO_PROC_H

#include <types.h>
#include <sched.h>
#include <hal.h>

#define USER_LOAD_BASE   0x40000000u
#define USER_STACK_TOP   0xC0000000u
#define USER_STACK_SIZE  0x00040000u
#define USER_ARG_TOP     0xBFFFF000u

int  proc_exec(struct task *t, const char *path, char *const argv[], char *const envp[]);
int  sys_fork_impl(struct hal_trapframe *tf);
int  sys_execve_impl(struct hal_trapframe *tf);
int  sys_wait4_impl(struct hal_trapframe *tf);
void proc_start_init(const char *path);

#endif
