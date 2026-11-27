; =====================================================================
;  straddle.asm : forward from a store at another byte offset of the
;                 same word, while the store is still in the store buffer.
;                 only aligned accesses (half at even, word at a multiple
;                 of 4): an unaligned ld/st raises fault 5, tested by
;                 test/interrupt/int_align.asm.
;
;  build (in program/) :  dasm test/memory/straddle.asm -o straddle.o
;                         dlnk straddle.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh straddle  (compares with the emulator)
;
;  output : "<case> PASS" or "<case> FAIL <word read hex>", then "E".
;  every case first writes the two words W0 = 0x11223344 (at X) and
;  W1 = 0x55667788 (at X+4) and lets the store buffer drain (INIT), then
;  starts with DELAY (a load of `slow` and dependent addi) so the case's
;  own stores wait in the store buffer when the load looks them up.
;  memory bytes from X : 44 33 22 11 88 77 66 55
;  A = 0x99AABBCC, H = 0xEEFF
;  load narrower than the store, at another offset:
;    a : st.32B X <- A ; ld.8L X+1                    expect 000000BB
;    b : st.32B X <- A ; ld.8L X+2                    expect 000000AA
;    c : st.32B X <- A ; ld.8L X+3                    expect 00000099
;    d : st.32B X <- A ; ld.16L X+2                   expect 000099AA
;    e : st.32B X <- A ; ld.8H X+1  (byte to [15:8])  expect 0000BB00
;    f : st.16L X+2 <- H ; ld.8L X+3                  expect 000000EE
;    g : st.16L X+2 <- H ; ld.8L X+2                  expect 000000FF
;    h : st.8H X+2 <- H (byte EE) ; ld.8L X+2         expect 000000EE
;  load wider than the store (store data + memory):
;    i : st.16L X+2 <- H ; ld.32B X                   expect EEFF3344
;    j : st.8L X+1 <- H (byte FF) ; ld.32B X          expect 1122FF44
;    k : st.8L X+3 <- H ; ld.16L X+2                  expect 0000FF22
;  same offset / other word:
;    l : st.8L X+1 <- H ; ld.8L X+1                   expect 000000FF
;    m : st.32B X+4 <- A ; ld.32B X                   expect 11223344
;    n : like d without DELAY                         expect 000099AA
;  partial loads go into a register cleared first (ld keeps other bits).
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

; INIT(name) : X = W0, X+4 = W1, empty the store buffer, print the name
#macro INIT 1
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw+4
    set gen[7].32B, 60
##wait:
    subi gen[7].32B, 1
    cmpi gen[7].32B, 0
    jmp.pos zero.32B, ##wait
    PUTC(#1)
    set gen[1].32B, 0
#end

; DELAY : a load and a chain of addi, so the commit waits
#macro DELAY 0
    ld.zero gen[9].32B, slow
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
#end

; VERDICT(expected) : gen[1] = word read
#macro VERDICT 1
    LSET(gen[2], #1)
    call zero.32B, verdict
#end

.data
slow:   4, 0

.bss
pad:    4
xw:     8
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('s')
    PUTC('t')
    PUTC('r')
    PUTC('\n')
    LSET(gen[3], 0x11223344)
    LSET(gen[4], 0x55667788)
    LSET(gen[5], 0x99AABBCC)
    LSET(gen[8], 0x0000EEFF)

    INIT('a')
    DELAY
    st.zero gen[5].32B, xw
    ld.zero gen[1].8L, xw+1
    VERDICT(0x000000BB)

    INIT('b')
    DELAY
    st.zero gen[5].32B, xw
    ld.zero gen[1].8L, xw+2
    VERDICT(0x000000AA)

    INIT('c')
    DELAY
    st.zero gen[5].32B, xw
    ld.zero gen[1].8L, xw+3
    VERDICT(0x00000099)

    INIT('d')
    DELAY
    st.zero gen[5].32B, xw
    ld.zero gen[1].16L, xw+2
    VERDICT(0x000099AA)

    INIT('e')
    DELAY
    st.zero gen[5].32B, xw
    ld.zero gen[1].8H, xw+1
    VERDICT(0x0000BB00)

    INIT('f')
    DELAY
    st.zero gen[8].16L, xw+2
    ld.zero gen[1].8L, xw+3
    VERDICT(0x000000EE)

    INIT('g')
    DELAY
    st.zero gen[8].16L, xw+2
    ld.zero gen[1].8L, xw+2
    VERDICT(0x000000FF)

    INIT('h')
    DELAY
    st.zero gen[8].8H, xw+2
    ld.zero gen[1].8L, xw+2
    VERDICT(0x000000EE)

    INIT('i')
    DELAY
    st.zero gen[8].16L, xw+2
    ld.zero gen[1].32B, xw
    VERDICT(0xEEFF3344)

    INIT('j')
    DELAY
    st.zero gen[8].8L, xw+1
    ld.zero gen[1].32B, xw
    VERDICT(0x1122FF44)

    INIT('k')
    DELAY
    st.zero gen[8].8L, xw+3
    ld.zero gen[1].16L, xw+2
    VERDICT(0x0000FF22)

    INIT('l')
    DELAY
    st.zero gen[8].8L, xw+1
    ld.zero gen[1].8L, xw+1
    VERDICT(0x000000FF)

    INIT('m')
    DELAY
    st.zero gen[5].32B, xw+4
    ld.zero gen[1].32B, xw
    VERDICT(0x11223344)

    INIT('n')
    st.zero gen[5].32B, xw
    ld.zero gen[1].16L, xw+2
    VERDICT(0x000099AA)

    PUTC('E')
    PUTC('\n')
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
