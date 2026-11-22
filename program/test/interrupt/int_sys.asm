; =====================================================================
;  int_sys.asm : syscall enters the handler and returns to the next word
;
;  syscall raises INT_req and the IPU must answer it (no IRQ pulse). the
;  IPU reads the table entry on INT_id-_bus, which is 1 during syscall
;  (id 0 is kept for panic / emergency use), so int_vec[1] = handler and
;  int_pri[1] = 1 (a level 0 entry never raises INT_out). the handler is at
;  address 8.
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
    set gen[1].32B, handler
    interst gen[1].32B, 0, vec | 1
    interst zero.32B, PKER + 1, pri | 1
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
