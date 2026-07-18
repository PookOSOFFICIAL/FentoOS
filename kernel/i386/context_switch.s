.global context_switch_asm
.type context_switch_asm, @function
context_switch_asm:
    movl 4(%esp), %eax
    movl 8(%esp), %edx

    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    movl %esp, (%eax)
    movl %edx, %esp

    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

.global context_trampoline
.type context_trampoline, @function
context_trampoline:
    sti
    popl %eax
    ret
