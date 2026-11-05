; =====================================================================
;  rotate.asm : rol / roli / ror / rori on the circuit
;
;  build (in program/) :  dasm test/rotate.asm -o rotate.o
;                         dlnk rotate.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh rotate
;
;  output : "<case> <result hex>", X = 0x12345678
;    a0..a5 : roli X, 0 / 1 / 4 / 8 / 31 / 32
;    b0..b5 : rori X, 0 / 1 / 4 / 8 / 31 / 32
;    c0..c3 : rol  X, R  with R = 4 / 31 / 32 / 36
;    d0..d3 : ror  X, R  with R = 4 / 31 / 32 / 36
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12
#define X 0x12345678

#macro LSET 2
    set #1.S16H, ((#2) >> 16) & 0xFFFF
    set #1.16L, (#2) & 0xFFFF
#end

#macro PUTC 1
    set gen[20].32B, #1
    st.gen[24] gen[20].32B, TERM_OUT
#end

#macro HEXD 1
    mov gen[20].32B, gen[1].32B
    shiftri gen[20].32B, #1
    bnd gen[20].32B, gen[18].32B
    cmpi gen[20].32B, 10
    jmp.neg zero.32B, ##num
    addi gen[20].32B, 'A'-10-'0'
##num:
    addi gen[20].32B, '0'
    st.gen[24] gen[20].32B, TERM_OUT
#end

; RI(case, op, imm) : gen[1] = X op imm, print it
#macro RI 4
    PUTC(#1)
    PUTC(#2)
    LSET(gen[1], X)
    #3 gen[1].32B, #4
    call zero.32B, show
#end

; RR(case, op, amount) : gen[1] = X op gen[2] (gen[2] = amount), print it
#macro RR 4
    PUTC(#1)
    PUTC(#2)
    LSET(gen[1], X)
    set gen[2].32B, #4
    #3 gen[1].32B, gen[2].32B
    call zero.32B, show
#end

.bss
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    RI('a', '0', roli, 0)
    RI('a', '1', roli, 1)
    RI('a', '2', roli, 4)
    RI('a', '3', roli, 8)
    RI('a', '4', roli, 31)
    RI('a', '5', roli, 32)
    RI('b', '0', rori, 0)
    RI('b', '1', rori, 1)
    RI('b', '2', rori, 4)
    RI('b', '3', rori, 8)
    RI('b', '4', rori, 31)
    RI('b', '5', rori, 32)
    RR('c', '0', rol, 4)
    RR('c', '1', rol, 31)
    RR('c', '2', rol, 32)
    RR('c', '3', rol, 36)
    RR('d', '0', ror, 4)
    RR('d', '1', ror, 31)
    RR('d', '2', ror, 32)
    RR('d', '3', ror, 36)
    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
    jmp.one pc.32B, 0

; show : print " <gen[1] hex>\n"
show:
    PUTC(' ')
    set gen[18].32B, 0xF
    HEXD(28)
    HEXD(24)
    HEXD(20)
    HEXD(16)
    HEXD(12)
    HEXD(8)
    HEXD(4)
    HEXD(0)
    PUTC('\n')
    ret
