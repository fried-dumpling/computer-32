; =====================================================================
;  int_iret2.asm : a second IRQ at every phase around the first handler's iret
;
;  same loop as int_iret. the runner pulses IRQ once in the loop, then a
;  second time at every phase from the handler entry to well after its iret
;  (also while the iret is in flight). the second IRQ is held while the
;  handler runs (mask 1) and must enter once after iret, with a return PC
;  in main. both entries log the return PC.
;
;  output : "L<loop> <main> S..I i..I i..E<gen5> <istack> <int-level> G<R1> <R2> g"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; int_vec[IRQ], address 8
    HPUTC('I')
    LOGRET()
    HPUTC('i')
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    INTVEC()
    PUTC('L')
    PADDR(loop)
    PADDR(main)
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
    DUMPLOG()
    jmp.one pc.32B, 0
