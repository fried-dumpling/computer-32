; =====================================================================
;  int_user.asm : user mode (privilege)
;
;  main enters user mode with iret: mem[ISTACK-4] = user, istack =
;  ISTACK-4, int-tmplv = 0 (ker 0, mask 0, level 0). in user:
;    syscall            -> sysh logs {R, int-level, int-tmplv, istack}
;    stexp gen[1], istack (0x1234)  ; kernel instruction: must trap
;    syscall            -> sysh again (only if the stexp did not trap)
;  ISA.md 2.2: in the handler ker = 1 (int-level bit 7) and int-tmplv has
;  the caller's ker = 0. the privilege trap has no fixed id yet; this test
;  assumes id 0 (panic / emergency). its handler "fault" logs {R, istack}
;  and goes to fin (no iret).
;  2026-11-18: the check is not wired (ISS_kernel has no consumer), so the
;  stexp runs and the 2nd syscall pushes at 0x1230.
;
;  output : "L<u1> <ust> <u3> KU[S]u[S|F]...E G<log>g"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; syscall, id 1
    HPUTC('S')
    LOGRET()
    LOGEXT(intlevel)
    LOGEXT(inttmplv)
    LOGEXT(istack)
    iret
fault:                                  ; privilege trap (assumed id 0)
    HPUTC('F')
    set gen[22].32B, 0xFA
    LOGK()
    LOGRET()
    LOGEXT(istack)
    jmp.one zero.32B, fin
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, 1)
    SETVEC(fault, 0, 15)
    PUTC('L')
    PADDR(u1)
    PADDR(ust)
    PADDR(u3)
    set gen[1].32B, user
    st.zero gen[1].32B, ISTACK-4
    set gen[1].32B, ISTACK-4
    stexp gen[1].32B, istack
    stexp zero.32B, inttmplv
    PUTC('K')
    iret
user:
    PUTC('U')
    syscall
u1:
    PUTC('u')
    set gen[1].32B, 0x1234
ust:
    stexp gen[1].32B, istack
    PUTC('v')
    syscall
u3:
    PUTC('w')
fin:
    PUTC('E')
    DUMPLOG()
    jmp.one pc.32B, 0
