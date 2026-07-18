#ifndef _FENTO_SYSCALL_H
#define _FENTO_SYSCALL_H

#include <types.h>

#define SYS_exit     1
#define SYS_fork     2
#define SYS_read     3
#define SYS_write    4
#define SYS_open     5
#define SYS_close    6
#define SYS_wait4    7
#define SYS_creat    8
#define SYS_link     9
#define SYS_unlink   10
#define SYS_chdir    12
#define SYS_mknod    14
#define SYS_chmod    15
#define SYS_lseek    19
#define SYS_getpid   20
#define SYS_getuid   24
#define SYS_kill     37
#define SYS_dup      41
#define SYS_getgid   47
#define SYS_ioctl    54
#define SYS_execve   59
#define SYS_mkdir    136
#define SYS_rmdir    137
#define SYS_fstat    62
#define SYS_stat     63
#define SYS_getdents 64
#define SYS_yield    65
#define SYS_sync     66
#define SYS_pipe     42
#define SYS_dup2     90
#define SYS_getppid  39
#define SYS_setuid   23
#define SYS_setgid   181
#define SYS_getcwd   326
#define SYS_brk      17
#define SYS_wait     84

#define TTY_SET_RAW 1
#define TTY_SET_ECHO 2
#define TTY_SET_FOREGROUND 3

#define SYSCALL_INT 0x90

void syscall_init(void);

#endif
