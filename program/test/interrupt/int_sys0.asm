; =====================================================================
;  int_sys0.asm : syscall with a level 0 table entry (int_pri[1] = 0)
;
;  a level 0 entry is never taken as itself: the IPU turns it into id 0
;  (panic) with level 31, so the CPU enters int_vec[0] with int-level 0x9f
;  (ker 1, mask 1, lev 0xf). the return PC is the word after the syscall,
;  like a normal syscall. the id 0 handler logs the return PC and the
;  int-level and returns, so the program goes on.
;
;  output : "AP" "BE" "G<R> <int-level> g"; "S" if the id 1 handler ran
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; id 1 (level 0: must not run)
    HPUTC('S')
    iret
panic:                                  ; id 0
    HPUTC('P')
    LOGRET()
    LOGEXT(intlevel)
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, 0)
    SETVEC(panic, 0, 0)
    PUTC('A')
    syscall
    PUTC('B')
    PUTC('E')
    DUMPLOG()
    jmp.one pc.32B, 0
