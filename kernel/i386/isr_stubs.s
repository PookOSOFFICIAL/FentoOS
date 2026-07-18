.macro ISR_NOERR num
.global isr\num
isr\num:
    cli
    pushl $0
    pushl $\num
    jmp isr_common
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    cli
    pushl $\num
    jmp isr_common
.endm

.macro IRQ num, remap
.global irq\num
irq\num:
    cli
    pushl $0
    pushl $\remap
    jmp irq_common
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_ERR   30
ISR_NOERR 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

.global isr144
isr144:
    cli
    pushl $0
    pushl $144
    jmp isr_common

.extern isr_dispatch
.extern irq_dispatch

isr_common:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp
    call isr_dispatch
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret

irq_common:
    pusha
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    pushl %esp
    call irq_dispatch
    addl $4, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popa
    addl $8, %esp
    iret
