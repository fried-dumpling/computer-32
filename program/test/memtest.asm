; =====================================================================
;  memtest.asm : store -> load ordering tests for the circuit
;
;  build (in program/) :  dasm test/memtest.asm -o memtest.o
;                         dlnk memtest.o -o run.txt
;
;  output : <letter> <error count in hex>   (00000000 = ok)
;  the letter is printed BEFORE the test, so a hang or restart points
;  at the test. every test repeats ROUNDS times (timing dependent bugs).
;  the harness only uses instructions already proven on the circuit.
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12
#define ROUNDS 16

#macro LSET 2
    set #1.S16H, ((#2) >> 16) & 0xFFFF
    set #1.16L, (#2) & 0xFFFF
#end

#macro PUTC 1
    set gen[20].32B, #1
    st.gen[24] gen[20].32B, TERM_OUT
#end

#macro BEGIN 1
    PUTC('\n')
    PUTC(#1)
    PUTC(' ')
    set gen[11].32B, 0                  ; error count
    set gen[10].32B, ROUNDS
#end

; gen[11]++ if gen[1] != gen[2]
#macro CHECK 0
    cmp gen[1].32B, gen[2].32B
    jmp.zero zero.32B, ##ok
    addi gen[11].32B, 1
##ok:
#end

; loop back to #1 while --gen[10] != 0, then print the error count
#macro REPEAT 1
    subi gen[10].32B, 1
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, #1
    mov gen[1].32B, gen[11].32B
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

.bss
slot:   4
arr:    4*16
far:    4*0x1100                        ; for alias tests (up to +0x4000)
stk:    4*256
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('m')
    PUTC('e')
    PUTC('m')

    ; ---- a : st then ld, same address, back to back ----
    BEGIN('a')
ta:
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x100
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    CHECK()
    REPEAT(ta)

    ; ---- b : same address through two different base registers ----
    BEGIN('b')
tb:
    set gen[13].32B, slot
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x200
    st.gen[13] gen[2].32B, 0
    ld.zero gen[1].32B, slot
    CHECK()
    REPEAT(tb)

    ; ---- c..f : store to X, store to X+d, load X  (d = 0x40 .. 0x4000) ----
    ; a wrong match on partial address bits returns the second value
    BEGIN('c')
tc:
    set gen[13].32B, far
    set gen[2].32B, 0x1111
    set gen[3].32B, 0x2222
    st.gen[13] gen[2].32B, 0
    st.gen[13] gen[3].32B, 0x40
    ld.gen[13] gen[1].32B, 0
    CHECK()
    REPEAT(tc)
    BEGIN('d')
td:
    set gen[13].32B, far
    set gen[2].32B, 0x1111
    set gen[3].32B, 0x2222
    st.gen[13] gen[2].32B, 0
    st.gen[13] gen[3].32B, 0x400
    ld.gen[13] gen[1].32B, 0
    CHECK()
    REPEAT(td)
    BEGIN('e')
te:
    set gen[13].32B, far
    set gen[2].32B, 0x1111
    set gen[3].32B, 0x2222
    st.gen[13] gen[2].32B, 0
    st.gen[13] gen[3].32B, 0x1000
    ld.gen[13] gen[1].32B, 0
    CHECK()
    REPEAT(te)
    BEGIN('f')
tf:
    set gen[13].32B, far
    set gen[2].32B, 0x1111
    set gen[3].32B, 0x2222
    st.gen[13] gen[2].32B, 0
    st.gen[13] gen[3].32B, 0x4000
    ld.gen[13] gen[1].32B, 0
    CHECK()
    REPEAT(tf)

    ; ---- g : st X, then 14 stores of 0 elsewhere (loop), then ld X ----
    BEGIN('g')
tg:
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x300
    st.zero gen[2].32B, slot
    call zero.32B, zero14
    ld.zero gen[1].32B, slot
    CHECK()
    REPEAT(tg)

    ; ---- h : st X, 14 stores of 0 unrolled (no call), ld X ----
    BEGIN('h')
th:
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x400
    st.zero gen[2].32B, slot
    set gen[13].32B, arr
    st.gen[13] zero.32B, 0
    st.gen[13] zero.32B, 4
    st.gen[13] zero.32B, 8
    st.gen[13] zero.32B, 12
    st.gen[13] zero.32B, 16
    st.gen[13] zero.32B, 20
    st.gen[13] zero.32B, 24
    st.gen[13] zero.32B, 28
    st.gen[13] zero.32B, 32
    st.gen[13] zero.32B, 36
    st.gen[13] zero.32B, 40
    st.gen[13] zero.32B, 44
    st.gen[13] zero.32B, 48
    st.gen[13] zero.32B, 52
    ld.zero gen[1].32B, slot
    CHECK()
    REPEAT(th)

    ; ---- i : fp_str2f prologue : 8 pushes, call zero14, 8 pops ----
    ; (a wrong ret value shows up as a restart or a hang here)
    BEGIN('i')
ti:
    set gen[5].32B, 0x55
    call zero.32B, save_call
    mov gen[1].32B, gen[5].32B
    set gen[2].32B, 0x55
    CHECK()
    REPEAT(ti)

    ; ---- j : read-modify-write loop over an array (bigint mul10) ----
    ;          arr[k] = arr[k] + 1, 14 words, then sum must be 14 * rounds
    BEGIN('j')
    call zero.32B, zero14
    set gen[12].32B, 0                  ; expected sum
tj:
    set gen[13].32B, arr
    set gen[14].32B, 14
tj_lp:
    ld.gen[13] gen[16].32B, 0
    addi gen[16].32B, 1
    st.gen[13] gen[16].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, tj_lp
    addi gen[12].32B, 14
    set gen[13].32B, arr                ; sum the array
    set gen[14].32B, 14
    set gen[1].32B, 0
tj_sum:
    ld.gen[13] gen[16].32B, 0
    add gen[1].32B, gen[16].32B
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, tj_sum
    mov gen[2].32B, gen[12].32B
    CHECK()
    REPEAT(tj)

    PUTC('\n')
    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
    halt
done:
    jmp.one zero.32B, done

; ---------------------------------------------------------------------
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

    ; arr[0..13] = 0, like fp_big_zero. destroys gen[13], gen[14]
zero14:
    set gen[13].32B, arr
    set gen[14].32B, 14
zero14_lp:
    st.gen[13] zero.32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, zero14_lp
    ret

    ; like FP_SAVE / FP_RESTORE : push gen[5..12], overwrite them,
    ; call zero14, pop them back. the caller's values must survive
save_call:
    push gen[5].32B
    push gen[6].32B
    push gen[7].32B
    push gen[8].32B
    push gen[9].32B
    push gen[10].32B
    push gen[11].32B
    push gen[12].32B
    set gen[5].32B, 0xEE
    set gen[6].32B, 0xEE
    set gen[7].32B, 0xEE
    set gen[8].32B, 0xEE
    set gen[9].32B, 0xEE
    set gen[10].32B, 0xEE
    set gen[11].32B, 0xEE
    set gen[12].32B, 0xEE
    call zero.32B, zero14
    pop gen[12].32B
    pop gen[11].32B
    pop gen[10].32B
    pop gen[9].32B
    pop gen[8].32B
    pop gen[7].32B
    pop gen[6].32B
    pop gen[5].32B
    ret
