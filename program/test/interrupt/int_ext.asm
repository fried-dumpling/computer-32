; =====================================================================
;  int_ext.asm : stexp / ldexp round trip of the extended registers
;
;  output : "<cpuid> <cpuid again> <istack> <intlevel> <inttmplv> <pbase>
;            <plimit> E"
;  each register gets 0x1234xxxx. intlevel and inttmplv keep only
;  lev[b0-3], mask[b4], ker[b7] (2026-11-16 sheet):
;    intlevel 0x5698 -> 0x98 (ker 1, mask 1, lev 8)
;    inttmplv 0x5687 -> 0x87 (ker 1, mask 0, lev 7)
;  ker stays 1 so the later stexp still run in kernel; the two values differ
;  so a write that lands in the other register shows up.
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
    RT(intlevel, 0x5698)
    RT(inttmplv, 0x5687)
    RT(pbase, 0x0000)
    RT(plimit, 0x5000)
    PUTC('E')
    jmp.one pc.32B, 0
