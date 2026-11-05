; =====================================================================
;  memtest4.asm : flushed wrong-path store vs. a correct-path load
;
;  build (in program/) :  dasm test/memtest4.asm -o memtest4.o
;                         dlnk memtest4.o -o run.txt
;
;  press a key 1..7 to run one test, it prints "<key> <value>" and
;  waits for the next key. press each key many times and watch the
;  id-manager occupancy (memtest3 : 3, 7, 8 leak ids, 1 and 4 do not).
;
;  every loop below ends with a mispredicted exit, so the wrong path runs
;  one more "st X = gen[10]" with gen[10] = 0. the real last store is 1
;  (or the value noted). a result showing the wrong-path value means the
;  load saw the flushed store.
;
;  1  loop st X ; after the loop ld X              (= memtest3 3)  -> 1
;  2  loop st X ; after the loop ld Y (Y = 0x55)                   -> 55
;  3  loop st X ; block ; ld X                                     -> 1
;  4  loop st X ; nop x8 ; ld X                                    -> 1
;  5  loop st X+0x100 ; after the loop ld X                        -> 101
;     (wrong-path store writes 0x100)
;  6  loop has no memory op ; st X = 7 ; ld X                      -> 7
;  7  loop st Y ; after the loop ld X (X = 0x66)                   -> 66
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

.bss
slotA:  4
slotB:  4
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('4')
    PUTC(' ')
    PUTC('1')
    PUTC('-')
    PUTC('7')

menu:
    PUTC('\n')
    PUTC('>')
    PUTC(' ')
key:
    block
    ld.gen[24] gen[3].32B, KEY_V
    cmp gen[3].32B, zero.32B
    jmp.zero zero.32B, key
    ld.gen[24] gen[4].32B, KEY_IN
    st.gen[24] full.32B, KEY_R
    st.gen[24] gen[4].32B, TERM_OUT
    PUTC(' ')
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
    PUTC('?')
    jmp.one zero.32B, menu

show:
    call zero.32B, puthex
    jmp.one zero.32B, menu

t1:
    st.zero gen[10].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t1
    ld.zero gen[1].32B, slotA
    jmp.one zero.32B, show

t2:
    set gen[2].32B, 0x55
    st.zero gen[2].32B, slotB
    block
t2_lp:
    st.zero gen[10].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t2_lp
    ld.zero gen[1].32B, slotB
    jmp.one zero.32B, show

t3:
    st.zero gen[10].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t3
    block
    ld.zero gen[1].32B, slotA
    jmp.one zero.32B, show

t4:
    st.zero gen[10].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t4
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    ld.zero gen[1].32B, slotA
    jmp.one zero.32B, show

t5:
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x100
    st.zero gen[2].32B, slotA
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t5
    ld.zero gen[1].32B, slotA
    jmp.one zero.32B, show

t6:
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t6
    set gen[2].32B, 7
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, slotA
    jmp.one zero.32B, show

t7:
    set gen[2].32B, 0x66
    st.zero gen[2].32B, slotA
    block
t7_lp:
    st.zero gen[10].32B, slotB
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t7_lp
    ld.zero gen[1].32B, slotA
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
