; =====================================================================
;  memtest6.asm : ld X followed by a PUSH whose address is ready late
;                (the M4.csv deadlock : ld [0x2D0] then call -> st [0x3D0])
;
;  build (in program/) :  dasm test/memtest6.asm -o memtest6.o
;                         dlnk memtest6.o -o run.txt
;
;  memtest5 (ld X ; st X+OFF with the address ready at once) passed for
;  every OFF. here the store is a push : its address comes from the stack
;  update, so it is known a few cycles after the load's address, like the
;  call in M4.csv. each key runs 8 x (ld X ; push) and prints the loaded
;  value sum (8 x 0x11 = 00000088). if it hangs, reset and try the next key.
;
;  1  push to X+0x104   (different low byte)       control
;  2  push to X+0x100   (same low byte, bit 8)     = M4.csv
;  3  push to X+0x200   (same low byte, bit 9)
;  4  push to X+0x1000  (same low byte, bit 12)
;  5  push to X+0x40    (bit 6)
;  6  call to X+0x100   (exactly the M4 shape : ld, jmp, call)
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



; stack = X + #1 + 4, then 8 x (ld X ; push) , gen[11] += loaded value
#macro LDPUSH 1
    set gen[15].32B, (#1) + 4
    add gen[15].32B, gen[13].32B
    mov stack.32B, gen[15].32B
    block
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    jmp.one zero.32B, ##half
##half:
    mov stack.32B, gen[15].32B
    block
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    ld.gen[13] gen[1].32B, 0
    push gen[2].32B
    add gen[11].32B, gen[1].32B
    jmp.one zero.32B, done
#end

.bss
area:   4*0x480                         ; X = area, pushes land at X+OFF
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('6')
    PUTC(' ')
    PUTC('1')
    PUTC('-')
    PUTC('6')

menu:
    set stack.32B, stk_end
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
    set gen[13].32B, area
    set gen[2].32B, 0x11
    st.gen[13] gen[2].32B, 0            ; X = 0x11
    set gen[2].32B, 0x99                ; pushed value
    set gen[11].32B, 0
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
    jmp.zero zero.32B, t7
    PUTC('?')
    jmp.one zero.32B, menu

done:
    set stack.32B, stk_end
    mov gen[1].32B, gen[11].32B
    call zero.32B, puthex
    jmp.one zero.32B, menu

t1:
    LDPUSH(0x104)
t2:
    LDPUSH(0x100)
t3:
    LDPUSH(0x200)
t4:
    LDPUSH(0x1000)
t5:
    LDPUSH(0x40)
t7:                                     ; ld X ; jmp ; call (push to X+0x100)
    set gen[15].32B, 0x104
    add gen[15].32B, gen[13].32B
    mov stack.32B, gen[15].32B
    set gen[10].32B, 8
    block
t7_lp:
    ld.gen[13] gen[1].32B, 0
    jmp.one zero.32B, t7_c
t7_c:
    call zero.32B, t7_f
    add gen[11].32B, gen[1].32B
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, t7_lp
    jmp.one zero.32B, done
t7_f:
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
