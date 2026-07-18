# Contributing to FentoOS

Thanks for your interest in FentoOS. This is a hobby operating system; the
guidelines below keep the tree consistent and buildable.

## Getting set up

1. Install the host toolchain described in [`BUILDING.md`](BUILDING.md).
2. Build the tree: `make`.
3. Boot it: `make run`.
4. Run the smoke test: `./test_full.sh`.

## Coding conventions

- **Language**: freestanding C (compiled with `-ffreestanding -nostdlib`) for
  the portable kernel and userland, plus x86 assembly (`.s`) for the low-level
  arch code.
- **No hosted libc**: the kernel and userland do not link against the host C
  library. Use the in-tree `flibc` (userland) or `kernel/lib/string.c` (kernel).
- **Headers**: kernel-facing declarations go in `include/`; userland-facing
  declarations go in `user/include/`.
- **Portability boundary**: keep architecture-specific code under
  `kernel/i386/` and behind the HAL API in `include/hal.h`. Portable subsystems
  (`mm`, `sched`, `proc`, `fs`, `syscall`) should not hardcode arch details.
- **Warnings**: the build uses `-Wall -Wextra`. Keep new code warning-clean.

## Adding a system call

1. Assign a number in both `include/syscall.h` and
   `user/include/syscall_nr.h` (keep them identical).
2. Implement the handler and register it in the dispatch table in
   `kernel/syscall/syscall.c`.
3. Add a userland wrapper in `user/lib/unistd.c` (and a prototype in
   `user/include/libc.h`).
4. Document it in [`SYSCALLS.md`](SYSCALLS.md).

## Adding a userland program

1. Create `user/bin/<name>.c`.
2. It is picked up automatically by `user/Makefile` (which globs `bin/*.c`).
3. Rebuild with `make userland` and rebuild the disk image with `make disk`.

## Submitting changes

- Keep commits focused and buildable — `make` should succeed at each commit.
- Describe *what* changed and *why* in the commit message.
- Verify the system still boots to a shell (`make run`) and that
  `./test_full.sh` passes before opening a pull request.

## License

By contributing you agree that your contributions are licensed under the
project's BSD 2-Clause License (see [`../LICENSE`](../LICENSE)).
