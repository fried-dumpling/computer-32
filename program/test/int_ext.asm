; =====================================================================
;  int_ext.asm : stexp / ldexp round trip of the extended registers
;
;  output : "<cpuid> <cpuid again> <istack> <panicvec> <intlevel> <pbase>
;            <plimit> E"
;  each register gets 0x1234xxxx (intlevel keeps 5 bits, so 0x5678 -> 0x18)
; =====================================================================
#include<int.hsm>

#macro RT 2
    LSET(gen[1], 0x12340000 + #2)
    stexp gen[1].32B, #1
    set gen[1].32B, 0
    ldexp gen[1].32B, #1
    PHEX()
#end

global _start
.text
_start:
    INIT()
    ldexp gen[1].32B, cpuid
    PHEX()
    ldexp gen[1].32B, cpuid
    PHEX()
    RT(istack, 0x5670)
    RT(panicvec, 0x5674)
    RT(intlevel, 0x5678)
    RT(pbase, 0x0000)
    RT(plimit, 0x5000)
    PUTC('E')
    jmp.one pc.32B, 0
