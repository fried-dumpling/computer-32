; =====================================================================
;  int_user.asm : user mode (privilege)
;
;  main sets pbase = 0, plimit = 0x5000 (user code must be inside the
;  limit: with the reset value the first user fetch faults) and enters
;  user mode with iret: mem[ISTACK-4] = user, istack = ISTACK-4,
;  int-tmplv = 0 (ker 0, mask 0, level 0).
;  user code makes no memory access: the MMU keeps user mode off the
;  terminal at 0x4000000 (a PUTC there faults, 2026-11-22). it puts a mark
;  in gen[7] and calls syscall; sysh (kernel) prints "S<mark>" and logs
;  {R, int-level, int-tmplv, istack}. in user:
;    gen[7] = 'U', syscall      -> "SU", iret back to user
;    gen[7] = 'w', syscall      -> "Sw", sysh goes to fin (no iret)
;  ISA.md 2.2: in the handler ker = 1 (int-level bit 7) and int-tmplv has
;  the caller's ker = 0.
;  a kernel instruction in user (stexp) is not run: the privilege fault
;  does not store its id yet, so it re-runs the instruction forever
;  instead of trapping (inttest 20c is SKIP until then).
;
;  output : "L<u1> <u2> KSUSwE G<log>g"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; syscall, id 1 (kernel)
    HPUTC('S')
    st.gen[24] gen[7].32B, TERM_OUT     ; the mark set by the user code
    LOGRET()
    LOGEXT(intlevel)
    LOGEXT(inttmplv)
    LOGEXT(istack)
    cmpi gen[7].32B, 'w'
    jmp.zero zero.32B, fin
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, PKER + 1)
    stexp zero.32B, pbase
    set gen[1].32B, 0x5000
    stexp gen[1].32B, plimit
    PUTC('L')
    PADDR(u1)
    PADDR(u2)
    set gen[1].32B, user
    st.zero gen[1].32B, ISTACK-4
    set gen[1].32B, ISTACK-4
    stexp gen[1].32B, istack
    stexp zero.32B, inttmplv
    PUTC('K')
    iret
user:                                   ; user mode: no memory access
    set gen[7].32B, 'U'
    syscall
u1:
    set gen[7].32B, 'w'
    syscall
u2:
fin:                                    ; kernel (from sysh)
    PUTC('E')
    DUMPLOG()
    jmp.one pc.32B, 0
