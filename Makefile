CC      := gcc
LD      := ld
AS      := gcc
NASM    := nasm

CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -Iinclude -O2 -c
ASFLAGS := -m32 -c
LDFLAGS := -m elf_i386 -T kernel/i386/linker.ld -nostdlib

BUILD   := build
KERNEL  := $(BUILD)/fentoos.elf
ISO     := fentoos.iso
DISK    := disk.img

C_SRCS := $(shell find kernel -name '*.c')
S_SRCS := $(shell find kernel -name '*.s')

C_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))
S_OBJS := $(patsubst %.s,$(BUILD)/%.o,$(S_SRCS))
OBJS   := $(S_OBJS) $(C_OBJS)

.PHONY: all clean iso run disk userland

all: $(KERNEL) userland

userland:
	$(MAKE) -C user all

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(KERNEL)
	@mkdir -p iso/boot
	cp $(KERNEL) iso/boot/fentoos.elf
	grub-mkrescue -o $(ISO) iso 2>/dev/null

disk: userland
	python3 tools/mkminix3.py $(DISK) $(BUILD)/user/bin

run: iso disk
	qemu-system-i386 -cdrom $(ISO) -drive file=$(DISK),format=raw,if=ide \
		-boot d -serial stdio -no-reboot -m 64

run-nofs: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -serial stdio -no-reboot -m 64

clean:
	$(MAKE) -C user clean
	rm -rf $(BUILD) iso/boot/fentoos.elf $(ISO) $(DISK)
