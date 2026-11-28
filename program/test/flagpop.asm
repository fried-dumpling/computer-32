; =====================================================================
;  flagpop.asm : an ALU op right after `pop flag` (errors.md 59)
;
;  build (in program/) :  dasm test/flagpop.asm -o flagpop.o
;                         dlnk flagpop.o -o ../boot/rom.bin
;  the emulator has no carry ops and no flag register: this program checks
;  itself. output per case: "<case> <flag hex> PASS" or "... FAIL", then "E".
;  <flag hex> is `push flag` right after the case (for d, e: the addc result).
;  cmp 0,1 gives 0x4A (one, carry, neg), cmpi 0,0 gives 0x51 (one, carry4, zero);
;  r1, r2 show these without a pop.
;    r1 : cmp 0,1                                       expect 4A
;    r2 : cmpi 0,0                                      expect 51
;    a  : pop flag (0) ; cmp 0,1                        expect 4A
;    b  : pop flag (0) ; nop ; cmp 0,1                  expect 4A
;    c  : pop flag (0) ; set ; cmp 0,1                  expect 4A
;    d  : pop flag (8 = carry) ; addc 0,0               expect result 1
;    e  : pop flag (8) ; set ; set ; addc 0,0           expect result 1
;    f  : pop flag (0) ; cmp late,1 (late = DELAY 0)    expect 4A
;         (the cmp completes long after the pop)
;    g  : pop flag (0) ; cmp 0,1 ; jmp.neg              expect taken (T)
;    h  : pop flag (4A) ; cmpi 0,0                      expect 51
;    i  : pop flag (0) ; cmp 0,1, the pop's load late  expect 4A
;         (DELAY before: the pop waits in the load buffer)
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

; CASE(name) : let the pipeline drain, print the name
#macro CASE 1
    set gen[7].32B, 40
##wait:
    subi gen[7].32B, 1
    cmpi gen[7].32B, 0
    jmp.pos zero.32B, ##wait
    PUTC(#1)
#end

; POPF(value) : flag = value through the stack
#macro POPF 1
    set gen[3].32B, #1
    push gen[3].32B
    pop flag.32B
#end

; FLAGV(expected) : gen[1] = flag now, then the verdict
#macro FLAGV 1
    push flag.32B
    pop gen[1].32B
    set gen[2].32B, #1
    call zero.32B, verdict
#end

; RESV(expected) : verdict on gen[1]
#macro RESV 1
    set gen[2].32B, #1
    call zero.32B, verdict
#end

; DELAY : gen[9] = 0, late (a load and 8 dependent addi)
#macro DELAY 0
    ld.zero gen[9].32B, slow
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
    addi gen[9].32B, 0
#end

.data
slow:   4, 0

.bss
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('f')
    PUTC('l')
    PUTC('g')
    PUTC('\n')
    set gen[5].32B, 0
    set gen[6].32B, 1

    CASE('1')
    cmp gen[5].32B, gen[6].32B
    FLAGV(0x4A)

    CASE('2')
    cmpi gen[5].32B, 0
    FLAGV(0x51)

    CASE('a')
    POPF(0)
    cmp gen[5].32B, gen[6].32B
    FLAGV(0x4A)

    CASE('b')
    POPF(0)
    nop
    cmp gen[5].32B, gen[6].32B
    FLAGV(0x4A)

    CASE('c')
    POPF(0)
    set gen[4].32B, 0
    cmp gen[5].32B, gen[6].32B
    FLAGV(0x4A)

    CASE('d')
    set gen[1].32B, 0
    set gen[2].32B, 0
    POPF(8)
    addc gen[1].32B, gen[2].32B
    RESV(1)

    CASE('e')
    set gen[1].32B, 0
    set gen[2].32B, 0
    POPF(8)
    set gen[4].32B, 0
    set gen[4].32B, 0
    addc gen[1].32B, gen[2].32B
    RESV(1)

    CASE('f')
    DELAY
    POPF(0)
    cmp gen[9].32B, gen[6].32B
    FLAGV(0x4A)

    CASE('g')
    POPF(0)
    cmp gen[5].32B, gen[6].32B
    set gen[1].32B, 'N'
    jmp.neg zero.32B, g_t
    jmp.one zero.32B, g_v
g_t:
    set gen[1].32B, 'T'
g_v:
    RESV('T')

    CASE('h')
    POPF(0x4A)
    cmpi gen[5].32B, 0
    FLAGV(0x51)

    CASE('i')
    set gen[3].32B, 0
    push gen[3].32B
    DELAY
    add gen[3].32B, gen[9].32B
    st.stack gen[3].32B, 0
    pop flag.32B
    cmp gen[5].32B, gen[6].32B
    FLAGV(0x4A)

    PUTC('E')
    PUTC('\n')
    jmp.one pc.32B, 0

; verdict : gen[1] = value, gen[2] = expected; prints " <gen[1] hex> PASS|FAIL"
verdict:
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
    cmp gen[1].32B, gen[2].32B
    jmp.zero zero.32B, v_pass
    PUTC(' ')
    PUTC('F')
    PUTC('A')
    PUTC('I')
    PUTC('L')
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
