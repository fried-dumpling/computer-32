; =====================================================================
;  int_fault.asm : page fault (id 3) and privilege fault (id 4)
;
;  main sets pbase = 0, plimit = 0x5000 and enters user mode with iret.
;  each user case raises one fault; the handler prints a mark, logs
;  {0xF3 | 0xF4, R, int-level, int-tmplv, istack}, sets the return PC
;  to gen[9] (the case's r<n>, set by the user code before it) and irets to user.
;    c1  ld   0x6000 (above plimit)       -> page fault 'P', R = c1
;    c2  st   0x6000                      -> 'P', R = c2
;    c3  stexp istack (kernel instr.)     -> privilege fault 'V', R = c3
;    c4  jmp  0x6000 (fetch above plimit) -> 'P', R = 0x6000
;    c5  iret (kernel instr.)             -> 'V', R = c5
;    c6  st   terminal 0x4000000          -> 'P', R = c6
;    wrong path: a ld 0x6000 and a stexp behind a taken jmp (BTB miss)
;    must not trap
;    syscall                              -> 'S', goes to fin
;  ld 0x4FFC (inside the limit) at the start must not fault.
;  FSTART (default user) is where the user code starts. inttest also
;  builds one variant per case (FSTART s<n>, FONE: the handler resumes at
;  wpend), so that one case that hangs does not hide the others.
;
;  output : "L<c1> .. <c6> KPPVPVPSE G<log>g"
; =====================================================================
#include<int.hsm>

#ifndef FSTART
#define FSTART user
#end
#ifdef FONE
#define N1 wpend
#define N2 wpend
#define N3 wpend
#define N4 wpend
#define N5 wpend
#define N6 wpend
#end
#ifndef FONE
#define N1 r1
#define N2 r2
#define N3 r3
#define N4 r4
#define N5 r5
#define N6 r6
#end

#macro FLOG 1
    set gen[22].32B, #1
    LOGK()
    LOGRET()
    LOGEXT(intlevel)
    LOGEXT(inttmplv)
    LOGEXT(istack)
    ldexp gen[22].32B, istack
    st.gen[22] gen[9].32B, 0            ; resume the user code at gen[9]
    iret
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
pgh:                                    ; page fault, id 3
    HPUTC('P')
    FLOG(0xF3)
prh:                                    ; privilege fault, id 4
    HPUTC('V')
    FLOG(0xF4)
sysh:                                   ; syscall, id 1: end of the test
    HPUTC('S')
    jmp.one zero.32B, fin
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(pgh, 3, PKER + 1)
    SETVEC(prh, 4, PKER + 1)
    SETVEC(sysh, 1, PKER + 1)
    stexp zero.32B, pbase
    set gen[1].32B, 0x5000
    stexp gen[1].32B, plimit
    PUTC('L')
    PADDR(c1)
    PADDR(c2)
    PADDR(c3)
    PADDR(c4)
    PADDR(c5)
    PADDR(c6)
    set gen[1].32B, FSTART
    st.zero gen[1].32B, ISTACK-4
    set gen[1].32B, ISTACK-4
    stexp gen[1].32B, istack
    stexp zero.32B, inttmplv
    PUTC('K')
    iret
user:
    ld.zero gen[2].32B, 0x4FFC          ; inside the limit
s1: set gen[9].32B, N1
c1: ld.zero gen[2].32B, 0x6000
r1:
s2: set gen[9].32B, N2
c2: st.zero gen[2].32B, 0x6000
r2:
s3: set gen[9].32B, N3
    set gen[1].32B, 0x1234              ; what the stexp would write
c3: stexp gen[1].32B, istack
r3:
s4: set gen[9].32B, N4
c4: jmp.one zero.32B, 0x6000
r4:
s5: set gen[9].32B, N5
c5: iret
r5:
s6: set gen[9].32B, N6
c6: st.gen[24] gen[2].32B, TERM_OUT
r6: jmp.one zero.32B, wpend
    ld.zero gen[2].32B, 0x6000
    stexp gen[1].32B, istack
    nop
wpend:
    syscall
fin:
    PUTC('E')
    DUMPLOG()
    jmp.one pc.32B, 0
