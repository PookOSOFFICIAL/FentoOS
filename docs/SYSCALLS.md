# FentoOS System Call Reference

FentoOS uses a BSD-style system call interface. Userland invokes a syscall by
loading the syscall number into `eax`, placing arguments in registers, and
issuing the software interrupt:

```
int 0x90
```

The return value is delivered in `eax`; a negative value indicates an error.
The trampoline lives in `user/lib/syscall.s`; the kernel-side entry and
dispatch are in `kernel/i386/syscall_entry.c` and `kernel/syscall/syscall.c`.

Syscall numbers are defined in two places that must stay in sync:

- Kernel: `include/syscall.h`
- Userland: `user/include/syscall_nr.h`

## Number table

| Number | Name       | Purpose                                             |
|-------:|------------|-----------------------------------------------------|
| 1      | `exit`     | Terminate the current process                       |
| 2      | `fork`     | Duplicate the current process                       |
| 3      | `read`     | Read from a file descriptor                         |
| 4      | `write`    | Write to a file descriptor                          |
| 5      | `open`     | Open a file                                         |
| 6      | `close`    | Close a file descriptor                             |
| 7      | `wait4`    | Wait for a child process, collecting status/usage   |
| 8      | `creat`    | Create a file                                       |
| 9      | `link`     | Create a hard link                                  |
| 10     | `unlink`   | Remove a directory entry                            |
| 12     | `chdir`    | Change the working directory                        |
| 14     | `mknod`    | Create a filesystem node                            |
| 15     | `chmod`    | Change file mode bits                               |
| 17     | `brk`      | Set the program break (heap end)                    |
| 19     | `lseek`    | Reposition a file offset                            |
| 20     | `getpid`   | Get the process ID                                  |
| 23     | `setuid`   | Set the user ID                                     |
| 24     | `getuid`   | Get the user ID                                     |
| 37     | `kill`     | Send a signal to a process                          |
| 39     | `getppid`  | Get the parent process ID                           |
| 41     | `dup`      | Duplicate a file descriptor                         |
| 42     | `pipe`     | Create a pipe                                       |
| 47     | `getgid`   | Get the group ID                                    |
| 54     | `ioctl`    | Device control (TTY modes)                          |
| 59     | `execve`   | Replace the process image with a new program        |
| 62     | `fstat`    | Get file status by descriptor                       |
| 63     | `stat`     | Get file status by path                             |
| 64     | `getdents` | Read directory entries                              |
| 65     | `yield`    | Yield the CPU to the scheduler                      |
| 66     | `sync`     | Flush filesystem buffers to disk                    |
| 84     | `wait`     | Wait for any child process                          |
| 90     | `dup2`     | Duplicate a descriptor onto a specific number       |
| 136    | `mkdir`    | Create a directory                                  |
| 137    | `rmdir`    | Remove a directory                                  |
| 181    | `setgid`   | Set the group ID                                    |
| 326    | `getcwd`   | Get the current working directory path              |

## ioctl commands

`ioctl` supports the following TTY control requests (see `include/syscall.h`):

| Constant             | Value | Effect                          |
|----------------------|------:|---------------------------------|
| `TTY_SET_RAW`        | 1     | Enable/disable raw input mode   |
| `TTY_SET_ECHO`       | 2     | Enable/disable input echo       |
| `TTY_SET_FOREGROUND` | 3     | Set the foreground process group |

## Calling convention notes

- Syscall number is passed in `eax`.
- Arguments are passed in registers per the trampoline in `user/lib/syscall.s`.
- The kernel saves a full `struct hal_trapframe` on entry so that `fork`,
  `execve`, and `wait4` can inspect and manipulate the caller's register state.
- On the transition from ring 3 to ring 0 the CPU switches to the kernel stack
  recorded in the TSS.
