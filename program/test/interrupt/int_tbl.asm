; =====================================================================
;  int_tbl.asm : interst / interld round trip of int_vec and int_pri
;
;  encoding (2026-11-14 ISA: interst = 79, interld = 78):
;    interst gen[1].32B, 5, vec | 3   ; 0x79080503  int_vec[3] = gen[1] + 5
;    interld gen[2].32B, vec | 3      ; 0x78100003
;    interst gen[1].32B, 2, pri | 3   ; 0x79080283  int_pri[3] = gen[1] + 2
;    interld gen[3].32B, pri | 3      ; 0x78180083
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
    interst gen[1].32B, 5, vec | 3
    interld gen[2].32B, vec | 3
    mov gen[1].32B, gen[2].32B
    PHEX()
    set gen[1].32B, 7
    interst gen[1].32B, 2, pri | 3
    interld gen[3].32B, pri | 3
    mov gen[1].32B, gen[3].32B
    PHEX()
    PUTC('E')
    jmp.one pc.32B, 0
