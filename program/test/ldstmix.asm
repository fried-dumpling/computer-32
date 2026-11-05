; =====================================================================
;  ldstmix.asm : many outstanding loads mixed with stores
;
;  build (in program/) :  dasm test/ldstmix.asm -o ldstmix.o
;                         dlnk ldstmix.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh ldstmix  (compares with the emulator)
;
;  bug (2026-11-05): the bus controller mid register (Register@1460,2520)
;  only takes LDB_MID and the store buffer has no mid output, so a store on
;  the bus showed a stale load mid (mid 8 overlap seen). the store response
;  is turned off for now; this test is for when it comes back.
;
;  table A[0..31] = V0 + i*K is written at the start. every case runs ROUNDS
;  rounds with p = &A[round] and checks every loaded value.
;  output : "<case> PASS" or "<case> FAIL <error count hex>", then "end".
;    m1 : 8 loads back to back (A[p..p+7]), then check
;    m2 : ld A / st B interleaved (different addresses), 8 deep
;    m3 : st C[j] ; ld A ; ld C[j] (store to load forwarding between loads)
;    m4 : ld A[j] ; st it to B[j] ; ld B[j] (store data from a load)
;    m5 : B[] from m4 read back and checked after the loop
;    m6 : pointer chase through D[] with a store between every load
; =====================================================================
#define IO_BASE 0x4000000
#define TERM_OUT 12
#define V0 0x5A000011
#define K 0x01030507
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

; CHK(reg) : reg must be gen[3] (the expected value), else gen[9] += 1.
; then gen[3] += K (expected value of the next table word)
#macro CHK 1
    cmp #1, gen[3].32B
    jmp.zero zero.32B, ##ok
    addi gen[9].32B, 1
##ok:
    add gen[3].32B, gen[4].32B
#end

; CHKV(reg, value) : reg must be value
#macro CHKV 2
    cmpi #1, #2
    jmp.zero zero.32B, ##ok
    addi gen[9].32B, 1
##ok:
#end

; BEGIN(c) : print the name, round counter gen[6] = 0, errors gen[9] = 0
#macro BEGIN 1
    PUTC('m')
    PUTC(#1)
    set gen[6].32B, 0
    set gen[9].32B, 0
#end

; ROUND : p = gen[5] = &A[gen[6]], gen[3] = A[gen[6]]
#macro ROUND 0
    mov gen[5].32B, gen[6].32B
    shiftli gen[5].32B, 2
    addi gen[5].32B, tabA
    call zero.32B, expect
#end

; NEXT(label) : next round
#macro NEXT 1
    addi gen[6].32B, 1
    cmpi gen[6].32B, ROUNDS
    jmp.neg zero.32B, #1
#end

.bss
tabA:   4*32
tabB:   4*32
tabC:   4*8
tabD:   4*16
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    LSET(gen[4], K)
    PUTC('l')
    PUTC('s')
    PUTC('\n')

    ; A[i] = V0 + i*K, B[i] = 0
    LSET(gen[3], V0)
    set gen[5].32B, tabA
    set gen[6].32B, 32
init:
    st.gen[5] gen[3].32B, 0
    st.gen[5] zero.32B, tabB-tabA
    add gen[3].32B, gen[4].32B
    addi gen[5].32B, 4
    subi gen[6].32B, 1
    cmpi gen[6].32B, 0
    ijmp.zero zero.32B, init
    ; D[i] = &D[(i*5+3) & 15], 16 links in one cycle (5 is odd)
    set gen[6].32B, 0
initd:
    mov gen[1].32B, gen[6].32B
    shiftli gen[1].32B, 2
    add gen[1].32B, gen[6].32B
    addi gen[1].32B, 3
    bndi gen[1].32B, 15
    shiftli gen[1].32B, 2
    addi gen[1].32B, tabD
    mov gen[2].32B, gen[6].32B
    shiftli gen[2].32B, 2
    st.gen[2] gen[1].32B, tabD
    addi gen[6].32B, 1
    cmpi gen[6].32B, 16
    jmp.neg zero.32B, initd

    ; ---- m1 : 8 loads back to back ----
    BEGIN('1')
m1:
    ROUND
    ld.gen[5] gen[13].32B, 0
    ld.gen[5] gen[14].32B, 4
    ld.gen[5] gen[15].32B, 8
    ld.gen[5] gen[16].32B, 12
    ld.gen[5] gen[17].32B, 16
    ld.gen[5] gen[18].32B, 20
    ld.gen[5] gen[19].32B, 24
    ld.gen[5] gen[10].32B, 28
    CHK(gen[13].32B)
    CHK(gen[14].32B)
    CHK(gen[15].32B)
    CHK(gen[16].32B)
    CHK(gen[17].32B)
    CHK(gen[18].32B)
    CHK(gen[19].32B)
    CHK(gen[10].32B)
    NEXT(m1)
    call zero.32B, verdict

    ; ---- m2 : ld A / st B interleaved, different addresses ----
    BEGIN('2')
m2:
    ROUND
    ld.gen[5] gen[13].32B, 0
    st.gen[5] gen[6].32B, tabB-tabA
    ld.gen[5] gen[14].32B, 4
    st.gen[5] gen[6].32B, tabB-tabA+4
    ld.gen[5] gen[15].32B, 8
    st.gen[5] gen[6].32B, tabB-tabA+8
    ld.gen[5] gen[16].32B, 12
    st.gen[5] gen[6].32B, tabB-tabA+12
    ld.gen[5] gen[17].32B, 16
    st.gen[5] gen[6].32B, tabB-tabA+16
    ld.gen[5] gen[18].32B, 20
    st.gen[5] gen[6].32B, tabB-tabA+20
    ld.gen[5] gen[19].32B, 24
    st.gen[5] gen[6].32B, tabB-tabA+24
    ld.gen[5] gen[10].32B, 28
    st.gen[5] gen[6].32B, tabB-tabA+28
    CHK(gen[13].32B)
    CHK(gen[14].32B)
    CHK(gen[15].32B)
    CHK(gen[16].32B)
    CHK(gen[17].32B)
    CHK(gen[18].32B)
    CHK(gen[19].32B)
    CHK(gen[10].32B)
    NEXT(m2)
    ; B[i] = i for i = 15+7 down to 0: the last round that wrote it
    set gen[6].32B, 0
m2b:
    mov gen[2].32B, gen[6].32B
    shiftli gen[2].32B, 2
    ld.gen[2] gen[1].32B, tabB
    mov gen[3].32B, gen[6].32B          ; expected: min(i, ROUNDS-1)
    cmpi gen[3].32B, ROUNDS
    jmp.neg zero.32B, m2c
    set gen[3].32B, ROUNDS-1
m2c:
    cmp gen[1].32B, gen[3].32B
    jmp.zero zero.32B, m2d
    addi gen[9].32B, 1
m2d:
    addi gen[6].32B, 1
    cmpi gen[6].32B, ROUNDS+7
    jmp.neg zero.32B, m2b
    call zero.32B, verdict

    ; ---- m3 : st C[j] ; ld A ; ld C[j] ----
    BEGIN('3')
m3:
    ROUND
    mov gen[11].32B, gen[6].32B
    shiftli gen[11].32B, 4              ; C values: round*16 + j
    st.zero gen[11].32B, tabC
    ld.gen[5] gen[13].32B, 0
    ld.zero gen[0].32B, tabC
    addi gen[11].32B, 1
    st.zero gen[11].32B, tabC+4
    ld.gen[5] gen[14].32B, 4
    ld.zero gen[1].32B, tabC+4
    addi gen[11].32B, 1
    st.zero gen[11].32B, tabC+8
    ld.gen[5] gen[15].32B, 8
    ld.zero gen[2].32B, tabC+8
    addi gen[11].32B, 1
    st.zero gen[11].32B, tabC+12
    ld.gen[5] gen[16].32B, 12
    ld.zero gen[12].32B, tabC+12
    CHK(gen[13].32B)
    CHK(gen[14].32B)
    CHK(gen[15].32B)
    CHK(gen[16].32B)
    sub gen[11].32B, gen[12].32B        ; C[3] = gen[11]
    CHKV(gen[11].32B, 0)
    mov gen[11].32B, gen[6].32B
    shiftli gen[11].32B, 4
    sub gen[0].32B, gen[11].32B
    CHKV(gen[0].32B, 0)
    sub gen[1].32B, gen[11].32B
    CHKV(gen[1].32B, 1)
    sub gen[2].32B, gen[11].32B
    CHKV(gen[2].32B, 2)
    NEXT(m3)
    call zero.32B, verdict

    ; ---- m4 : ld A[j] ; st it to B[j] ; ld B[j] ----
    BEGIN('4')
m4:
    ROUND
    ld.gen[5] gen[13].32B, 0
    st.gen[5] gen[13].32B, tabB-tabA
    ld.gen[5] gen[14].32B, 4
    st.gen[5] gen[14].32B, tabB-tabA+4
    ld.gen[5] gen[15].32B, tabB-tabA
    ld.gen[5] gen[16].32B, 8
    st.gen[5] gen[16].32B, tabB-tabA+8
    ld.gen[5] gen[17].32B, tabB-tabA+4
    ld.gen[5] gen[18].32B, tabB-tabA+8
    mov gen[10].32B, gen[3].32B
    CHK(gen[13].32B)
    CHK(gen[14].32B)
    CHK(gen[16].32B)
    mov gen[3].32B, gen[10].32B
    CHK(gen[15].32B)
    CHK(gen[17].32B)
    CHK(gen[18].32B)
    NEXT(m4)
    call zero.32B, verdict

    ; ---- m5 : B[] from m4 read back: B[i] = A[i] for i < ROUNDS+2 ----
    BEGIN('5')
    set gen[5].32B, tabB
    LSET(gen[3], V0)
    set gen[6].32B, ROUNDS+2
m5:
    ld.gen[5] gen[13].32B, 0
    CHK(gen[13].32B)
    addi gen[5].32B, 4
    subi gen[6].32B, 1
    cmpi gen[6].32B, 0
    ijmp.zero zero.32B, m5
    call zero.32B, verdict

    ; ---- m6 : pointer chase with a store between the loads ----
    ; 64 steps from D[0] through the cycle of 16 ends back at D[0]
    BEGIN('6')
    set gen[1].32B, tabD
    set gen[6].32B, 64
m6:
    ld.gen[1] gen[1].32B, 0
    st.zero gen[6].32B, tabC+16
    ld.zero gen[2].32B, tabC+16
    sub gen[2].32B, gen[6].32B
    CHKV(gen[2].32B, 0)
    subi gen[6].32B, 1
    cmpi gen[6].32B, 0
    ijmp.zero zero.32B, m6
    subi gen[1].32B, tabD
    CHKV(gen[1].32B, 0)
    call zero.32B, verdict

    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
done:
    jmp.one pc.32B, 0

; expect : gen[3] = A[gen[6]] = V0 + gen[6]*K
expect:
    LSET(gen[3], V0)
    mov gen[1].32B, gen[6].32B
exp1:
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, exp2
    add gen[3].32B, gen[4].32B
    subi gen[1].32B, 1
    jmp.one zero.32B, exp1
exp2:
    ret

; verdict : PASS if gen[9] = 0, else FAIL <gen[9]>
verdict:
    cmpi gen[9].32B, 0
    jmp.zero zero.32B, v_pass
    PUTC(' ')
    PUTC('F')
    PUTC('A')
    PUTC('I')
    PUTC('L')
    PUTC(' ')
    mov gen[1].32B, gen[9].32B
    call zero.32B, puthex
    PUTC('\n')
    ret
v_pass:
    PUTC(' ')
    PUTC('P')
    PUTC('A')
    PUTC('S')
    PUTC('S')
    PUTC('\n')
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
