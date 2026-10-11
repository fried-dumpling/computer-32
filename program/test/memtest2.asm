; =====================================================================
;  memtest2.asm : narrows down the hang of memtest 'a' (st then ld)
;
;  build (in program/) :  dasm test/memtest2.asm -o memtest2.o
;                         dlnk memtest2.o -o run.txt
;
;  output : <test> <value read in hex>, one shot each (no loops) except
;  the last two. the name is printed BEFORE the test runs, so a hang
;  stops right after the name of the test that hung.
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

#macro BEGIN 2
    PUTC('\n')
    PUTC(#1)
    PUTC(#2)
    PUTC(' ')
#end

#macro SHOW 0
    call zero.32B, puthex
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

.data
pre:    4, 0x7777                       ; already in memory before the run
.bss
slotA:  4
slotB:  4
stk:    4*64
stk_end:

.text
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('2')

    BEGIN('l', '0')                     ; ld only (no store before)
    ld.zero gen[1].32B, pre
    SHOW()

    BEGIN('s', '0')                     ; st only, ld much later
    set gen[2].32B, 0x1234
    st.zero gen[2].32B, slotB
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    ld.zero gen[1].32B, slotB
    SHOW()

    BEGIN('a', '1')                     ; st X ; ld X   (back to back)
    set gen[2].32B, 0x1111
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, slotA
    SHOW()

    BEGIN('a', '2')                     ; st X ; nop x4 ; ld X
    set gen[2].32B, 0x2222
    st.zero gen[2].32B, slotA
    nop
    nop
    nop
    nop
    ld.zero gen[1].32B, slotA
    SHOW()

    BEGIN('a', '3')                     ; st X ; block ; ld X
    set gen[2].32B, 0x3333
    st.zero gen[2].32B, slotA
    block
    ld.zero gen[1].32B, slotA
    SHOW()

    BEGIN('a', '4')                     ; st X ; ld Y   (different address)
    set gen[2].32B, 0x4444
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, pre
    SHOW()

    BEGIN('a', '5')                     ; value reg written just before st
    set gen[2].32B, 0x5555              ; (same as a1 but via add)
    addi gen[2].32B, 0
    st.zero gen[2].32B, slotA
    ld.zero gen[1].32B, slotA
    SHOW()

    BEGIN('a', '6')                     ; st X ; ld X with another base reg
    set gen[13].32B, slotA
    set gen[2].32B, 0x6666
    st.zero gen[2].32B, slotA
    ld.gen[13] gen[1].32B, 0
    SHOW()

    BEGIN('a', '7')                     ; st X ; st X ; ld X
    set gen[2].32B, 0x7070
    set gen[3].32B, 0x7171
    st.zero gen[2].32B, slotA
    st.zero gen[3].32B, slotA
    ld.zero gen[1].32B, slotA
    SHOW()

    BEGIN('a', '8')                     ; ld X ; st X  (load first)
    set gen[2].32B, 0x8888
    ld.zero gen[1].32B, slotA
    st.zero gen[2].32B, slotA
    SHOW()

    BEGIN('L', '1')                     ; loop x16 of block-separated st/ld
    set gen[10].32B, 16
    set gen[11].32B, 0
lp1:
    st.zero gen[10].32B, slotA
    block
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, lp1
    mov gen[1].32B, gen[11].32B         ; 16+15+...+1 = 0x88
    SHOW()

    BEGIN('L', '2')                     ; loop x16 of back-to-back st/ld
    set gen[10].32B, 16
    set gen[11].32B, 0
lp2:
    st.zero gen[10].32B, slotA
    ld.zero gen[1].32B, slotA
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, lp2
    mov gen[1].32B, gen[11].32B         ; 0x88
    SHOW()

    PUTC('\n')
    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
    halt
done:
    jmp.one zero.32B, done

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
