; =====================================================================
;  int_tbl.asm : interst / interld round trip of int_vec and int_pri
;  run : python tools/dsim/runint.py tbl
;
;  bin/dasm.exe is an old build that cannot assemble interst / interld, so
;  the four nops below are patched by runint.py (PATCH table), in this order:
;    interst gen[1].32B, 5, vec | 3   ; 0x78080503  int_vec[3] = gen[1] + 5
;    interld gen[2].32B, vec | 3      ; 0x79100003
;    interst gen[1].32B, 2, pri | 3   ; 0x78080283  int_pri[3] = gen[1] + 2
;    interld gen[3].32B, pri | 3      ; 0x79180083
;  when bin/dasm.exe is replaced, write them here and drop the PATCH entry.
;
;  output : "A00000105 00000009 E"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    INIT()
    PUTC('A')
    set gen[1].32B, 0x100
    nop                                 ; interst gen[1].32B, 5, vec | 3
    nop                                 ; interld gen[2].32B, vec | 3
    mov gen[1].32B, gen[2].32B
    PHEX()
    set gen[1].32B, 7
    nop                                 ; interst gen[1].32B, 2, pri | 3
    nop                                 ; interld gen[3].32B, pri | 3
    mov gen[1].32B, gen[3].32B
    PHEX()
    PUTC('E')
    jmp.one pc.32B, 0
