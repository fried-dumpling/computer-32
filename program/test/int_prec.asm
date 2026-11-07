; =====================================================================
;  int_prec.asm : interrupt entry from a loop with a store and a branch
;  run : python tools/dsim/runint.py prec
;
;  the runner pulses INT_in at every phase of the loop (two loop periods,
;  2 halves apart) and checks each run:
;    - the handler runs to the end ('i')
;    - the printed dots, gen[5] and the return PC describe one commit point
;    - the return PC is in mem[ISTACK-4], istack = ISTACK-4, mem[ISTACK] kept
;    - int-level in the handler = {mask 1, level 1} = 0x11
;
;  output : "L<loop> S....I<gen5> <istack> <m-4> <m> <level> i"
;           "L<loop> S....(200 dots)E" without an interrupt
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; address 8 = INT_vec
    PUTC('I')
    mov gen[1].32B, gen[5].32B
    PHEX()
    DUMPINT()
    PUTC('i')
    jmp.one pc.32B, 0
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
    cmpi gen[5].32B, 200
    ijmp.zero zero.32B, loop
    PUTC('E')
    jmp.one pc.32B, 0
