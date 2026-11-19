; =====================================================================
;  int_misp.asm : interrupt around branch mispredictions
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
    cmpi gen[8].32B, 0                  ; +12
    jmp.zero zero.32B, even             ; +16
    addi gen[6].32B, 1                  ; +20 odd
    jmp.one zero.32B, join              ; +24
even:
    addi gen[7].32B, 1                  ; +28 even
join:
    cmpi gen[5].32B, 400                ; +32
    ijmp.zero zero.32B, loop            ; +36
    PUTC('E')
    jmp.one pc.32B, 0
