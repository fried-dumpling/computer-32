; =====================================================================
;  int_align.asm : alignment fault (id 5)
;
;  rule (natural alignment): a half (16L) needs an even address, a word
;  (32B) a multiple of 4; a byte (8L / 8H) is allowed anywhere. anything
;  else raises fault 5 and the ld / st must not take effect.
;
;  runs in kernel. X = 0x3800 holds W0 = 0x11223344, X+4 = W1 = 0x55667788.
;  before each case gen[2] = SENT and gen[9] = where the handler resumes.
;  the handler prints 'A', logs {0xF5, R, gen[2]} (R = mem[istack], the
;  faulting instruction; gen[2] must still be SENT: the faulting ld does not
;  write it) and irets to gen[9].
;    c1  ld.32B X+2        -> 'A', R = c1
;    c2  ld.16L X+1        -> 'A', R = c2
;    c3  st.32B X+2 <- AABBCCDD   -> 'A', R = c3, memory unchanged
;    c4  st.16L X+3 <- AABBCCDD   -> 'A', R = c4, memory unchanged (crosses)
;    legal, no fault: ld.16L X+2 -> 00001122, ld.8L X+3 -> 00000011,
;                     ld.8H X+1 -> 00003300
;    wrong path: an unaligned ld and st behind a taken jmp must not trap
;  at the end it prints mem[X], mem[X+4] (must be W0, W1), the three legal
;  loads and the log.
;  FSTART (default s1) is where the cases start. inttest also builds one
;  variant per case (FSTART s<n>, FONE: the handler resumes at wpend) and
;  one from r4 (legal loads and the wrong path only).
;
;  output : "L<c1> .. <c4> KAAAAE<X> <X+4> <l1> <l2> <l3> G<log>g"
; =====================================================================
#include<int.hsm>

#define XW 0x3800

#ifndef FSTART
#define FSTART s1
#end
#ifdef FONE
#define N1 wpend
#define N2 wpend
#define N3 wpend
#define N4 wpend
#end
#ifndef FONE
#define N1 r1
#define N2 r2
#define N3 r3
#define N4 r4
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
alh:                                    ; alignment fault, id 5
    HPUTC('A')
    set gen[22].32B, 0xF5
    LOGK()
    LOGRET()
    LOGREG(gen[2].32B)
    ldexp gen[22].32B, istack
    st.gen[22] gen[9].32B, 0            ; resume at gen[9]
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(alh, 5, PKER + 1)
    LSET(gen[1], 0x11223344)
    st.zero gen[1].32B, XW
    LSET(gen[1], 0x55667788)
    st.zero gen[1].32B, XW+4
    LSET(gen[5], 0xAABBCCDD)
    set gen[10].32B, 0
    set gen[11].32B, 0
    set gen[12].32B, 0
    PUTC('L')
    PADDR(c1)
    PADDR(c2)
    PADDR(c3)
    PADDR(c4)
    PUTC('K')
    jmp.one zero.32B, FSTART
s1: set gen[9].32B, N1
    LSET(gen[2], SENT)
c1: ld.zero gen[2].32B, XW+2
r1:
s2: set gen[9].32B, N2
    LSET(gen[2], SENT)
c2: ld.zero gen[2].16L, XW+1
r2:
s3: set gen[9].32B, N3
    LSET(gen[2], SENT)
c3: st.zero gen[5].32B, XW+2
r3:
s4: set gen[9].32B, N4
    LSET(gen[2], SENT)
c4: st.zero gen[5].16L, XW+3
r4:
    ld.zero gen[10].16L, XW+2           ; legal
    ld.zero gen[11].8L, XW+3
    ld.zero gen[12].8H, XW+1
    jmp.one zero.32B, wpend
    ld.zero gen[2].32B, XW+1            ; wrong path
    st.zero gen[5].32B, XW+1
    nop
wpend:
    PUTC('E')
    ld.zero gen[1].32B, XW
    call zero.32B, phex
    ld.zero gen[1].32B, XW+4
    PHEX()
    mov gen[1].32B, gen[10].32B
    call zero.32B, phex
    mov gen[1].32B, gen[11].32B
    call zero.32B, phex
    mov gen[1].32B, gen[12].32B
    call zero.32B, phex
    DUMPLOG()
    jmp.one pc.32B, 0

phex:                                   ; print gen[1] (8 hex digits and a space)
    PHEX()
    ret
