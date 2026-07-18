# Building FentoOS

## Host requirements

FentoOS must be built on a Linux host (native or WSL) with a 32-bit capable
GNU toolchain. On a Debian/Ubuntu system:

```bash
sudo apt-get update
sudo apt-get install \
    build-essential \
    gcc-multilib \
    nasm \
    ld \
    grub-pc-bin \
    grub-common \
    xorriso \
    mtools \
    qemu-system-x86 \
    python3
```

The important pieces:

- `gcc` with `-m32` (provided by `gcc-multilib`) to emit 32-bit freestanding code.
- `nasm` (and `gcc` as an assembler) for the `.s` sources.
- `ld` targeting `elf_i386`.
- `grub-mkrescue` + `xorriso` to build the bootable ISO.
- `qemu-system-i386` to run it.
- `python3` for the disk-image builder.

## Compiler flags

From the top-level `Makefile`:

```
CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -Iinclude -O2 -c
ASFLAGS := -m32 -c
LDFLAGS := -m elf_i386 -T kernel/i386/linker.ld -nostdlib
```

The userland `Makefile` uses the same freestanding flags but links against
`user/lib/user.ld`.

## Make targets

| Target        | Description                                                        |
|---------------|--------------------------------------------------------------------|
| `make`        | Build the kernel ELF (`build/fentoos.elf`) and all userland binaries |
| `make userland` | Build only the userland (`build/user/bin/*`)                     |
| `make iso`    | Build `fentoos.iso` (GRUB rescue image containing the kernel)      |
| `make disk`   | Build `disk.img` (MINIX v3 image populated by `tools/mkminix3.py`) |
| `make run`    | Build ISO + disk and boot under QEMU with the disk attached        |
| `make run-nofs` | Boot the kernel only, without a disk image                       |
| `make clean`  | Remove `build/`, the ISO, the disk image, and the staged ELF       |

## Build outputs

All compiled objects and binaries go under `build/` (git-ignored):

```
build/fentoos.elf              Linked kernel
build/kernel/**/*.o            Kernel objects
build/user/bin/<prog>          Userland executables
build/user/lib/*.o             flibc objects
```

`fentoos.iso` and `disk.img` are produced in the repository root and are also
git-ignored, since they are fully regenerable from source.

## Running under QEMU

```bash
make run
```

This is equivalent to:

```bash
qemu-system-i386 -cdrom fentoos.iso -drive file=disk.img,format=raw,if=ide \
    -boot d -serial stdio -no-reboot -m 64
```

Console output is routed to the serial port (`-serial stdio`), and the shell
reads from serial input, so the whole session runs in your terminal.

### Scripted runs

`qrun.sh` boots the system and pipes a command string into the shell after a
short delay:

```bash
./qrun.sh 'ls /bin\npwd\ncat /src/hello.c\n' 25
```

The first argument is the command stream (`\n` separated), the second is the
timeout in seconds.

`test_full.sh` runs a fixed end-to-end smoke test (pipelines, the on-device
toolchain build script, and I/O redirection) and prints the captured output.
