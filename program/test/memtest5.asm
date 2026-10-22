; =====================================================================
;  memtest5.asm : does a YOUNGER store block an older load? which address
;                bits does the load/store match compare?
;
;  build (in program/) :  dasm test/memtest5.asm -o memtest5.o
;                         dlnk memtest5.o -o run.txt
;
;  found in log/M4.csv : the load "ld [0x2D0]" never got its data after the
;  younger push of a call "st [0x3D0]" entered the store buffer. the two
;  addresses differ only in bit 8. the load was the oldest instruction, the
;  store can only drain after commit -> deadlock.
;
;  press a key, it runs 8 x (ld X ; st X+OFF) back to back and prints "ok".
;  if it hangs, reset and try the next key.
;
;  1  OFF = 4        (control : different low bits)
;  2  OFF = 0        (same address, younger store)
;  3  OFF = 0x100    (the M4.csv case, bit 8)
;  4  OFF = 0x200    (bit 9)
;  5  OFF = 0x400    (bit 10)
;  6  OFF = 0x1000   (bit 12)
;  7  OFF = 0x80     (bit 7)
;  8  OFF = 0x40     (bit 6)
;  9  OFF = 0x10000  (bit 16, X+OFF via a second base register)
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


; 8 x (ld [gen[13]] ; st [gen[14] + #1]) back to back
#macro LDST 1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    ld.gen[13] gen[1].32B, 0
    st.gen[14] gen[2].32B, #1
    jmp.one zero.32B, ok
#end

.bss
area:   4*0x480                         ; X = area, X+OFF up to +0x1000
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('5')
    PUTC(' ')
    PUTC('1')
    PUTC('-')
    PUTC('9')

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
    set gen[13].32B, area
    set gen[14].32B, area
    set gen[2].32B, 0x77
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

ok:
    PUTC('o')
    PUTC('k')
    jmp.one zero.32B, menu

t1:
    LDST(4)
t2:
    LDST(0)
t3:
    LDST(0x100)
t4:
    LDST(0x200)
t5:
    LDST(0x400)
t6:
    LDST(0x1000)
t7:
    LDST(0x80)
t8:
    LDST(0x40)
t9:                                     ; gen[14] = area + 0x10000 (memory above
    LSET(gen[14], 0x10000)              ; the program is unused RAM)
    set gen[15].32B, area
    add gen[14].32B, gen[15].32B
    block
    LDST(0)
