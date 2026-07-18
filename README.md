# FentoOS

A small, self-contained hobby operating system for the 32-bit x86 (i386)
architecture. FentoOS boots via Multiboot/GRUB, runs a preemptive multitasking
kernel with a full virtual memory system, mounts a real MINIX v3 filesystem
from an ATA disk, and drops into a ring-3 userland shell backed by a
BSD-style system call interface and a small self-hosted toolchain.

The entire system — bootstrap, memory management, scheduler, drivers,
filesystem, C library and userland utilities — is built from source with a
plain `gcc` / `ld` / `nasm` cross-toolchain and runs under QEMU.

## Highlights

- **Multiboot boot path** via GRUB (`grub-mkrescue` ISO).
- **Segmentation**: flat GDT + TSS for ring-0/ring-3 privilege separation.
- **Interrupts**: full IDT, PIC remap, ISR/IRQ stubs, exception handlers.
- **Timer**: PIT programmed at 100 Hz driving the preemptive scheduler.
- **Physical memory manager (PMM)**: frame allocator over Multiboot memory map.
- **Virtual memory manager (VMM)**: paging enabled, per-process address spaces,
  kernel heap allocator.
- **Scheduler**: preemptive round-robin task scheduling with context switching.
- **Processes**: `fork`, `execve`, `wait4`, ELF loading, user stack/argv/envp setup.
- **System calls**: BSD-style numbering via `int 0x90` (36 syscalls).
- **Drivers**: VGA text console, 16550 serial (COM1), PS/2 keyboard, ATA PIO disk.
- **Filesystem**: MINIX v3 read/write with a VFS-style file layer.
- **Userland**: freestanding C library (`flibc`), `crt0`, `malloc`, `stdio`,
  a shell, ~20 coreutils, plus a tiny self-hosted assembler/compiler
  (`fasm`, `fcc`) and a BASIC interpreter (`basic`).

## Repository layout

```
.
├── Makefile              Top-level build (kernel + ISO + disk image)
├── qrun.sh               Scripted QEMU launcher (feeds shell commands)
├── test_full.sh          End-to-end smoke test under QEMU
├── include/              Kernel public headers
├── kernel/
│   ├── i386/             Arch: boot, GDT, IDT, paging, PIT, context switch,
│   │                     syscall entry, ELF loader, linker script
│   ├── hal/              Hardware abstraction layer glue
│   ├── mm/               pmm.c (physical), vmm.c (virtual + heap)
│   ├── sched/            sched.c preemptive scheduler
│   ├── proc/             proc.c fork/exec/wait, process lifecycle
│   ├── syscall/          syscall.c dispatch table
│   ├── fs/               minixfs.c + file.c (VFS layer)
│   ├── drivers/          vga, serial, keyboard, ata
│   ├── lib/              string.c freestanding helpers
│   └── main.c            Kernel entry (kernel_main)
├── user/
│   ├── include/          libc.h, syscall_nr.h
│   ├── lib/              flibc: crt0.s, syscall.s, unistd, stdio, malloc, string
│   ├── bin/              Userland programs (sh + coreutils + toolchain)
│   ├── examples/         hello.c / hello.s / demo.bas + build.sh
│   └── Makefile          Userland build
└── tools/
    └── mkminix3.py       Builds a MINIX v3 disk image from userland binaries
```

## Building

### Prerequisites

A Linux host (or WSL) with a 32-bit-capable GNU toolchain and QEMU:

```bash
sudo apt-get install build-essential gcc-multilib nasm \
                     grub-pc-bin grub-common xorriso qemu-system-x86 python3
```

### Build everything

```bash
make            # builds the kernel ELF and the userland binaries
make iso        # produces fentoos.iso (bootable GRUB rescue image)
make disk       # produces disk.img (MINIX v3 populated with /bin, /src)
```

Build outputs land in `build/` (git-ignored). `fentoos.iso` and `disk.img`
are generated artifacts and are not tracked in the repository.

## Running

```bash
make run        # boots the ISO with disk.img attached (serial on stdio)
make run-nofs   # boots the kernel only, without the disk image
```

Or use the scripted launcher to pipe commands into the shell:

```bash
./qrun.sh 'ls /bin\npwd\n' 25
./test_full.sh
```

QEMU is launched with `-serial stdio`, so all console output is visible in
your terminal and the shell reads input from the serial line.

## Boot sequence

`kernel_main` (in `kernel/main.c`) brings the system up in this order:

1. Serial + VGA console.
2. `hal_cpu_init` — GDT + TSS.
3. `hal_interrupts_init` — IDT + PIC, keyboard, serial input.
4. `pmm_init` — physical frame allocator from the Multiboot memory map.
5. `hal_paging_init` — enable paging.
6. `vmm_init` — kernel heap.
7. `syscall_init` — install the `int 0x90` handler.
8. `ata_init` — probe the IDE/ATA disk.
9. `sched_init` + `hal_timer_init(100)` — scheduler and 100 Hz PIT.
10. `minixfs_mount` — mount the MINIX v3 root filesystem.
11. `proc_start_init("/bin/sh")` — spawn the userland shell in ring 3.
12. `sched_start` — enter the scheduler; the system is live.

## System call interface

FentoOS uses a BSD-style syscall convention: the syscall number is passed in
`eax`, arguments in registers, invoked through software interrupt `int 0x90`.
Numbers are defined in `include/syscall.h` (kernel) and
`user/include/syscall_nr.h` (userland). Implemented calls include:

`exit`, `fork`, `read`, `write`, `open`, `close`, `wait4`, `creat`, `link`,
`unlink`, `chdir`, `mknod`, `chmod`, `lseek`, `getpid`, `getppid`, `getuid`,
`getgid`, `setuid`, `setgid`, `kill`, `dup`, `dup2`, `pipe`, `ioctl`,
`execve`, `mkdir`, `rmdir`, `fstat`, `stat`, `getdents`, `getcwd`, `yield`,
`sync`, `brk`, `wait`.

## Userland

The userland ships a freestanding C library (`flibc`) and the following
programs in `/bin`:

- **Shell**: `sh` (pipelines, redirection, environment, script execution).
- **Coreutils**: `cat`, `clear`, `cp`, `echo`, `env`, `false`, `head`, `ls`,
  `mkdir`, `pwd`, `rm`, `rmdir`, `sleep`, `stat`, `touch`, `true`, `wc`.
- **Toolchain**: `fasm` (assembler), `fcc` (C compiler), `fento` (launcher),
  `basic` (BASIC interpreter).

Example sources live in `user/examples/` (`hello.c`, `hello.s`, `demo.bas`)
with a `build.sh` that exercises the on-device toolchain.

## Documentation

See the [`docs/`](docs/) directory for deeper design notes:

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — subsystem-by-subsystem design.
- [`docs/BUILDING.md`](docs/BUILDING.md) — build and toolchain details.
- [`docs/SYSCALLS.md`](docs/SYSCALLS.md) — the full syscall reference.
- [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) — how to contribute.

## License

FentoOS is distributed under the **BSD 2-Clause License**. See
[`LICENSE`](LICENSE) for the full text.
