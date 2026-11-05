; =====================================================================
;  knownbug.asm : the known circuit problems in CLAUDE.md (expected fail)
;
;  build (in program/) :  dasm test/knownbug.asm -o knownbug.o
;                         dlnk knownbug.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh knownbug  (DIFF until the bugs are fixed)
;
;  output : "<case> PASS" or "<case> FAIL <word read hex>"
;    k1 : st .8L  of 0xAA   into 0x11223344, expect 0x112233AA
;    k2 : st .16L of 0xBEEF into 0x11223344, expect 0x1122BEEF
;    k3 : st .8H  of 0xCC00 into 0x11223344, expect 0x112233CC
;         (the emulator writes bits 15..8 to the addressed byte)
;    (circuit: load-buffer forwarding ignores the width, so the ld right
;     after the store gets the zero-extended value 000000AA / 0000BEEF /
;     000000CC. memory is right: with a delay before the ld, all pass)
;    h  : loop of 3 rounds printing '1' '2' '3', then halt right after the
;         loop branch. the branch is predicted not taken the first time,
;         and the circuit runs halt before the branch resolves, so it
;         stops after "h 1". expected "h 123".
;  this is the only test that ends with halt (it is part of the test).
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12

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

; STW(name) : X = 0x11223344. then the partial store, then STR(expected)
#macro STW 1
    PUTC('k')
    PUTC(#1)
    LSET(gen[2], 0x11223344)
    st.zero gen[2].32B, slot
#end

; STR(expected) : ld X, PASS if it is expected
#macro STR 1
    ld.zero gen[1].32B, slot
    LSET(gen[2], #1)
    call zero.32B, verdict
#end

.bss
slot:   4
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('k')
    PUTC('b')
    PUTC('\n')
    set gen[3].32B, 0xAA
    STW('1')
    st.zero gen[3].8L, slot
    STR(0x112233AA)
    set gen[3].32B, 0xBEEF
    STW('2')
    st.zero gen[3].16L, slot
    STR(0x1122BEEF)
    set gen[3].32B, 0xCC00
    STW('3')
    st.zero gen[3].8H, slot
    STR(0x112233CC)

    PUTC('h')
    PUTC(' ')
    set gen[6].32B, '1'
hloop:
    st.gen[24] gen[6].32B, TERM_OUT
    addi gen[6].32B, 1
    cmpi gen[6].32B, '4'
    jmp.neg zero.32B, hloop
    halt
    jmp.one pc.32B, 0

; verdict : gen[1] = word read, gen[2] = expected
verdict:
    cmp gen[1].32B, gen[2].32B
    jmp.zero zero.32B, v_pass
    PUTC(' ')
    PUTC('F')
    PUTC('A')
    PUTC('I')
    PUTC('L')
    PUTC(' ')
    call zero.32B, puthex
    PUTC('\n')
    ret
v_pass:
    PUTC(' ')
    PUTC('P')
    PUTC('A')
    PUTC('S')
    PUTC('S')
    PUTC('\n')
    ret

puthex:
    set gen[18].32B, 0xF
    HEXD(28)
    HEXD(24)
    HEXD(20)
    HEXD(16)
    HEXD(12)
    HEXD(8)
    HEXD(4)
    HEXD(0)
    ret
