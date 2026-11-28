; =====================================================================
;  carry-loopshow.asm : carry.asm with a looping hex printer (show).
;    reproduces document/errors.md #60: the circuit stalls at PC=0x214
;    on case a2 (2nd digit); the emulator runs to the end.
;    dsim: tools/dsim/runtests.sh options, -Drom=<this .bin>
;
;  build (in program/) :  dasm test/carry-loopshow.asm -o carry-loopshow.o
;                         dlnk carry-loopshow.o -o ../boot/rom.bin
;
;  output : "<case> <result hex...> <flags>"
;    flags : Z N P C 4 V (zero, neg, pos, carry, carry4, overflow),
;            '.' when clear. Arithmetic cases print all six, shifts and
;            rotates only Z N P C (the others are adder side effects,
;            see tools/emu/isa32emu.py)
;    a* : add / addi        b* : addc with carry in 0 / 1
;    c* : sub / subi        d* : subc with borrow in 0 / 1
;    e* : cmp / cmpi
;    f* : 64 / 96 bit add chains (hi .. lo)
;    g* : 64 bit sub chains (hi lo)
;    h* : shiftl / shiftr / shiftli / shiftri carry out
;    i* : shiftlc / shiftrc carry in (by 1, 4, 0)
;    j* : 64 bit shifts by 1 (hi lo)
;    k* : rol / ror carry
;    m* : carry kept over mov / set / ld / st / push / pop, sub -> addc
;  carry in is set with CIN(v) = "cmpi 0, v" (v = 1 borrows).
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

; carry = v (0 or 1); zero/neg/pos/carry4/overflow change too
#macro CIN 1
    set gen[22].32B, 0
    cmpi gen[22].32B, #1
#end

; FB(flag, char) : print char if flag is set, '.' otherwise (no flag change)
#macro FB 2
    set gen[21].32B, #2
    jmp.#1 zero.32B, ##t
    set gen[21].32B, '.'
##t:
    st.gen[24] gen[21].32B, TERM_OUT
#end

#macro FALL 0
    PUTC(' ')
    FB(zero, 'Z')
    FB(neg, 'N')
    FB(pos, 'P')
    FB(carry, 'C')
    FB(carry4, '4')
    FB(overflow, 'V')
#end

#macro FSH 0
    PUTC(' ')
    FB(zero, 'Z')
    FB(neg, 'N')
    FB(pos, 'P')
    FB(carry, 'C')
#end

#macro HEX 1
    mov gen[17].32B, #1
    call zero.32B, show
#end

#macro NAME 2
    PUTC(#1)
    PUTC(#2)
#end

; A2(c1, c2, op, A, B) : gen[1] = A op B, all flags
#macro A2 5
    NAME(#1, #2)
    LSET(gen[1], #4)
    LSET(gen[2], #5)
    #3 gen[1].32B, gen[2].32B
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')
#end

; AI(c1, c2, op, A, imm)
#macro AI 5
    NAME(#1, #2)
    LSET(gen[1], #4)
    #3 gen[1].32B, #5
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')
#end

; AC(c1, c2, cin, op, A, B) : like A2 with carry in
#macro AC 6
    NAME(#1, #2)
    LSET(gen[1], #5)
    LSET(gen[2], #6)
    CIN(#3)
    #4 gen[1].32B, gen[2].32B
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')
#end

; SC(c1, c2, cin, op, A, amount) : shift / rotate by a register
#macro SC 6
    NAME(#1, #2)
    LSET(gen[1], #5)
    set gen[2].32B, #6
    CIN(#3)
    #4 gen[1].32B, gen[2].32B
    FSH()
    HEX(gen[1].32B)
    PUTC('\n')
#end

; SI(c1, c2, cin, op, A, imm) : shift / rotate by an immediate
#macro SI 6
    NAME(#1, #2)
    LSET(gen[1], #5)
    CIN(#3)
    #4 gen[1].32B, #6
    FSH()
    HEX(gen[1].32B)
    PUTC('\n')
#end

; W64(c1, c2, op, opc, AH, AL, BH, BL) : (gen[3]:gen[1]) op= (gen[4]:gen[2])
#macro W64 8
    NAME(#1, #2)
    LSET(gen[1], #6)
    LSET(gen[2], #8)
    LSET(gen[3], #5)
    LSET(gen[4], #7)
    #3 gen[1].32B, gen[2].32B
    #4 gen[3].32B, gen[4].32B
    FALL()
    HEX(gen[3].32B)
    HEX(gen[1].32B)
    PUTC('\n')
#end

; A96(c1, c2, A2, A1, A0, B2, B1, B0) : 96 bit add
#macro A96 8
    NAME(#1, #2)
    LSET(gen[1], #5)
    LSET(gen[2], #8)
    LSET(gen[3], #4)
    LSET(gen[4], #7)
    LSET(gen[5], #3)
    LSET(gen[6], #6)
    add gen[1].32B, gen[2].32B
    addc gen[3].32B, gen[4].32B
    addc gen[5].32B, gen[6].32B
    FALL()
    HEX(gen[5].32B)
    HEX(gen[3].32B)
    HEX(gen[1].32B)
    PUTC('\n')
#end

; SHL64 / SHR64(c1, c2, H, L) : 64 bit shift by 1 through carry (gen[3]:gen[1])
#macro SHL64 4
    NAME(#1, #2)
    LSET(gen[1], #4)
    LSET(gen[3], #3)
    shiftl gen[1].32B, one.32B
    shiftlc gen[3].32B, one.32B
    FSH()
    HEX(gen[3].32B)
    HEX(gen[1].32B)
    PUTC('\n')
#end

#macro SHR64 4
    NAME(#1, #2)
    LSET(gen[1], #4)
    LSET(gen[3], #3)
    shiftr gen[3].32B, one.32B
    shiftrc gen[1].32B, one.32B
    FSH()
    HEX(gen[3].32B)
    HEX(gen[1].32B)
    PUTC('\n')
#end

.data
cell:   4, 0

.bss
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)

    A2('a','0', add, 0xFFFFFFFF, 1)
    A2('a','1', add, 1, 2)
    A2('a','2', add, 0x7FFFFFFF, 1)
    A2('a','3', add, 0x0000000F, 1)
    A2('a','4', add, 0x80000000, 0x80000000)
    A2('a','5', add, 0xFFFFFFFF, 0xFFFFFFFF)
    A2('a','6', add, 0, 0)
    AI('a','7', addi, 0xFFFFFFFF, 1)
    AI('a','8', addi, 0x7FFFFFFF, 0xFFFF)

    AC('b','0', 0, addc, 0xFFFFFFFF, 0)
    AC('b','1', 1, addc, 0xFFFFFFFF, 0)
    AC('b','2', 0, addc, 1, 2)
    AC('b','3', 1, addc, 1, 2)
    AC('b','4', 1, addc, 0xFFFFFFFF, 0xFFFFFFFF)
    AC('b','5', 1, addc, 0x7FFFFFFF, 0)
    AC('b','6', 1, addc, 0x0000000F, 0)
    AC('b','7', 1, addc, 0xFFFFFFFE, 1)
    AC('b','8', 0, addc, 0, 0)

    A2('c','0', sub, 0, 1)
    A2('c','1', sub, 1, 1)
    A2('c','2', sub, 2, 1)
    A2('c','3', sub, 0x80000000, 1)
    A2('c','4', sub, 0x7FFFFFFF, 0xFFFFFFFF)
    A2('c','5', sub, 0x00000010, 1)
    A2('c','6', sub, 0xFFFFFFFF, 0xFFFFFFFF)
    AI('c','7', subi, 0, 1)
    AI('c','8', subi, 2, 1)

    AC('d','0', 0, subc, 0, 0)
    AC('d','1', 1, subc, 0, 0)
    AC('d','2', 0, subc, 5, 3)
    AC('d','3', 1, subc, 5, 3)
    AC('d','4', 0, subc, 3, 3)
    AC('d','5', 1, subc, 3, 3)
    AC('d','6', 1, subc, 0, 0xFFFFFFFF)
    AC('d','7', 1, subc, 0x80000000, 0)
    AC('d','8', 1, subc, 0x00000010, 0)

    A2('e','0', cmp, 0, 1)
    A2('e','1', cmp, 2, 1)
    A2('e','2', cmp, 1, 1)
    A2('e','3', cmp, 0x80000000, 1)
    A2('e','4', cmp, 0xFFFFFFFF, 0)
    AI('e','5', cmpi, 0, 1)
    AI('e','6', cmpi, 0x8000, 0xFFFF)

    W64('f','0', add, addc, 0x00000001, 0xFFFFFFFF, 0, 1)
    W64('f','1', add, addc, 0xFFFFFFFF, 0xFFFFFFFF, 0, 1)
    W64('f','2', add, addc, 0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321)
    W64('f','3', add, addc, 0x7FFFFFFF, 0xFFFFFFFF, 0, 1)
    W64('f','4', add, addc, 0x00000000, 0x80000000, 0, 0x7FFFFFFF)
    A96('f','5', 0, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 1)
    A96('f','6', 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0, 0, 1)

    W64('g','0', sub, subc, 0x00000001, 0x00000000, 0, 1)
    W64('g','1', sub, subc, 0x00000000, 0x00000000, 0, 1)
    W64('g','2', sub, subc, 0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321)
    W64('g','3', sub, subc, 0x80000000, 0x00000000, 0, 1)
    W64('g','4', sub, subc, 0x00000005, 0x00000003, 0x00000005, 0x00000003)

    SC('h','0', 0, shiftl, 0x80000001, 1)
    SC('h','1', 1, shiftl, 0x40000001, 1)
    SC('h','2', 0, shiftl, 0x10000000, 4)
    SC('h','3', 0, shiftl, 0x80000000, 4)
    SC('h','4', 0, shiftl, 0x01000000, 4)
    SC('h','5', 1, shiftl, 0x80000000, 0)
    SC('h','6', 0, shiftl, 0x00000002, 31)
    SC('h','7', 0, shiftr, 0x80000001, 1)
    SC('h','8', 0, shiftr, 0x00000008, 4)
    SC('h','9', 0, shiftr, 0x00000001, 4)
    SC('H','0', 0, shiftr, 0x00000010, 4)
    SC('H','1', 1, shiftr, 0x00000001, 0)
    SI('H','2', 0, shiftli, 0x80000001, 1)
    SI('H','3', 0, shiftri, 0x00000003, 2)

    SC('i','0', 1, shiftlc, 0x80000001, 1)
    SC('i','1', 0, shiftlc, 0x80000001, 1)
    SC('i','2', 1, shiftrc, 0x80000001, 1)
    SC('i','3', 0, shiftrc, 0x80000001, 1)
    SC('i','4', 1, shiftlc, 0x00000010, 4)
    SC('i','5', 1, shiftrc, 0x00000010, 4)
    SC('i','6', 1, shiftlc, 0x80000000, 0)
    SC('i','7', 1, shiftrc, 0x00000001, 0)
    SC('i','8', 1, shiftlc, 0, 31)
    SC('i','9', 0, shiftlc, 0xFFFFFFFF, 31)

    SHL64('j','0', 0x00000000, 0x80000000)
    SHL64('j','1', 0x80000000, 0x00000001)
    SHL64('j','2', 0x12345678, 0x9ABCDEF0)
    SHR64('j','3', 0x00000001, 0x00000000)
    SHR64('j','4', 0x00000000, 0x00000001)
    SHR64('j','5', 0x12345678, 0x9ABCDEF0)

    SC('k','0', 0, rol, 0x80000001, 1)
    SC('k','1', 0, rol, 0x10000000, 4)
    SC('k','2', 0, rol, 0x00000001, 4)
    SC('k','3', 1, rol, 0x80000000, 0)
    SC('k','4', 0, ror, 0x00000001, 1)
    SC('k','5', 0, ror, 0x00000008, 4)
    SC('k','6', 0, ror, 0x80000000, 4)
    SI('k','7', 0, roli, 0x01000000, 8)
    SI('k','8', 0, rori, 0x00000100, 8)

    ; m0 : add sets carry, mov / set / st / ld / push / pop keep it, addc uses it
    NAME('m','0')
    LSET(gen[1], 0xFFFFFFFF)
    add gen[1].32B, one.32B
    mov gen[5].32B, gen[1].32B
    set gen[6].32B, 7
    st.zero gen[6].32B, cell
    ld.zero gen[7].32B, cell
    push gen[7].32B
    pop gen[8].32B
    set gen[1].32B, 0
    set gen[2].32B, 0
    addc gen[1].32B, gen[2].32B
    FALL()
    HEX(gen[1].32B)
    HEX(gen[8].32B)
    PUTC('\n')
    ; m1 : sub borrow -> addc
    NAME('m','1')
    set gen[1].32B, 0
    sub gen[1].32B, one.32B
    set gen[1].32B, 5
    addc gen[1].32B, zero.32B
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')
    ; m2 : shift carry out -> addc
    NAME('m','2')
    LSET(gen[1], 0x80000000)
    shiftli gen[1].32B, 1
    set gen[1].32B, 5
    addc gen[1].32B, zero.32B
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')
    ; m3 : add carry -> subc (carry = borrow)
    NAME('m','3')
    LSET(gen[1], 0xFFFFFFFF)
    add gen[1].32B, one.32B
    set gen[1].32B, 5
    subc gen[1].32B, zero.32B
    FALL()
    HEX(gen[1].32B)
    PUTC('\n')

    NAME('o','k')
    PUTC('\n')
    jmp.one pc.32B, 0

; show : print " <gen[17] hex>"
show:
    PUTC(' ')
    set gen[18].32B, 0xF
    set gen[19].32B, 28
show_loop:
    mov gen[20].32B, gen[17].32B
    shiftr gen[20].32B, gen[19].32B
    bnd gen[20].32B, gen[18].32B
    cmpi gen[20].32B, 10
    jmp.neg zero.32B, show_num
    addi gen[20].32B, 'A'-10-'0'
show_num:
    addi gen[20].32B, '0'
    st.gen[24] gen[20].32B, TERM_OUT
    cmpi gen[19].32B, 0
    jmp.zero zero.32B, show_end
    subi gen[19].32B, 4
    jmp.one zero.32B, show_loop
show_end:
    ret
