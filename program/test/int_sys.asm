; =====================================================================
;  int_sys.asm : syscall enters the handler and returns to the next word
;  run : python tools/dsim/runint.py sys
;
;  the test bench has no IPU. the runner first runs this without a pulse to
;  see INT_req go high, then answers it with an INT_in pulse, like an IPU.
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
