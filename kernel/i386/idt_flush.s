.global idt_flush
.type idt_flush, @function
idt_flush:
    movl 4(%esp), %eax
    lidt (%eax)
    ret
