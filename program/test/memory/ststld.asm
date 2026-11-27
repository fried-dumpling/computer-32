; =====================================================================
;  ststld.asm : two or more stores to the same word, then a load of it.
;               the load must get the youngest older store's value.
;
;  build (in program/) :  dasm test/memory/ststld.asm -o ststld.o
;                         dlnk ststld.o -o ../boot/rom.bin
;
;  output : "<case> PASS" or "<case> FAIL <word read hex>"
;  every case but g starts with DELAY: a load of `slow` and a chain of
;  addi on it, so the commit waits and the stores stay in the store
;  buffer when the load looks them up. DRAIN between cases lets the
;  store buffer empty, so a case sees only its own stores.
;    a : st X<-A1 ; st X<-B2 ; ld X                       expect B2
;    b : st X<-A1 ; st X<-B2 ; st X<-C3 ; ld X             expect C3
;    c : st Y1 ; st Y2 ; st X<-A1 ; st X<-B2 ; ld X        expect B2
;        (other stores first: the X stores get other slots)
;    d : st Y1 ; st X<-A1 ; st X<-B2 ; ld X                expect B2
;    e : st X<-A1 (data late) ; st X<-B2 ; ld X            expect B2
;    f : st X<-A1 ; st X<-B2 (data late) ; ld X            expect B2
;        (the younger store has no data yet when the load looks)
;    g : like a, no DELAY                                  expect B2
;    h : st X<-A1 ; st Y1 ; st X<-B2 ; ld X                expect B2
;  the late data comes from the DELAY chain (gen[9] - gen[9] + value).
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

; CASE(name) : print the name, empty the store buffer
#macro CASE 1
    set gen[7].32B, 60
##wait:
    subi gen[7].32B, 1
    cmpi gen[7].32B, 0
    jmp.pos zero.32B, ##wait
    PUTC(#1)
#end

; DELAY(n) : gen[9] = slow + n, late (a load and 8 dependent addi)
#macro DELAY 1
    ld.zero gen[9].32B, slow
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, 1
    addi gen[9].32B, (#1) - 7
#end

; LATE(reg, value) : reg = value, ready only after DELAY
#macro LATE 2
    mov #1.32B, gen[9].32B
    sub #1.32B, gen[9].32B
    addi #1.32B, #2
#end

; CHECK(expected) : ld X, PASS if it is expected
#macro CHECK 1
    ld.zero gen[1].32B, xw
    set gen[2].32B, #1
    call zero.32B, verdict
#end

.data
slow:   4, 0

.bss
xw:     4
yw1:    4
yw2:    4
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('s')
    PUTC('s')
    PUTC('l')
    PUTC('\n')
    set gen[3].32B, 0xA1
    set gen[4].32B, 0xB2
    set gen[5].32B, 0xC3
    set gen[6].32B, 0x55

    CASE('a')
    DELAY(0)
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    CASE('b')
    DELAY(0)
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw
    st.zero gen[5].32B, xw
    CHECK(0xC3)

    CASE('c')
    DELAY(0)
    st.zero gen[6].32B, yw1
    st.zero gen[6].32B, yw2
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    CASE('d')
    DELAY(0)
    st.zero gen[6].32B, yw1
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    CASE('e')
    DELAY(0)
    LATE(gen[10], 0xA1)
    st.zero gen[10].32B, xw
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    CASE('f')
    DELAY(0)
    LATE(gen[10], 0xB2)
    st.zero gen[3].32B, xw
    st.zero gen[10].32B, xw
    CHECK(0xB2)

    CASE('g')
    st.zero gen[3].32B, xw
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    CASE('h')
    DELAY(0)
    st.zero gen[3].32B, xw
    st.zero gen[6].32B, yw1
    st.zero gen[4].32B, xw
    CHECK(0xB2)

    PUTC('E')
    PUTC('\n')
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
