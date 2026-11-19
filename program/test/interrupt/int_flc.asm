; =====================================================================
;  int_flc.asm : like int_misp, the branch resolves late (st + ld)
;
;  the condition goes through memory (st T ; ld), so the branch resolves
;  late. this is meant to make an interrupt flush (Fl_INT) and a mispredict
;  flush (Fl_JPU) fall in the same half (item 11), which int_misp never hit.
;  layout (offsets from loop) differs from int_misp: see flc_states().
;
;  the jmp.zero alternates taken / not taken, so the 2-bit predictor keeps
;  missing. each iteration adds to gen[6] or gen[7], never both. the runner
;  pulses IRQ at every phase of two iterations and checks that the return
;  PC and gen[5..7] are one instruction boundary of the correct path. a
;  return PC on the wrong path, or wrong-path results in the registers, fail.
;  it also reports the runs where Fl_INT and Fl_JPU are high in the same half.
;
;  output : "L<loop> SI<gen5> <gen6> <gen7> <istack> <m-4> <m> <level> i"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; int_vec[IRQ], address 8
    PUTC('I')
    mov gen[1].32B, gen[5].32B
    PHEX()
    mov gen[1].32B, gen[6].32B
    PHEX()
    mov gen[1].32B, gen[7].32B
    PHEX()
    DUMPINT()
    PUTC('i')
    jmp.one pc.32B, 0
main:
    INIT()
    INITIST()
    INTVEC()
    PUTC('L')
    set gen[1].32B, loop
    PHEX()
    PUTC('S')
    set gen[5].32B, 0
    set gen[6].32B, 0
    set gen[7].32B, 0
loop:
    addi gen[5].32B, 1                  ; +0
    mov gen[8].32B, gen[5].32B          ; +4
    bndi gen[8].32B, 1                  ; +8
    st.zero gen[8].32B, 0x2010          ; +12
    ld.zero gen[9].32B, 0x2010          ; +16
    cmpi gen[9].32B, 0                  ; +20
    jmp.zero zero.32B, even             ; +24
    addi gen[6].32B, 1                  ; +28 odd
    jmp.one zero.32B, join              ; +32
even:
    addi gen[7].32B, 1                  ; +36 even
join:
    cmpi gen[5].32B, 150                ; +40 (int_misp: 400, too slow here)
    ijmp.zero zero.32B, loop            ; +44
    PUTC('E')
    jmp.one pc.32B, 0
