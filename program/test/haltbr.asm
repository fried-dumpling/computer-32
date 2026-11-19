; =====================================================================
;  haltbr.asm : halt right after a branch that is predicted not taken,
;               but taken, and resolves late (CLAUDE.md known problem)
;
;  one case per image: put "#define H<n> 1" in front, e.g.
;    (echo '#define H3 1'; cat test/haltbr.asm) > build/haltbr3.asm
;    dasm build/haltbr3.asm -o haltbr3.o ; dlnk haltbr3.o -o ../boot/rom.bin
;
;  output : "A" before the branch, "B" at the branch target, then end.
;  expected "AB". "A" alone means halt ran on the wrong path.
;    H1  : cmp of constants (branch resolves at once)
;    H2  : cmp after a 24 long dependent chain
;    H3  : cmp of a loaded value
;    H4  : cmp after a 4 deep pointer chase
;    H5  : cmp of a load that self flushes (late store to the same address)
;    H6  : loop of 3 rounds with a loaded counter, halt after the loop branch
;          (expected "A123", no "B")
;    H7  : branch not taken (correct) then a late taken branch, then halt
;    H8  : jmp.one through a loaded target register, then halt
;    H9  : cmp after a store to the terminal and 8 loads in flight
;    H10 : like H2, but the target is far ahead (fetch keeps going past halt)
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12

#macro PUTC 1
    set gen[20].32B, #1
    st.gen[24] gen[20].32B, TERM_OUT
#end

#macro CHAIN4 1
    addi #1, 1
    addi #1, 1
    addi #1, 1
    addi #1, 1
#end

.data
val:    4, 7
p0:     4, p1
p1:     4, p2
p2:     4, p3
p3:     4, val
cnt:    4, 3
tgtw:   4, target
arr:    4*8, 0

.bss
slot:   4
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    set gen[24].S16H, IO_BASE >> 16
    set gen[24].16L, IO_BASE & 0xFFFF
    PUTC('A')

#ifdef H1
    set gen[1].32B, 5
    cmpi gen[1].32B, 5
    jmp.zero zero.32B, target
    halt
#end

#ifdef H2
    set gen[1].32B, 0
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    cmpi gen[1].32B, 24
    jmp.zero zero.32B, target
    halt
#end

#ifdef H3
    ld.zero gen[1].32B, val
    cmpi gen[1].32B, 7
    jmp.zero zero.32B, target
    halt
#end

#ifdef H4
    ld.zero gen[1].32B, p0
    ld.gen[1] gen[1].32B, 0
    ld.gen[1] gen[1].32B, 0
    ld.gen[1] gen[1].32B, 0
    ld.gen[1] gen[1].32B, 0
    cmpi gen[1].32B, 7
    jmp.zero zero.32B, target
    halt
#end

#ifdef H5
    set gen[2].32B, 0
    CHAIN4(gen[2].32B)
    CHAIN4(gen[2].32B)
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    cmpi gen[1].32B, 8
    jmp.zero zero.32B, target
    halt
#end

#ifdef H6
    set gen[6].32B, '1'
    ld.zero gen[5].32B, cnt
h6loop:
    st.gen[24] gen[6].32B, TERM_OUT
    addi gen[6].32B, 1
    subi gen[5].32B, 1
    st.zero gen[5].32B, cnt
    ld.zero gen[5].32B, cnt
    cmpi gen[5].32B, 0
    ijmp.zero zero.32B, h6loop
    halt
#end

#ifdef H7
    set gen[1].32B, 1
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, h7bad
    ld.zero gen[1].32B, val
    CHAIN4(gen[1].32B)
    cmpi gen[1].32B, 11
    jmp.zero zero.32B, target
    halt
h7bad:
    PUTC('X')
    halt
#end

#ifdef H8
    ld.zero gen[1].32B, tgtw
    jmp.one gen[1].32B, 0
    halt
#end

#ifdef H9
    PUTC('-')
    ld.zero gen[13].32B, arr
    ld.zero gen[14].32B, arr+4
    ld.zero gen[15].32B, arr+8
    ld.zero gen[16].32B, arr+12
    ld.zero gen[17].32B, arr+16
    ld.zero gen[18].32B, arr+20
    ld.zero gen[19].32B, arr+24
    ld.zero gen[1].32B, val
    cmpi gen[1].32B, 7
    jmp.zero zero.32B, target
    halt
#end

#ifdef H10
    set gen[1].32B, 0
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    CHAIN4(gen[1].32B)
    cmpi gen[1].32B, 24
    jmp.zero zero.32B, target
    halt
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    PUTC('Y')
    halt
#end

    PUTC('Z')                           ; fell through without halting
    halt

target:
    PUTC('B')
    PUTC('\n')
    jmp.one pc.32B, 0
