; =====================================================================
;  int_sysirq.asm : IRQ pulse around a syscall (INT_MASKED window)
;
;  syscall = id 1, IRQ = id 3, both level 1. the runner pulses IRQ at every
;  phase from the loop before the syscall to well after its handler. one
;  of these must happen, never something else:
;    - Q first: the IRQ enters before the syscall commits (return PC at or
;      before sc), then the syscall runs with return PC = after
;    - S first: the syscall handler runs (return PC = after). the IRQ waits
;      (same level, mask 1) and enters after iret, return PC in main
;  both handlers log {return PC, int-level}, so the log has 4 words.
;  gen[6] = 0x77 before the syscall shows a skipped "set gen[6], 0".
;
;  output : "L<sc> <main> <after> [Q][S]...E<gen5> <gen6> G<log>g<istack> <level> e"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; IRQ, id 3
    HPUTC('Q')
    LOGRET()
    LOGEXT(intlevel)
    iret
sysh:                                   ; syscall, id 1
    HPUTC('S')
    LOGRET()
    LOGEXT(intlevel)
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    INTVEC()
    SETVEC(sysh, 1, 1)
    PUTC('L')
    PADDR(sc)
    PADDR(main)
    PADDR(after)
    set gen[5].32B, 0
    set gen[6].32B, 0x77
pre:
    addi gen[5].32B, 1
    cmpi gen[5].32B, 6
    ijmp.zero zero.32B, pre
    mov gen[7].32B, gen[5].32B          ; not nop / branch right before syscall (14a)
sc:
    syscall
after:
    set gen[6].32B, 0
post:
    addi gen[6].32B, 1
    cmpi gen[6].32B, 60
    jmp.neg zero.32B, post
    PUTC('E')
    mov gen[1].32B, gen[5].32B
    PHEX()
    mov gen[1].32B, gen[6].32B
    PHEX()
    DUMPLOG()
    ldexp gen[1].32B, istack
    PHEX()
    ldexp gen[1].32B, intlevel
    PHEX()
    PUTC('e')
    jmp.one pc.32B, 0
