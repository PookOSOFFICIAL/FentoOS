.section .text
.global _start
.type _start, @function
_start:
    movl 4(%esp), %eax
    movl 8(%esp), %ecx
    movl 12(%esp), %edx

    pushl %edx
    pushl %ecx
    pushl %eax
    call main
    addl $12, %esp

    pushl %eax
    call _exit
1:
    jmp 1b
