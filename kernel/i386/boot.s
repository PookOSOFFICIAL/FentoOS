.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot
.align 4
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    movl $stack_top, %esp
    andl $-16, %esp

    pushl %ebx
    pushl %eax

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

.size _start, . - _start
