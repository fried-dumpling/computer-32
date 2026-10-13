#include<io.hsm>

.data
t_vals:
    4, 0
    4, 7
    4, -1
    4, 10
    4, 1000000000
    4, 2147483647
    4, -2147483647-1
    4, -123456789
    4, 0x1234ABCD
t_end:
t_str:
    4, 'h'
    4, 'i'
    4, 0
t_pr:   4,'>' 4,' ' 4,0
t_b4:   8, "abcd"                 ; packed strings (io_printb)
t_b5:   8, "Hello"
t_b3:   4, "xyz"
t_b0:   4, ""

.bss
t_buf:  4*5
t_stk:  4*64
t_stkend:

.text
    set stack.32B, t_stkend
    set gen[5].32B, t_vals
lp:
    ld.gen[5] gen[1].32B, 0
    call zero.32B, io_printint
    set gen[1].32B, ' '
    call zero.32B, io_putc
    ld.gen[5] gen[1].32B, 0
    call zero.32B, io_printhex
    call zero.32B, io_newline
    addi gen[5].32B, 4
    cmpi gen[5].32B, t_end
    ijmp.zero zero.32B, lp

    set gen[1].32B, t_str               ; "hi" + length
    call zero.32B, io_print
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    set gen[1].32B, t_str
    call zero.32B, io_println
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    call zero.32B, io_newline

    set gen[1].32B, t_b4                ; packed : 4, 5, 3, 0 chars + length
    call zero.32B, pb_len
    set gen[1].32B, t_b5
    call zero.32B, pb_len
    set gen[1].32B, t_b3
    call zero.32B, pb_len
    set gen[1].32B, t_b0
    call zero.32B, pb_len
    set gen[1].32B, t_b5
    call zero.32B, io_printlnb
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    call zero.32B, io_newline

    set gen[6].32B, 2                   ; two lines into a 5 word buffer
    set gen[7].32B, t_pr                ; first with a prompt, then none
line:
    set gen[1].32B, t_buf
    set gen[2].32B, 5
    mov gen[3].32B, gen[7].32B
    set gen[7].32B, 0
    call zero.32B, io_getline
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    set gen[1].32B, '['
    call zero.32B, io_putc
    set gen[1].32B, t_buf
    call zero.32B, io_print
    set gen[1].32B, ']'
    call zero.32B, io_putc
    call zero.32B, io_newline
    subi gen[6].32B, 1
    cmpi gen[6].32B, 0
    ijmp.zero zero.32B, line

    call zero.32B, io_getc              ; getc + echo
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_putc
    call zero.32B, io_trygetc           ; no key left : 0
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    halt

    ; io_printb gen[1], then its length and a newline
pb_len:
    call zero.32B, io_printb
    mov gen[1].32B, gen[0].32B
    call zero.32B, io_printint
    jmp.one zero.32B, io_newline
