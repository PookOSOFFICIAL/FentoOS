.section .text

.global syscall0
.type syscall0, @function
syscall0:
    movl 4(%esp), %eax
    int $0x90
    ret

.global syscall1
.type syscall1, @function
syscall1:
    pushl %ebx
    movl 8(%esp), %eax
    movl 12(%esp), %ebx
    int $0x90
    popl %ebx
    ret

.global syscall2
.type syscall2, @function
syscall2:
    pushl %ebx
    movl 8(%esp), %eax
    movl 12(%esp), %ebx
    movl 16(%esp), %ecx
    int $0x90
    popl %ebx
    ret

.global syscall3
.type syscall3, @function
syscall3:
    pushl %ebx
    movl 8(%esp), %eax
    movl 12(%esp), %ebx
    movl 16(%esp), %ecx
    movl 20(%esp), %edx
    int $0x90
    popl %ebx
    ret
