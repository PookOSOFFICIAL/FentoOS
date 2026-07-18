# FentoOS Architecture

This document describes the design of FentoOS subsystem by subsystem. All
paths are relative to the repository root.

## Target platform

- **Architecture**: 32-bit x86 (i386), protected mode.
- **Boot protocol**: Multiboot 1, loaded by GRUB.
- **Toolchain**: `gcc -m32 -ffreestanding`, `nasm`/`gcc` for assembly,
  `ld -m elf_i386` with a custom linker script.
- **Machine model**: single processor, PC-compatible (PIC, PIT, PS/2, VGA,
  16550 UART, ATA/IDE).

## Bootstrap (`kernel/i386/boot.s`, `linker.ld`)

`boot.s` provides the Multiboot header and the initial entry point. It sets up
a temporary stack and calls `kernel_main`, passing the Multiboot magic and the
`multiboot_info` pointer. `linker.ld` places the kernel at its physical/virtual
load address and exports `kernel_end`, which the PMM uses as the start of
allocatable memory.

## Hardware abstraction layer (`include/hal.h`, `kernel/hal/`, `kernel/i386/`)

The HAL exposes an arch-neutral API used by the portable kernel:

- `hal_cpu_init` — load the GDT and TSS.
- `hal_interrupts_init` — build the IDT, remap the PIC.
- `hal_paging_init` — turn on paging.
- `hal_timer_init(hz)` — program the PIT.
- `hal_sti` / `hal_hlt` — interrupt/idle primitives.
- `struct hal_trapframe` — the saved register frame passed to trap handlers.

### GDT + TSS (`gdt.c`, `gdt_flush.s`)

A flat segmentation model: null, kernel code/data (ring 0), user code/data
(ring 3), and a TSS descriptor. The TSS holds the ring-0 stack pointer used on
privilege transitions (interrupts/syscalls from user mode).

### IDT + interrupts (`idt.c`, `idt_flush.s`, `isr_stubs.s`)

256-entry IDT. CPU exceptions (0–31) and hardware IRQs (remapped to 32–47)
have assembly stubs in `isr_stubs.s` that push a uniform trapframe and call
into C. The PIC is remapped so IRQs don't collide with CPU exception vectors.

## Physical memory manager (`kernel/mm/pmm.c`, `include/pmm.h`)

A frame allocator that manages 4 KiB physical frames. It is initialized from
the Multiboot `mem_upper` figure and reserves everything below `kernel_end`.
It exposes `pmm_init`, `pmm_alloc_frame`, `pmm_free_frame`, and the accounting
helpers `pmm_total_count` / `pmm_free_count`.

## Virtual memory manager (`kernel/mm/vmm.c`, `kernel/i386/paging.c`, `include/vmm.h`)

`paging.c` manages page directories/tables and the `cr3` switch. `vmm.c` builds
the kernel address space, provides map/unmap primitives, per-process address
spaces, and a kernel heap allocator (`kmalloc`/`kfree`) carved out of mapped
virtual memory. Each user process gets its own page directory sharing the
kernel's higher-half mappings.

## Scheduler (`kernel/sched/sched.c`, `include/sched.h`)

A preemptive round-robin scheduler over a `struct task` run queue. The PIT tick
(100 Hz) drives preemption; `sched_yield`/`SYS_yield` allow cooperative
switching. `context_switch.s` and `context.c` perform the low-level register
and stack switch between tasks; `proc_asm.s` handles the transition into ring 3.

## Processes (`kernel/proc/proc.c`, `kernel/i386/elf.c`, `include/proc.h`)

Process creation and lifecycle:

- `proc_exec` / `sys_execve_impl` — load an ELF from the filesystem (`elf.c`),
  set up the user address space, stack, and `argv`/`envp` at the top of the
  user stack region, then enter ring 3.
- `sys_fork_impl` — duplicate the calling process's address space and task
  state.
- `sys_wait4_impl` — reap children and collect exit status.
- `proc_start_init` — spawn the first userland process (`/bin/sh`).

Layout constants (`include/proc.h`): user image loads at `0x40000000`, user
stack top at `0xC0000000`.

## System calls (`kernel/syscall/syscall.c`, `kernel/i386/syscall_entry.c`)

Userland issues `int 0x90` with the syscall number in `eax`. `syscall_entry.c`
saves the trapframe and dispatches through the table in `syscall.c`. Numbers
follow the classic BSD numbering (see `docs/SYSCALLS.md`). `ioctl` supports TTY
control (`TTY_SET_RAW`, `TTY_SET_ECHO`, `TTY_SET_FOREGROUND`).

## Drivers (`kernel/drivers/`)

- **vga.c** — 80x25 VGA text console with color attributes and `console_printf`.
- **serial.c** — 16550 UART on COM1; used as both log output and interactive
  input (`serial_enable_input`).
- **keyboard.c** — PS/2 keyboard IRQ handler and scancode translation.
- **ata.c** — ATA PIO driver providing sector read/write and `ata_sector_count`.

## Filesystem (`kernel/fs/minixfs.c`, `kernel/fs/file.c`)

`minixfs.c` implements a read/write MINIX v3 filesystem on top of the ATA
driver: superblock parsing, inode and zone handling, directory operations, and
path resolution. `file.c` is a small VFS-style layer providing the file
descriptor table, open files, and the plumbing behind `open`/`read`/`write`/
`close`/`lseek`/`getdents`/`stat`. The root filesystem is built by
`tools/mkminix3.py` and mounted at boot.

## Userland C library (`user/lib/`, `user/include/`)

`flibc` is a freestanding libc:

- `crt0.s` — process entry stub that calls `main` and then `exit`.
- `syscall.s` — the `int 0x90` trampoline.
- `unistd.c` — thin syscall wrappers.
- `stdio.c` — buffered I/O and formatted printing.
- `malloc.c` — heap allocator over `brk`.
- `string.c` — string/memory helpers.

Programs link against `crt0.o` + the library objects using `user/lib/user.ld`.

## Disk image tool (`tools/mkminix3.py`)

A host-side Python script that formats a MINIX v3 image and copies the built
userland binaries (and example sources) into it, producing the `disk.img` that
QEMU attaches as the IDE drive.
