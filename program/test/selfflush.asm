; =====================================================================
;  selfflush.asm : loads that pass an older store to the same address
;                  (self flush, document/errors.md #11)
;
;  build (in program/) :  dasm test/selfflush.asm -o selfflush.o
;                         dlnk selfflush.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh selfflush  (compares with the emulator)
;
;  every case stores a value whose data or address is late (dependent
;  chain or a load), then loads the same address right away. the load
;  runs first with the old memory value, the store resolves, and the
;  circuit self flushes at the load. the old load id still commits (with
;  the old value) and the load is fetched again as a new id. these cases
;  check that the old value never becomes visible.
;  output : <letter> <error count hex>  (00000000 = ok), then "end"
;    a : st (data late, addi chain) ; ld X                   (= memtest a)
;    b : st (data from a load) ; ld X
;    c : st (address late) ; ld X
;    d : st (data late) ; ld.gen13 gen13 (load dest = its base register).
;        old X = &decoy, so a reload through the old gen13 reads decoy
;    e : st (data late, = &tgt) ; ld gen13, X ; ld gen1, [gen13]
;        (dependent load through the self flushed load)
;    f : push (data late) ; pop  -> value and stack pointer
;    g : st (late) at [stack] = address of g_ok ; ret  (ret is a load)
;    h : st (data late) ; ld gen1, X ; ld gen3, X
;    i : st (data late) ; ld gen1, X ; st gen1, Y ; ld gen3, Y
;    j : st.stack (data late) ; pop
;    k : st.stack (data late) ; ld.stack
;    l : st.stack (data late) ; push ; (delay) ; ld the store's address.
;        no load near the store: checks that the store uses the stack
;        value before the younger push
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12
#define ROUNDS 16
#define DECOY 0x0BAD0BAD
#define TGT 0x7A7A0000

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

; gen[11]++ if #1 != #2
#macro CHECK 2
    cmp #1, #2
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

; LATE(reg, add) : reg = gen[10] + add through a short dependent chain
#macro LATE 2
    mov #1, gen[10].32B
    addi #1, #2
    addi #1, 0
    addi #1, 0
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
slot2:  4
src:    4
decoy:  4
tgt:    4
stk:    4*256
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    LSET(gen[7], DECOY)
    st.zero gen[7].32B, decoy
    LSET(gen[7], TGT)
    st.zero gen[7].32B, tgt
    PUTC('s')
    PUTC('f')

    ; ---- a : data late ----
    BEGIN('a')
ta:
    LATE(gen[2].32B, 0x100)
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    CHECK(gen[1].32B, gen[2].32B)
    REPEAT(ta)

    ; ---- b : data from a load ----
    BEGIN('b')
tb:
    mov gen[3].32B, gen[10].32B
    addi gen[3].32B, 0x200
    st.zero gen[3].32B, src
    ld.zero gen[2].32B, src
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    CHECK(gen[1].32B, gen[3].32B)
    REPEAT(tb)

    ; ---- c : address late ----
    BEGIN('c')
tc:
    mov gen[2].32B, gen[10].32B
    addi gen[2].32B, 0x300
    mov gen[13].32B, gen[10].32B
    sub gen[13].32B, gen[10].32B
    addi gen[13].32B, slot
    st.gen[13] gen[2].32B, 0
    ld.zero gen[1].32B, slot
    CHECK(gen[1].32B, gen[2].32B)
    REPEAT(tc)

    ; ---- d : load dest = base register ----
    BEGIN('d')
td:
    set gen[3].32B, decoy
    st.zero gen[3].32B, slot            ; old X = &decoy
    set gen[13].32B, slot
    LATE(gen[2].32B, 0x400)
    st.zero gen[2].32B, slot
    ld.gen[13] gen[13].32B, 0
    CHECK(gen[13].32B, gen[2].32B)
    REPEAT(td)

    ; ---- e : dependent load through the self flushed load ----
    BEGIN('e')
te:
    set gen[3].32B, decoy
    st.zero gen[3].32B, slot            ; old X = &decoy
    LATE(gen[2].32B, tgt)
    subi gen[2].32B, 0
    sub gen[2].32B, gen[10].32B         ; gen[2] = tgt (late)
    st.zero gen[2].32B, slot
    ld.zero gen[13].32B, slot
    ld.gen[13] gen[1].32B, 0
    LSET(gen[3], TGT)
    CHECK(gen[1].32B, gen[3].32B)
    REPEAT(te)

    ; ---- f : push (late) ; pop ----
    BEGIN('f')
tf:
    mov gen[8].32B, stack.32B
    LATE(gen[2].32B, 0x600)
    push gen[2].32B
    pop gen[1].32B
    CHECK(gen[1].32B, gen[2].32B)
    mov gen[3].32B, stack.32B
    CHECK(gen[3].32B, gen[8].32B)
    set stack.32B, stk_end              ; keep going after an error
    REPEAT(tf)

    ; ---- g : return address overwritten (late) ; ret ----
    BEGIN('g')
tg:
    call zero.32B, g_fn
    addi gen[11].32B, 1                 ; returned to the old address
    jmp.one zero.32B, g_done
g_ok:
    set stack.32B, stk_end
g_done:
    REPEAT(tg)

    ; ---- h : two loads ----
    BEGIN('h')
th:
    LATE(gen[2].32B, 0x800)
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    ld.zero gen[3].32B, slot
    CHECK(gen[1].32B, gen[2].32B)
    CHECK(gen[3].32B, gen[2].32B)
    REPEAT(th)

    ; ---- i : loaded value stored again ----
    BEGIN('i')
ti:
    LATE(gen[2].32B, 0x900)
    st.zero gen[2].32B, slot
    ld.zero gen[1].32B, slot
    st.zero gen[1].32B, slot2
    ld.zero gen[3].32B, slot2
    CHECK(gen[3].32B, gen[2].32B)
    REPEAT(ti)

    ; ---- j : st.stack (late) ; pop ----
    BEGIN('j')
tj:
    push zero.32B
    LATE(gen[2].32B, 0xA00)
    st.stack gen[2].32B, 0
    pop gen[1].32B
    CHECK(gen[1].32B, gen[2].32B)
    set stack.32B, stk_end
    REPEAT(tj)

    ; ---- k : st.stack (late) ; ld.stack ----
    BEGIN('k')
tk:
    push zero.32B
    LATE(gen[2].32B, 0xB00)
    st.stack gen[2].32B, 0
    ld.stack gen[1].32B, 0
    CHECK(gen[1].32B, gen[2].32B)
    set stack.32B, stk_end
    REPEAT(tk)

    ; ---- l : st.stack (late) ; push (younger stack change), no load ----
    BEGIN('l')
tl:
    set stack.32B, stk_end-8
    st.zero zero.32B, stk_end-8
    LATE(gen[2].32B, 0xC00)
    st.stack gen[2].32B, 0              ; must go to stk_end-8
    push gen[10].32B                    ; stk_end-12
    set gen[3].32B, 40
l_wait:
    subi gen[3].32B, 1
    cmpi gen[3].32B, 0
    ijmp.zero zero.32B, l_wait
    ld.zero gen[1].32B, stk_end-8
    CHECK(gen[1].32B, gen[2].32B)
    set stack.32B, stk_end
    REPEAT(tl)

    PUTC('\n')
    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
    jmp.one pc.32B, 0

; g_fn : overwrite the return address with g_ok (late), then ret
g_fn:
    LATE(gen[2].32B, g_ok)
    sub gen[2].32B, gen[10].32B
    st.stack gen[2].32B, 0
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
