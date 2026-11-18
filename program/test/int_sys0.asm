; =====================================================================
;  int_sys0.asm : syscall with a level 0 table entry (int_pri[1] = 0)
;
;  a level 0 entry is never accepted (ISA.md 5, interst), so the handler
;  must not run. what the CPU does then is not decided (todo.md: id 0 /
;  kernel must set int_pri[1] >= 1). the runner reports whether the program
;  goes on after the syscall (B) or stops there.
;
;  output : "A" then "BE" if it goes on, "S" if the handler ran
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; id 1
    HPUTC('S')
    iret
main:
    INIT()
    INITIST()
    SETVEC(sysh, 1, 0)
    PUTC('A')
    syscall
    PUTC('B')
    PUTC('E')
    jmp.one pc.32B, 0
