; =====================================================================
;  selftest.asm : checks the instructions calc.asm needs on the circuit
;
;  build (in program/) :
;    dasm test/selftest.asm -o selftest.o
;    dasm floatingpoint/float.asm -o float.o
;    dlnk selftest.o float.o -o run.txt
;
;  output, one line per test :  <letter> <o|x|?> <value in hex>
;    o = as expected, x = wrong, ? = info only (no expected value)
;  the letter is printed BEFORE the test runs, so if the CPU restarts or
;  hangs, the last letter shows the failing test.
;
;  the harness itself only uses instructions that add_terminal.txt /
;  prime_terminal.txt already run on the circuit (set S16H/16L/32B, ld,
;  st, cmp, cmpi, jmp, ijmp.zero, mov, add(i), sub(i), shiftli, shiftri,
;  bnd, bor, call, ret).
; =====================================================================
#include<../floatingpoint/float.hsm>

#define IO_BASE 0x4000000
#define TERM_OUT 12

; reg = 32 bit constant (proven order)
#macro LSET 2
    set #1.S16H, ((#2) >> 16) & 0xFFFF
    set #1.16L, (#2) & 0xFFFF
#end

#macro PUTC 1
    set gen[20].32B, #1
    st.gen[24] gen[20].32B, TERM_OUT
#end

; new line + "<letter> "
#macro BEGIN 1
    PUTC('\n')
    PUTC(#1)
    PUTC(' ')
#end

; compare gen[1] with #1, print o/x and gen[1] in hex
#macro EXPECT 1
    LSET(gen[19], #1)
    set gen[20].32B, 'x'
    cmp gen[1].32B, gen[19].32B
    ijmp.zero zero.32B, ##bad
    set gen[20].32B, 'o'
##bad:
    st.gen[24] gen[20].32B, TERM_OUT
    PUTC(' ')
    call zero.32B, puthex
#end

#macro INFO 0
    PUTC('?')
    PUTC(' ')
    call zero.32B, puthex
#end

; one hex digit of gen[1] at bit #1
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

.data
s_five:     4, "5"                  ; byte string

.bss
obuf:       FP_STRBUF
stk:        4*256
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('s')
    PUTC('e')
    PUTC('l')
    PUTC('f')
    PUTC('t')
    PUTC('e')
    PUTC('s')
    PUTC('t')

    ; ---- set modes ----
    BEGIN('A')                          ; S16H alone : lower half kept or cleared?
    set gen[1].32B, 0x1234
    set gen[1].S16H, 0xABCD
    INFO()
    BEGIN('B')                          ; LSET
    LSET(gen[1], 0x12345678)
    EXPECT(0x12345678)

    ; ---- ALU ops never run on the circuit before ----
    BEGIN('C')                          ; bndi (imm zero extended)
    LSET(gen[1], 0xFFFFFFFF)
    bndi gen[1].32B, 0xFFFF
    EXPECT(0x0000FFFF)
    BEGIN('D')                          ; bori
    LSET(gen[1], 0x12340000)
    bori gen[1].32B, 0xF678
    EXPECT(0x1234F678)
    BEGIN('E')                          ; bxr
    LSET(gen[1], 0xFF00FF00)
    LSET(gen[2], 0x0FF00FF0)
    bxr gen[1].32B, gen[2].32B
    EXPECT(0xF0F0F0F0)
    BEGIN('F')                          ; bxri
    LSET(gen[1], 0x12345678)
    bxri gen[1].32B, 0xFFFF
    EXPECT(0x1234A987)
    BEGIN('G')                          ; shiftl by register
    LSET(gen[1], 0x12345678)
    set gen[2].32B, 4
    shiftl gen[1].32B, gen[2].32B
    EXPECT(0x23456780)
    BEGIN('H')                          ; shiftl by 31
    set gen[1].32B, 1
    set gen[2].32B, 31
    shiftl gen[1].32B, gen[2].32B
    EXPECT(0x80000000)
    BEGIN('I')                          ; shiftr by register (logical)
    LSET(gen[1], 0xF0000000)
    set gen[2].32B, 4
    shiftr gen[1].32B, gen[2].32B
    EXPECT(0x0F000000)
    BEGIN('J')                          ; shiftr by 31
    LSET(gen[1], 0x80000000)
    set gen[2].32B, 31
    shiftr gen[1].32B, gen[2].32B
    EXPECT(1)
    BEGIN('K')                          ; shiftri is logical
    LSET(gen[1], 0x80000000)
    shiftri gen[1].32B, 31
    EXPECT(1)
    BEGIN('L')                          ; mov from full
    mov gen[1].32B, full.32B
    EXPECT(0xFFFFFFFF)

    ; ---- flags / branches ----
    BEGIN('M')                          ; testi : 2 & 1 = 0 -> zero (+1),
    set gen[1].32B, 0                   ;         3 & 1 = 1 -> not zero (+2)
    set gen[2].32B, 2
    testi gen[2].32B, 1
    ijmp.zero zero.32B, m1
    addi gen[1].32B, 1
m1:
    set gen[2].32B, 3
    testi gen[2].32B, 1
    jmp.zero zero.32B, m2
    addi gen[1].32B, 2
m2:
    EXPECT(3)
    BEGIN('N')                          ; ijmp.neg : 5-3 taken, 2-3 not (+2)
    set gen[1].32B, 0
    set gen[2].32B, 5
    cmpi gen[2].32B, 3
    ijmp.neg zero.32B, n1
    addi gen[1].32B, 1
n1:
    set gen[2].32B, 2
    cmpi gen[2].32B, 3
    ijmp.neg zero.32B, n2
    addi gen[1].32B, 2
n2:
    EXPECT(2)

    ; ---- stack ----
    BEGIN('O')                          ; push / pop order
    set gen[2].32B, 0x11
    set gen[3].32B, 0x22
    push gen[2].32B
    push gen[3].32B
    pop gen[4].32B
    pop gen[5].32B
    mov gen[1].32B, gen[4].32B
    shiftli gen[1].32B, 8
    bor gen[1].32B, gen[5].32B
    EXPECT(0x2211)
    BEGIN('P')                          ; stack pointer back where it was
    mov gen[1].32B, stack.32B
    set gen[2].32B, stk_end
    sub gen[1].32B, gen[2].32B
    EXPECT(0)
    BEGIN('Q')                          ; save 8 regs, nested call, restore
    call zero.32B, t_save
    EXPECT(0x24)                        ; 1+2+...+8 = 36
    BEGIN('R')                          ; recursion depth 12
    set gen[1].32B, 12
    set gen[2].32B, 0
    call zero.32B, t_rec
    mov gen[1].32B, gen[2].32B
    EXPECT(12)
    BEGIN('S')                          ; recursion depth 24
    set gen[1].32B, 24
    set gen[2].32B, 0
    call zero.32B, t_rec
    mov gen[1].32B, gen[2].32B
    EXPECT(24)

    ; ---- float library ----
    BEGIN('T')                          ; fp_add 1 + 2 = 3
    LSET(gen[1], 0x3F800000)
    LSET(gen[2], 0x40000000)
    call zero.32B, fp_add
    mov gen[1].32B, gen[0].32B
    EXPECT(0x40400000)
    BEGIN('U')                          ; fp_div 5 / 2 = 2.5
    LSET(gen[1], 0x40A00000)
    LSET(gen[2], 0x40000000)
    call zero.32B, fp_div
    mov gen[1].32B, gen[0].32B
    EXPECT(0x40200000)
    BEGIN('V')                          ; fp_mul 1.5 * 3 = 4.5
    LSET(gen[1], 0x3FC00000)
    LSET(gen[2], 0x40400000)
    call zero.32B, fp_mul
    mov gen[1].32B, gen[0].32B
    EXPECT(0x40900000)
    BEGIN('W')                          ; fp_str2f "5"
    set gen[1].32B, s_five
    call zero.32B, fp_str2f
    mov gen[1].32B, gen[0].32B
    EXPECT(0x40A00000)
    BEGIN('X')                          ; fp_f2str 2.5 -> "2.5", length 3
    LSET(gen[1], 0x40200000)
    set gen[2].32B, obuf
    set gen[3].32B, 7
    call zero.32B, fp_f2str
    mov gen[1].32B, gen[0].32B
    EXPECT(3)
    PUTC(' ')
    set gen[1].32B, 0                   ; byte string : 3 chars
    ld.zero gen[1].8L, obuf
    st.gen[24] gen[1].32B, TERM_OUT
    ld.zero gen[1].8L, obuf+1
    st.gen[24] gen[1].32B, TERM_OUT
    ld.zero gen[1].8L, obuf+2
    st.gen[24] gen[1].32B, TERM_OUT

    BEGIN('Y')                          ; halt
    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
    halt
    PUTC('!')                           ; should never print
done:
    jmp.one zero.32B, done

; ---------------------------------------------------------------------
    ; print gen[1] as 8 hex digits (gen[1] kept, uses gen[18], gen[20])
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

    ; gen[1] = sum of gen[5..12] after save / nested call / restore
t_save:
    set gen[5].32B, 1
    set gen[6].32B, 2
    set gen[7].32B, 3
    set gen[8].32B, 4
    set gen[9].32B, 5
    set gen[10].32B, 6
    set gen[11].32B, 7
    set gen[12].32B, 8
    push gen[5].32B
    push gen[6].32B
    push gen[7].32B
    push gen[8].32B
    push gen[9].32B
    push gen[10].32B
    push gen[11].32B
    push gen[12].32B
    set gen[5].32B, 100
    set gen[12].32B, 100
    call zero.32B, t_inner
    pop gen[12].32B
    pop gen[11].32B
    pop gen[10].32B
    pop gen[9].32B
    pop gen[8].32B
    pop gen[7].32B
    pop gen[6].32B
    pop gen[5].32B
    mov gen[1].32B, gen[5].32B
    add gen[1].32B, gen[6].32B
    add gen[1].32B, gen[7].32B
    add gen[1].32B, gen[8].32B
    add gen[1].32B, gen[9].32B
    add gen[1].32B, gen[10].32B
    add gen[1].32B, gen[11].32B
    add gen[1].32B, gen[12].32B
    ret
t_inner:
    push gen[5].32B
    push gen[12].32B
    set gen[5].32B, 200
    pop gen[12].32B
    pop gen[5].32B
    ret

    ; gen[2] += gen[1] by recursion depth gen[1]
t_rec:
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, t_rec_end
    subi gen[1].32B, 1
    call zero.32B, t_rec
    addi gen[2].32B, 1
t_rec_end:
    ret
