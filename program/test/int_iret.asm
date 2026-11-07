; =====================================================================
;  int_iret.asm : iret returns and the program goes on
;  run : python tools/dsim/runint.py iret
;
;  the handler leaves gen[1], gen[5], gen[18], gen[20] and the flags alone, so
;  the loop must finish as if nothing happened: 60 dots in all and gen[5] = 60.
;  ISA.md 2.2: iret restores istack (= ISTACK) and int-level (mask 0, level 0).
;
;  output : "L<loop> S..I i..E<gen5> <istack> <int-level> e"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; address 8 = INT_vec
    HPUTC('I')
    HPUTC('i')
    iret
main:
    INIT()
    INITIST()
    PUTC('L')
    set gen[1].32B, loop
    PHEX()
    PUTC('S')
    set gen[5].32B, 0
loop:
    addi gen[5].32B, 1
    set gen[20].32B, '.'
    st.gen[24] gen[20].32B, TERM_OUT
    cmpi gen[5].32B, 60
    ijmp.zero zero.32B, loop
    PUTC('E')
    mov gen[1].32B, gen[5].32B
    PHEX()
    ldexp gen[1].32B, istack
    PHEX()
    ldexp gen[1].32B, intlevel
    PHEX()
    PUTC('e')
    jmp.one pc.32B, 0
