CC   := gcc
LD   := ld
AS   := gcc

CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -Wall -Wextra -Iinclude -O2 -c
ASFLAGS := -m32 -c
LDFLAGS := -m elf_i386 -T lib/user.ld -nostdlib

BUILD := ../build/user
BINOUT := $(BUILD)/bin

LIB_C_SRCS := $(wildcard lib/*.c)
LIB_S_SRCS := lib/syscall.s
LIB_OBJS := $(patsubst lib/%.c,$(BUILD)/lib/%.o,$(LIB_C_SRCS)) \
            $(patsubst lib/%.s,$(BUILD)/lib/%.o,$(LIB_S_SRCS))
CRT0 := $(BUILD)/lib/crt0.o

PROGS := $(patsubst bin/%.c,$(BINOUT)/%,$(wildcard bin/*.c))

.PHONY: all clean

all: $(PROGS)

$(BUILD)/lib/%.o: lib/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/lib/%.o: lib/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(CRT0): lib/crt0.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BINOUT)/%: bin/%.c $(CRT0) $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $(BUILD)/$*.o
	$(LD) $(LDFLAGS) -o $@ $(CRT0) $(BUILD)/$*.o $(LIB_OBJS)

clean:
	rm -rf $(BUILD)
