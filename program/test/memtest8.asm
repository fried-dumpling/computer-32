; =====================================================================
;  memtest8.asm : calc restart window in fp_str2f (call -> quick ret)
;
;  build (in program/) :  dasm test/memtest8.asm -o memtest8.o
;                         dasm floatingpoint/float.asm -o float.o
;                         dlnk memtest8.o float.o -o run.txt
;
;  observation (log/ram/calc-dump.txt, stale stack frames) :
;    calc restarts (jumps to 0) inside the first fp_str2f, between
;      0xCC8  call fp_big_mul10n        ; K = 0 : mov, cmpi, jmp.zero, ret
;      0xCCC  jmp.one fp_s2f_div
;      0xCE0  call fp_bigdiv            ; never reached (its push is missing)
;    right after fp_big_set32(B, 1) (call -> call fp_big_zero -> 14 st).
;    byte access is not involved (every ld/st/push/pop is 32B).
;
;  keys (each test prints its result, then the stack pointer) :
;    1  call r0 (only ret) ; jmp.one ; call r0                 -> SP
;    2  call kz (mul10n with K = 0) ; jmp.one ; call r0         -> SP
;    3  set32 clone (14+2 st) then the same as 2, like str2f    -> SP
;    4  real fp_str2f "5"                                       -> 40A00000, SP
;  result on the circuit (2026-10-11) : 1, 2, 4 pass, 3 restarts.
;  5..9 each drop one part of 3 :
;    5  set32 only (no kz / jmp / call after it)                 -> SP
;    6  call bzero (14 st, no tail st) then kz ; jmp.one ; call  -> SP
;    7  set32 then call r0 (not kz) ; jmp.one ; call r0          -> SP
;    8  14+2 st inline (no call) then kz ; jmp.one ; call r0     -> SP
;    9  set32 then kz, then nothing                              -> SP
;    a..i  16 rounds of 1..9 without key input (line starts A..P)
;  result (6 presses each) : 1 2 5 6 8 always pass, 3 4 7 9 restart
;  sometimes. so it needs set32 followed soon by a short call -> ret,
;  and either the 2 tail stores (6 passes) or the nesting (8 passes).
;  suspicion : that ret (pops the slot of the earlier "call set32" push)
;  gets the data of another store still in the store buffer.
;    j  = 3, but every store writes the address of a trap instead of
;       0 / 1 : bzero -> trap Z, tail st +0 -> trap 0, tail st +4 ->
;       trap 4. a wrong forward then prints "!Z", "!0" or "!4" (and goes
;       back to the menu) instead of restarting. a restart or a normal SP
;       means the bad value did not come from those stores.
;    k  = 3 without the 2 tail stores (nesting only)
;    l  = 6 plus the 2 tail stores after bzero (stores only, no nesting)
;  result (6 presses each) : j k l always pass. so nesting and tail stores
;  are both needed, and j's change of the store data (immediate set
;  instead of the mov/shiftri/bndi chain) hides it.
;    m  = 3 with only gen[2] changed : trap4 << 16 | trap0 instead of 1.
;       set32 is the same code, so the tail stores write trap0 (+0) and
;       trap4 (+4) through the same ALU chain. bzero still writes 0.
;       "!0" / "!4" : the ret got that tail store's data. restart : 0
;       from bzero or from somewhere else.
;    n  = 3 with bzero storing a register that holds 0 (not zero.32B)
;  result : m 0/12 fail. n 0/6 when pressed in a row, but 5/6 when
;  alternated with m -> it depends on what ran before (id alignment ?),
;  so m (3 more instructions than 3) does not prove the value matters.
;    o / p  the same code (t_op) and the same number of committed
;       instructions before it, only the data differs. the key sets
;       gen[9] before the block, t_op then does mov gen[2], gen[9]
;       where 3 does set gen[2], 1 :
;         o : gen[9] = 1                    (= test 3)
;         p : gen[9] = trap4 << 16 | trap0  (= test m data)
;       if p restarts, the value does not matter (and nothing printed
;       "!0" / "!4" means the bad ret did not get the tail store data).
;       if only o fails, the value 0 / 1 written by the tail stores
;       matters. alternate o and p, and also press each in a row.
;  SP must be stk_end (000019DC when linked with float.o as above); any
;  other value or the banner "m8 ..." printed again (= restart from 0)
;  is a failure.
;  the first test that fails tells which part of the window is enough.
; =====================================================================
#include<../floatingpoint/float.hsm>

#define IO_BASE 0x4000000
#define KEY_IN 0
#define KEY_V 4
#define KEY_R 8
#define TERM_OUT 12
#define ROUNDS 16
#define BIGN 14

#macro LSET 2
    set #1.S16H, ((#2) >> 16) & 0xFFFF
    set #1.16L, (#2) & 0xFFFF
#end

#macro PUTC 1
    set gen[22].32B, #1
    st.gen[24] gen[22].32B, TERM_OUT
#end

#macro HEXD 1
    mov gen[22].32B, gen[1].32B
    shiftri gen[22].32B, #1
    bnd gen[22].32B, gen[21].32B
    cmpi gen[22].32B, 10
    jmp.neg zero.32B, ##num
    addi gen[22].32B, 'A'-10-'0'
##num:
    addi gen[22].32B, '0'
    st.gen[24] gen[22].32B, TERM_OUT
#end

.data
s5:     4,'5' 4,0

.bss
bigB:   4*BIGN
stk:    4*128
stk_end:

.text
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    set gen[11].32B, 0              ; auto rounds left
    set gen[10].32B, '1'            ; key used by auto rounds
    PUTC('m')
    PUTC('8')
    PUTC(' ')
    PUTC('1')
    PUTC('2')
    PUTC('3')
    PUTC('4')
    PUTC('5')
    PUTC('6')
    PUTC('7')
    PUTC('8')
    PUTC('9')
    PUTC('a')
    PUTC('-')
    PUTC('i')
    PUTC(' ')
    PUTC('j')
    PUTC('k')
    PUTC('l')
    PUTC('m')
    PUTC('n')
    PUTC('o')
    PUTC('p')

menu:
    PUTC('\n')
    PUTC('>')
    PUTC(' ')
key:
    block
    cmpi gen[11].32B, 0
    jmp.zero zero.32B, getkey
    subi gen[11].32B, 1             ; auto round : letter A..P
    set gen[22].32B, 'A'+ROUNDS-1
    sub gen[22].32B, gen[11].32B
    st.gen[24] gen[22].32B, TERM_OUT
    PUTC(' ')
    mov gen[4].32B, gen[10].32B
    jmp.one zero.32B, got
getkey:
    ld.gen[24] gen[3].32B, KEY_V
    cmp gen[3].32B, zero.32B
    jmp.zero zero.32B, key
    ld.gen[24] gen[4].32B, KEY_IN
    st.gen[24] full.32B, KEY_R
    cmpi gen[4].32B, 'j'
    jmp.zero zero.32B, got
    cmpi gen[4].32B, 'k'
    jmp.zero zero.32B, got
    cmpi gen[4].32B, 'l'
    jmp.zero zero.32B, got
    cmpi gen[4].32B, 'm'
    jmp.zero zero.32B, got
    cmpi gen[4].32B, 'n'
    jmp.zero zero.32B, got
    cmpi gen[4].32B, 'o'
    jmp.zero zero.32B, key_o
    cmpi gen[4].32B, 'p'
    jmp.zero zero.32B, key_p
    mov gen[3].32B, gen[4].32B      ; 'a'..'i' : auto rounds of '1'..'9'
    subi gen[3].32B, 'a'
    cmpi gen[3].32B, 0
    jmp.neg zero.32B, got           ; below 'a'
    cmpi gen[3].32B, 9
    ijmp.neg zero.32B, got          ; above 'i'
    mov gen[10].32B, gen[3].32B
    addi gen[10].32B, '1'
    set gen[11].32B, ROUNDS
    jmp.one zero.32B, key
got:
    st.gen[24] gen[4].32B, TERM_OUT
got_np:
    PUTC(' ')
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
    cmpi gen[4].32B, 'j'
    jmp.zero zero.32B, tj
    cmpi gen[4].32B, 'k'
    jmp.zero zero.32B, tk
    cmpi gen[4].32B, 'l'
    jmp.zero zero.32B, tl
    cmpi gen[4].32B, 'm'
    jmp.zero zero.32B, tm
    cmpi gen[4].32B, 'n'
    jmp.zero zero.32B, tn
    cmpi gen[4].32B, 'o'
    jmp.zero zero.32B, t_op
    PUTC('?')
    jmp.one zero.32B, menu

    ; print SP (must be stk_end)
show_sp:
    mov gen[1].32B, stack.32B
    call zero.32B, puthex
    jmp.one zero.32B, menu

; ---------------------------------------------------------------------
;  1 : call / ret with nothing in between, then jmp.one, call
; ---------------------------------------------------------------------
t1:
    call zero.32B, r0
    jmp.one zero.32B, t1_next
t1_next:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  2 : the fp_big_mul10n shape with K = 0 (taken jmp.zero right before ret)
; ---------------------------------------------------------------------
t2:
    set gen[2].32B, 0
    call zero.32B, kz
    jmp.one zero.32B, t2_next
t2_next:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  3 : float.asm 897-912 replayed : set32(B, 1), then 2
; ---------------------------------------------------------------------
t3:
    set gen[8].32B, 0               ; K
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, t3_kneg
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, t3_div
t3_kneg:                            ; not taken (K = 0), kept for the shape
    set gen[1].32B, bigB
    set gen[2].32B, 0
    sub gen[2].32B, gen[8].32B
    call zero.32B, kz
t3_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  4 : the real library call, as in calc / selftest W
; ---------------------------------------------------------------------
t4:
    set gen[1].32B, s5
    call zero.32B, fp_str2f
    mov gen[1].32B, gen[0].32B
    call zero.32B, puthex
    PUTC(' ')
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  5..9 : test 3 with one part removed
; ---------------------------------------------------------------------
t5:                                 ; set32 only
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32
    jmp.one zero.32B, show_sp

t6:                                 ; bzero (no tail stores), then kz shape
    set gen[8].32B, 0
    set gen[1].32B, bigB
    call zero.32B, bzero
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, t6_div
t6_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

t7:                                 ; set32, then r0 instead of kz
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, r0
    jmp.one zero.32B, t7_div
t7_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

t8:                                 ; the 14+2 stores inline, no call
    set gen[8].32B, 0
    set gen[1].32B, bigB
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, BIGN
t8_lp:
    st.gen[13] zero.32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, t8_lp
    set gen[13].32B, 1
    st.gen[1] gen[13].32B, 0
    set gen[13].32B, 0
    st.gen[1] gen[13].32B, 4
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, t8_div
t8_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

t9:                                 ; set32, then kz, then nothing
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  j : 3 with trap addresses as store data
; ---------------------------------------------------------------------
tj:
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32t
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, tj_div
tj_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

trapZ:
    PUTC('!')
    PUTC('Z')
    jmp.one zero.32B, trap
trap0:
    PUTC('!')
    PUTC('0')
    jmp.one zero.32B, trap
trap4:
    PUTC('!')
    PUTC('4')
trap:
    PUTC(' ')
    mov gen[1].32B, stack.32B       ; SP when the bad ret jumped here
    call zero.32B, puthex
    set stack.32B, stk_end
    jmp.one zero.32B, menu

; ---------------------------------------------------------------------
;  k : 3 without the tail stores    l : 6 plus the tail stores
; ---------------------------------------------------------------------
tk:
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32n
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, tk_div
tk_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

tl:
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, bzero
    mov gen[13].32B, gen[2].32B
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 0
    mov gen[13].32B, gen[2].32B
    shiftri gen[13].32B, 16
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 4
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, tl_div
tl_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  m : 3 with gen[2] = trap4 << 16 | trap0    n : 3 with bzero0
; ---------------------------------------------------------------------
tm:
    set gen[2].32B, trap4
    shiftli gen[2].32B, 16
    bori gen[2].32B, trap0
    set gen[8].32B, 0
    set gen[1].32B, bigB
    call zero.32B, set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, tm_div
tm_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

tn:
    set gen[8].32B, 0
    set gen[1].32B, bigB
    set gen[2].32B, 1
    call zero.32B, set32z
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, tn_div
tn_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  o / p : same instructions, only gen[9] differs
; ---------------------------------------------------------------------
key_o:                              ; 3 instructions, like key_p
    set gen[9].32B, 0
    shiftli gen[9].32B, 16
    bori gen[9].32B, 1
    jmp.one zero.32B, key_op
key_p:
    set gen[9].32B, trap4
    shiftli gen[9].32B, 16
    bori gen[9].32B, trap0
    jmp.one zero.32B, key_op
key_op:
    st.gen[24] gen[4].32B, TERM_OUT
    set gen[4].32B, 'o'             ; both run t_op
    jmp.one zero.32B, got_np

t_op:                               ; = t3, set gen[2], 1 -> mov from gen[9]
    set gen[8].32B, 0
    set gen[1].32B, bigB
    mov gen[2].32B, gen[9].32B
    call zero.32B, set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, show_sp
    set gen[1].32B, bigB
    mov gen[2].32B, gen[8].32B
    call zero.32B, kz
    jmp.one zero.32B, t_op_div
t_op_div:
    call zero.32B, r0
    jmp.one zero.32B, show_sp

; ---------------------------------------------------------------------
;  callees (copies of the float.asm shapes, gen[13..18] destroyed)
; ---------------------------------------------------------------------
r0:
    ret

kz:                                 ; = fp_big_mul10n
    mov gen[18].32B, gen[2].32B
kz_lp:
    cmpi gen[18].32B, 0
    jmp.zero zero.32B, kz_end
    call zero.32B, r0
    subi gen[18].32B, 1
    jmp.one zero.32B, kz_lp
kz_end:
    ret

bzero:                              ; = fp_big_zero
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, BIGN
bzero_lp:
    st.gen[13] zero.32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, bzero_lp
    ret

set32:                              ; = fp_big_set32
    call zero.32B, bzero
    mov gen[13].32B, gen[2].32B
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 0
    mov gen[13].32B, gen[2].32B
    shiftri gen[13].32B, 16
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 4
    ret

bzerot:                             ; = bzero, but stores trapZ
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, BIGN
    set gen[15].32B, trapZ
bzerot_lp:
    st.gen[13] gen[15].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, bzerot_lp
    ret

set32t:                             ; = set32, same length, trap data
    call zero.32B, bzerot
    set gen[13].32B, trap0
    nop
    st.gen[1] gen[13].32B, 0
    set gen[13].32B, trap4
    nop
    nop
    st.gen[1] gen[13].32B, 4
    ret

bzero0:                             ; = bzero, storing a register = 0
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, BIGN
    set gen[15].32B, 0
bzero0_lp:
    st.gen[13] gen[15].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, bzero0_lp
    ret

set32z:                             ; = set32 calling bzero0
    call zero.32B, bzero0
    mov gen[13].32B, gen[2].32B
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 0
    mov gen[13].32B, gen[2].32B
    shiftri gen[13].32B, 16
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 4
    ret

set32n:                             ; = set32 without the 2 stores
    call zero.32B, bzero
    mov gen[13].32B, gen[2].32B
    bndi gen[13].32B, 0xFFFF
    nop
    mov gen[13].32B, gen[2].32B
    shiftri gen[13].32B, 16
    bndi gen[13].32B, 0xFFFF
    nop
    ret

puthex:                             ; gen[1] as 8 hex digits
    set gen[21].32B, 0xF
    HEXD(28)
    HEXD(24)
    HEXD(20)
    HEXD(16)
    HEXD(12)
    HEXD(8)
    HEXD(4)
    HEXD(0)
    ret
