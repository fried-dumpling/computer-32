; =====================================================================
;  memtest3.asm : which part of "st X ; ld X in a loop" hangs (memtest2 L2)
;
;  build (in program/) :  dasm test/memtest3.asm -o memtest3.o
;                         dlnk memtest3.o -o run.txt
;
;  press a key 1..9 to run one test. it prints "<key> <result in hex>"
;  and waits for the next key. if a test hangs, reset the circuit and
;  press the next key. every test has 16 store/load pairs, expected
;  result 00000088 (16+15+...+1) unless noted.
;
;  1  unrolled : 16 x (st X ; ld X), no branch at all
;  2  unrolled : 16 x (st X ; ld Y), different addresses, no branch
;  3  loop     : st only (16 x), one ld after the loop
;  4  loop     : st X ; ld Y (different addresses)
;  5  loop     : ld X ; st X (load first)
;  6  loop     : st X ; nop x4 ; ld X
;  7  loop     : st X ; ld X, but only 2 iterations   (expected 3)
;  8  loop     : st X ; ld X, branch is a forward jmp + backward jmp.one
;  9  loop     : st X ; ld X  (= memtest2 L2, the one that hung)
; =====================================================================
#define IO_BASE 0x4000000
#define KEY_IN 0
#define KEY_V 4
#define KEY_R 8
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

; one unrolled pair : X = #1, then gen[11] += X
#macro PAIRX 1
    set gen[2].32B, #1
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
#end

; one unrolled pair : st X = #1, ld Y (holds 0) , then gen[11] += #1
#macro PAIRY 1
    set gen[2].32B, #1
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, slotB
    add gen[11].32B, gen[2].32B
    add gen[11].32B, gen[1].32B
#end

.bss
slotA:  4
slotB:  4
stk:    4*64
stk_end:

.text
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('3')
    PUTC(' ')
    PUTC('1')
    PUTC('-')
    PUTC('9')

menu:
    PUTC('\n')
    PUTC('>')
    PUTC(' ')
key:                                    ; same polling as add_terminal.txt
    block
    ld.gen[24] gen[3].32B, KEY_V
    cmp gen[3].32B, zero.32B
    jmp.zero zero.32B, key
    ld.gen[24] gen[4].32B, KEY_IN
    st.gen[24] full.32B, KEY_R
    st.gen[24] gen[4].32B, TERM_OUT
    PUTC(' ')
    set gen[11].32B, 0
    set gen[10].32B, 16
    st.zero zero.32B, slotA
    st.zero zero.32B, slotB
    block
    cmpi gen[4].32B, '1'
    jmp.zero zero.32B, t1
    cmpi gen[4].32B, '2'
    jmp.zero zero.32B, t2
    cmpi gen[4].32B, '3'
    jmp.zero zero.32B, t3
    cmpi gen[4].32B, '4'
    jmp.zero zero.32B, t4
    cmpi gen[4].32B, '5'
    jmp.zero zero.32B, t5
    cmpi gen[4].32B, '6'
    jmp.zero zero.32B, t6
    cmpi gen[4].32B, '7'
    jmp.zero zero.32B, t7
    cmpi gen[4].32B, '8'
    jmp.zero zero.32B, t8
    cmpi gen[4].32B, '9'
    jmp.zero zero.32B, t9
    PUTC('?')
    jmp.one zero.32B, menu

show:
    mov gen[1].32B, gen[11].32B
    call zero.32B, puthex
    jmp.one zero.32B, menu

t1:
    PAIRX(16)
    PAIRX(15)
    PAIRX(14)
    PAIRX(13)
    PAIRX(12)
    PAIRX(11)
    PAIRX(10)
    PAIRX(9)
    PAIRX(8)
    PAIRX(7)
    PAIRX(6)
    PAIRX(5)
    PAIRX(4)
    PAIRX(3)
    PAIRX(2)
    PAIRX(1)
    jmp.one zero.32B, show

t2:
    PAIRY(16)
    PAIRY(15)
    PAIRY(14)
    PAIRY(13)
    PAIRY(12)
    PAIRY(11)
    PAIRY(10)
    PAIRY(9)
    PAIRY(8)
    PAIRY(7)
    PAIRY(6)
    PAIRY(5)
    PAIRY(4)
    PAIRY(3)
    PAIRY(2)
    PAIRY(1)
    jmp.one zero.32B, show

t3:                                     ; st only, then 1 ld (expected 1 = last value)
    st.zero gen[10].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t3
    ld.zero gen[11].32B, slotA
    jmp.one zero.32B, show

t4:                                     ; st X ; ld Y (Y = 0)
    st.zero gen[10].32B, slotA
    ld.zero gen[1].32B, slotB
    add gen[11].32B, gen[10].32B
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t4
    jmp.one zero.32B, show

t5:                                     ; ld X ; st X  (reads previous value)
    ld.zero gen[1].32B, slotA           ; sum of 0,16,...,2 = 0x87
    st.zero gen[10].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t5
    jmp.one zero.32B, show

t6:                                     ; st X ; nop x4 ; ld X
    st.zero gen[10].32B, slotA
    nop
    nop
    nop
    nop
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t6
    jmp.one zero.32B, show

t7:                                     ; 2 iterations (2 + 1 = 3)
    set gen[10].32B, 2
t7_lp:
    st.zero gen[10].32B, slotA
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t7_lp
    jmp.one zero.32B, show

t8:                                     ; exit test first, loop back with jmp.one
    cmpi gen[10].32B, 0
    jmp.zero zero.32B, show
    st.zero gen[10].32B, slotA
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    jmp.one zero.32B, t8

t9:                                     ; = memtest2 L2
    st.zero gen[10].32B, slotA
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t9
    jmp.one zero.32B, show

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
