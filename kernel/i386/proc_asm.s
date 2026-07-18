.global proc_fork_return
.type proc_fork_return, @function
proc_fork_return:
    addl $4, %esp
    call proc_fork_return_c
    hlt
