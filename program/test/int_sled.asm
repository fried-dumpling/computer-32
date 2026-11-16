; =====================================================================
;  int_sled.asm : exact return PC. a straight run of "addi gen[5], 1"
;
;  gen[5] counts the committed instructions, so the first uncommitted one
;  is at sled + 4 * gen[5]. that must be the return PC (ISA.md 2.2).
;
;  output : "L<sled> SI<gen5> <istack> <m-4> <m> <level> i"
; =====================================================================
#include<int.hsm>

#macro A4 0
    addi gen[5].32B, 1
    addi gen[5].32B, 1
    addi gen[5].32B, 1
    addi gen[5].32B, 1
#end
#macro A16 0
    A4
    A4
    A4
    A4
#end
#macro A64 0
    A16
    A16
    A16
    A16
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; int_vec[IRQ], address 8
    PUTC('I')
    mov gen[1].32B, gen[5].32B
    PHEX()
    DUMPINT()
    PUTC('i')
    jmp.one pc.32B, 0
main:
    INIT()
    INITIST()
    INTVEC()
    PUTC('L')
    set gen[1].32B, sled
    PHEX()
    PUTC('S')
    set gen[5].32B, 0
sled:
    A64
    A64
    A64
    A64
    PUTC('E')
    jmp.one pc.32B, 0
