; =====================================================================
;  memtest7.asm : id-manager epoch restore when a flushing branch has id F
;
;  build (in program/) :  dasm test/memtest7.asm -o memtest7.o
;                         dlnk memtest7.o -o run.txt
;
;  hypothesis (log/compare/deadlock-investigation.md, M4-7.csv rows
;  2971/2975/2983) :
;    the "call puthex" flush in memtest4 test 7 happens while
;    halt-FT-DC-ISS = 1 and another instruction is dispatched in the same
;    cycle. when that call has id F, the alloc pointer is restored to 0
;    but the epoch is not flipped (new id 10 instead of 00). the next
;    flush (loop ijmp) then gets an empty CLR_inst, the wrong path
;    commits and the cpu stops.
;    in memtest4 every press of 7 commits 2 (mod 16) ids, so the call
;    lands on id F only once every 8 presses.
;
;  keys :
;    7  one round of memtest4 test 7 (same code path)       -> 66
;    8  the same with one extra committed instruction       -> 66
;    a  16 rounds of 7 without key input
;    b  16 rounds of 8 without key input
;    while running a/b each line starts with the round letter A..P.
;
;  an auto round of 8 commits 3 ids more than one of 7 (cmpi '8',
;  jmp.zero t8, addi), so one of a/b commits an odd number of ids per
;  round. 16 rounds of that one put the call on every id 0..F once, and
;  the hypothesis says it must stop there.
;  the round it stops on should be the one whose call got id F; check
;  inst_ID of the call (opcode 0x70, "call zero, puthex") in the dump.
;  if 16 rounds of both a and b finish, the hypothesis is wrong
;  (or the call did not flush with halt = 1 in this layout, see the dump).
;  reset the cpu between a and b runs so both start from the same state.
; =====================================================================
#define IO_BASE 0x4000000
#define KEY_IN 0
#define KEY_V 4
#define KEY_R 8
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
    set gen[12].32B, 0              ; auto rounds left
    set gen[13].32B, '7'            ; key used by auto rounds
    PUTC('m')
    PUTC('7')
    PUTC(' ')
    PUTC('7')
    PUTC('8')
    PUTC('a')
    PUTC('b')

menu:
    PUTC('\n')
    PUTC('>')
    PUTC(' ')
key:
    block
    cmpi gen[12].32B, 0
    jmp.zero zero.32B, getkey
    ; auto round : print the round letter (A = 1st .. P = 16th), then act
    ; as if gen[13] was pressed. no branch here, so every round commits
    ; the same number of instructions.
    subi gen[12].32B, 1
    set gen[20].32B, 'A'+ROUNDS-1
    sub gen[20].32B, gen[12].32B
    st.gen[24] gen[20].32B, TERM_OUT
    PUTC(' ')
    mov gen[4].32B, gen[13].32B
    jmp.one zero.32B, got
getkey:
    ld.gen[24] gen[3].32B, KEY_V
    cmp gen[3].32B, zero.32B
    jmp.zero zero.32B, key
    ld.gen[24] gen[4].32B, KEY_IN
    st.gen[24] full.32B, KEY_R
    cmpi gen[4].32B, 'a'
    jmp.zero zero.32B, auto7
    cmpi gen[4].32B, 'b'
    jmp.zero zero.32B, auto8
got:
    ; from here on the same path as memtest4 (key '7')
    st.gen[24] gen[4].32B, TERM_OUT
    PUTC(' ')
    set gen[10].32B, 16
    st.zero zero.32B, slotA
    st.zero zero.32B, slotB
    block
    cmpi gen[4].32B, '1'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '2'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '3'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '4'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '5'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '6'
    jmp.zero zero.32B, other
    cmpi gen[4].32B, '7'
    jmp.zero zero.32B, t7
    cmpi gen[4].32B, '8'
    jmp.zero zero.32B, t8
other:
    PUTC('?')
    jmp.one zero.32B, menu

auto7:
    set gen[13].32B, '7'
    set gen[12].32B, ROUNDS
    jmp.one zero.32B, key
auto8:
    set gen[13].32B, '8'
    set gen[12].32B, ROUNDS
    jmp.one zero.32B, key

show:
    call zero.32B, puthex
    jmp.one zero.32B, menu

t8:
    addi gen[11].32B, 1             ; the one extra committed id
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
