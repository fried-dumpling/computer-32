; =====================================================================
;  stackmov.asm : a normal register write to `stack` (mov / set / ld /
;                 addi) next to push / pop / call, which update stack on
;                 their own path (errors.md 59 had the same kind of
;                 priority problem for flag).
;
;  build (in program/) :  dasm test/memory/stackmov.asm -o stackmov.o
;                         dlnk stackmov.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh stackmov  (compares with the emulator)
;
;  output per case: "<case> <stack> <w0> <w1> <reg>" in hex, then "E".
;  w0, w1 = the words at T-4 and T-8 (T = top of a scratch area), reg =
;  the register a pop / call / load wrote. every case clears the area,
;  runs, then sets stack back to the program's own stack.
;    a : set stack, T ; push V                 (set then push)
;    b : mov stack, T ; push V
;    c : mov stack, T (T late, DELAY) ; push V
;    d : push V ; mov stack, T                 (push then mov)
;    e : pop R ; mov stack, T
;    f : mov stack, T-8 ; pop R                (R = mem[T-8])
;    g : mov stack, T ; call retsub              (mem[T-4] = return)
;    h : ld stack, ptr (ptr = T) ; push V
;    i : mov stack, T ; subi stack, 8 ; push V
;    j : push V ; push V ; mov stack, T ; push V
;    k : like b without DELAY before
;    l : mov stack, T ; pop R ; push V         (R = mem[T])
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

; CASE(name) : clear the area, reset R, let the pipeline drain, print the name
#macro CASE 1
    st.zero zero.32B, area+56
    st.zero zero.32B, area+60
    st.zero zero.32B, area+64
    set gen[10].32B, 0
    set gen[7].32B, 40
##wait:
    subi gen[7].32B, 1
    cmpi gen[7].32B, 0
    jmp.pos zero.32B, ##wait
    PUTC(#1)
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

; SHOW : print stack, mem[T-4], mem[T-8], R; then stack = own stack
#macro SHOW 0
    mov gen[11].32B, stack.32B
    set stack.32B, stk_end
    call zero.32B, show
#end

.data
slow:   4, 0
ptr:    4, area+64

.bss
area:   4*17
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('s')
    PUTC('t')
    PUTC('k')
    PUTC('\n')
    set gen[5].32B, area+64            ; T
    LSET(gen[6], 0xA5A5C3C3)           ; V

    CASE('a')
    DELAY
    set stack.32B, area+64
    push gen[6].32B
    SHOW

    CASE('b')
    DELAY
    mov stack.32B, gen[5].32B
    push gen[6].32B
    SHOW

    CASE('c')
    DELAY
    mov gen[12].32B, gen[5].32B
    add gen[12].32B, gen[9].32B
    mov stack.32B, gen[12].32B
    push gen[6].32B
    SHOW

    CASE('d')
    DELAY
    push gen[6].32B
    mov stack.32B, gen[5].32B
    SHOW

    CASE('e')
    DELAY
    pop gen[10].32B
    mov stack.32B, gen[5].32B
    SHOW

    CASE('f')
    st.zero gen[6].32B, area+56
    DELAY
    mov gen[12].32B, gen[5].32B
    subi gen[12].32B, 8
    mov stack.32B, gen[12].32B
    pop gen[10].32B
    SHOW

    CASE('g')
    DELAY
    mov stack.32B, gen[5].32B
    call zero.32B, retsub
    SHOW

    CASE('h')
    DELAY
    ld.zero stack.32B, ptr
    push gen[6].32B
    SHOW

    CASE('i')
    DELAY
    mov stack.32B, gen[5].32B
    subi stack.32B, 8
    push gen[6].32B
    SHOW

    CASE('j')
    DELAY
    push gen[6].32B
    push gen[6].32B
    mov stack.32B, gen[5].32B
    push gen[6].32B
    SHOW

    CASE('k')
    mov stack.32B, gen[5].32B
    push gen[6].32B
    SHOW

    CASE('l')
    st.zero gen[6].32B, area+64
    DELAY
    mov stack.32B, gen[5].32B
    pop gen[10].32B
    push gen[6].32B
    SHOW

    PUTC('E')
    PUTC('\n')
    halt
    jmp.one pc.32B, 0

; retsub : R = 0x5B
retsub:
    set gen[10].32B, 0x5B
    ret

; show : " <stack> <mem[T-4]> <mem[T-8]> <R>\n"
show:
    mov gen[1].32B, gen[11].32B
    call zero.32B, puthex
    ld.zero gen[1].32B, area+60
    call zero.32B, puthex
    ld.zero gen[1].32B, area+56
    call zero.32B, puthex
    mov gen[1].32B, gen[10].32B
    call zero.32B, puthex
    PUTC('\n')
    ret

puthex:
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
    ret
