; =====================================================================
;  int_sys.asm : syscall enters the handler and returns to the next word
;
;  syscall raises INT_req and the IPU must answer it (no IRQ pulse). the
;  syscall vector id is not decided yet (ISA.md 2.2), so no int_vec is set
;  and the handler is still at address 8.
;  the return PC printed by the handler must be the word after syscall.
;
;  output : "L<after> AH<m-4> hBCE"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; address 8 = INT_vec
    PUTC('H')
    ld.zero gen[1].32B, ISTACK-4
    PHEX()
    PUTC('h')
    iret
main:
    INIT()
    INITIST()
    PUTC('L')
    set gen[1].32B, after
    PHEX()
    PUTC('A')
    syscall
after:
    PUTC('B')
    PUTC('C')
    PUTC('E')
    jmp.one pc.32B, 0
