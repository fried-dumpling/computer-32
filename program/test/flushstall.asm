; =====================================================================
;  flushstall.asm : front end flush while the bus is busy with a store
;
;  build (in program/) :  dasm test/flushstall.asm -o flushstall.o
;                         dlnk flushstall.o -o ../boot/rom.bin
;  run    : tools/dsim/runtests.sh flushstall  (compares with the emulator)
;
;  bug (2026-11-05, calc_nh "2*3-1"): a call/jmp flush (FL_JPU, clr) came
;  while a store held the bus (CT_BUS_IAR=0, dc_e=0). the old fetch in the
;  FT stage register (PCS_FT) was not cleared and DC took it as valid when
;  the stall ended, so an instruction after the call ran on the wrong path.
;
;  each case: K stores, N nops, then a taken call / jmp / branch / ret.
;  every word after the flushing instruction is a "poison" add, so a stale
;  fetch that runs shows up in the counters:
;    gen[7] : adds after the call (run once after the return) / after a
;             jump (never run); poison after ret adds 0x100
;    gen[6] : callee / jump target ran
;    gen[8] : stack at the start (the stack must come back to it)
;  result = gen[7] << 16 | gen[6] << 8 | (start stack - stack)
;  output : "<case> PASS" or "<case> FAIL <result hex>", then "end".
;  case name : <kind><K><N>
;    c : call plain callee   (expect 00040100)
;    r : call, callee does stores before ret (00040100)
;    d : two calls back to back, like calc 0xf08/0xf0c (00040200)
;    p : two pushes, call, two pops (00040100)
;    j : jmp.one (00000100)
;    b : taken jmp.zero after cmp (00000100)
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

; ---- store / nop counts ----
#macro S0 0
#end
#macro S1 0
    st.zero gen[5].32B, buf
#end
#macro S2 0
    S1
    st.zero gen[5].32B, buf+4
#end
#macro S3 0
    S2
    st.zero gen[5].32B, buf+8
#end
#macro S4 0
    S3
    st.zero gen[5].32B, buf+12
#end
#macro N0 0
#end
#macro N1 0
    nop
#end
#macro N2 0
    N1
    nop
#end
#macro N3 0
    N2
    nop
#end
#macro N4 0
    N3
    nop
#end
#macro N6 0
    N4
    nop
    nop
#end

#macro POISON 0
    addi gen[7].32B, 1
    addi gen[7].32B, 1
    addi gen[7].32B, 1
    addi gen[7].32B, 1
#end

; BEGIN(kind, K, N) : print the name, clear the counters
#macro BEGIN 3
    PUTC(#1)
    PUTC(#2)
    PUTC(#3)
    set gen[7].32B, 0
    set gen[6].32B, 0
    mov gen[8].32B, stack.32B
    set gen[5].32B, 0x55
#end

; END(expected) : build the result in gen[1] and print PASS / FAIL
#macro END 1
    LSET(gen[2], #1)
    call zero.32B, verdict
#end

; call cases: K stores, N nops, call callee, poison
#macro CALLC 6
    BEGIN(#1, #2, #3)
    #4
    #5
    call zero.32B, #6
    POISON
    END(0x00040100)
#end

; jump cases: K stores, N nops, jmp.one over the poison
#macro JMPC 5
    BEGIN(#1, #2, #3)
    #4
    #5
    jmp.one zero.32B, ##t
    POISON
##t:
    addi gen[6].32B, 1
    END(0x00000100)
#end

; branch cases: K stores, N nops, cmp, taken jmp.zero over the poison
#macro BRC 5
    BEGIN(#1, #2, #3)
    #4
    #5
    cmp zero.32B, zero.32B
    jmp.zero zero.32B, ##t
    POISON
##t:
    addi gen[6].32B, 1
    END(0x00000100)
#end

.bss
buf:    4*8
stk:    4*64
stk_end:

global _start
.text
_start:
    set stack.32B, stk_end
    LSET(gen[24], IO_BASE)
    PUTC('f')
    PUTC('s')
    PUTC('\n')

    CALLC('c', '0', '0', S0, N0, callee)
    CALLC('c', '1', '0', S1, N0, callee)
    CALLC('c', '1', '1', S1, N1, callee)
    CALLC('c', '1', '2', S1, N2, callee)
    CALLC('c', '1', '3', S1, N3, callee)
    CALLC('c', '1', '4', S1, N4, callee)
    CALLC('c', '1', '6', S1, N6, callee)
    CALLC('c', '2', '0', S2, N0, callee)
    CALLC('c', '2', '1', S2, N1, callee)
    CALLC('c', '2', '2', S2, N2, callee)
    CALLC('c', '2', '3', S2, N3, callee)
    CALLC('c', '2', '4', S2, N4, callee)
    CALLC('c', '3', '0', S3, N0, callee)
    CALLC('c', '3', '1', S3, N1, callee)
    CALLC('c', '3', '2', S3, N2, callee)
    CALLC('c', '3', '3', S3, N3, callee)
    CALLC('c', '4', '0', S4, N0, callee)
    CALLC('c', '4', '1', S4, N1, callee)
    CALLC('c', '4', '2', S4, N2, callee)
    CALLC('c', '4', '4', S4, N4, callee)

    CALLC('r', '0', '0', S0, N0, callee_st)
    CALLC('r', '1', '0', S1, N0, callee_st)
    CALLC('r', '2', '1', S2, N1, callee_st)
    CALLC('r', '4', '2', S4, N2, callee_st)

    BEGIN('d', '1', '0')                ; calc 0xf08 / 0xf0c : call ; call
    S1
    call zero.32B, callee
    call zero.32B, callee
    POISON
    END(0x00040200)
    BEGIN('d', '2', '1')
    S2
    N1
    call zero.32B, callee_st
    call zero.32B, callee
    POISON
    END(0x00040200)

    BEGIN('p', '2', '0')                ; push ; push ; call ; pop ; pop
    push gen[5].32B
    push gen[5].32B
    call zero.32B, callee
    POISON
    pop gen[1].32B
    pop gen[1].32B
    END(0x00040100)
    BEGIN('p', '2', '2')
    push gen[5].32B
    push gen[5].32B
    N2
    call zero.32B, callee
    POISON
    pop gen[1].32B
    pop gen[1].32B
    END(0x00040100)

    JMPC('j', '0', '0', S0, N0)
    JMPC('j', '1', '0', S1, N0)
    JMPC('j', '1', '1', S1, N1)
    JMPC('j', '2', '0', S2, N0)
    JMPC('j', '2', '2', S2, N2)
    JMPC('j', '4', '0', S4, N0)
    JMPC('j', '4', '3', S4, N3)

    BRC('b', '1', '0', S1, N0)
    BRC('b', '2', '1', S2, N1)
    BRC('b', '4', '0', S4, N0)
    BRC('b', '4', '2', S4, N2)

    PUTC('e')
    PUTC('n')
    PUTC('d')
    PUTC('\n')
done:
    jmp.one pc.32B, 0

; ---- callees: the words after ret are poison (gen[7] += 0x100) ----
callee:
    addi gen[6].32B, 1
    ret
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100

callee_st:
    addi gen[6].32B, 1
    st.zero gen[5].32B, buf+16
    st.zero gen[5].32B, buf+20
    st.zero gen[5].32B, buf+24
    ret
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100
    addi gen[7].32B, 0x100

; verdict : gen[2] = expected. result from gen[6..8] and stack
verdict:
    mov gen[1].32B, gen[7].32B
    shiftli gen[1].32B, 8
    bor gen[1].32B, gen[6].32B
    shiftli gen[1].32B, 8
    mov gen[3].32B, gen[8].32B
    subi gen[3].32B, 4                  ; the return address of this call
    sub gen[3].32B, stack.32B
    bndi gen[3].32B, 0xFF
    bor gen[1].32B, gen[3].32B
    cmp gen[1].32B, gen[2].32B
    jmp.zero zero.32B, v_pass
    PUTC(' ')
    PUTC('F')
    PUTC('A')
    PUTC('I')
    PUTC('L')
    PUTC(' ')
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
