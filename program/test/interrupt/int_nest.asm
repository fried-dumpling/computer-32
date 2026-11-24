; =====================================================================
;  int_nest.asm : a higher-level IRQ inside a handler (nesting)
;
;  low  = I-in bit IRQ  (id 3, level 1), handler "lo"
;  high = I-in bit IRQ2 (id 4, level 2), handler "hi"
;  the runner pulses low in the main loop, then high at every phase of the
;  low handler. the low handler saves int-tmplv, lowers the mask
;  (int-level = ker 1, mask 0, level 1 = 0x81), runs a nop sled where high
;  may nest, sets mask 1 again, restores int-tmplv and returns (ISA.md 2.2).
;  istack is not moved by software (nothing is pushed), so the nested entry
;  writes its return PC at ISTACK-8.
;  hi saves gen[22], gen[23] (the low handler's temporaries) to memory and
;  ends with a dummy st: with "ld ; ld ; iret" its iret never returned.
;  a high pulse before the mask is lowered must wait for it; one after the
;  mask is set again must wait for the low iret (then it enters from main).
;
;  log: lo {R, int-level}, hi {R, int-level, int-tmplv}, lo {int-tmplv
;  after restore}. order of the words follows the entries.
;  output : "L<loop> <open> <close> <main> S..A.a..B.b..z..E<gen5> <istack> <level> G<log>g"
;    open = first word after the unmask stexp, close = the remask stexp
; =====================================================================
#include<int.hsm>

#define SAVE_TMP 0x27E0
#define H22 0x27E4
#define H23 0x27E8

#macro N8 0
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
lo:                                     ; id 3, level 1
    HPUTC('A')
    LOGRET()
    LOGEXT(intlevel)
    ldexp gen[23].32B, inttmplv
    st.zero gen[23].32B, SAVE_TMP
    HPUTC('a')
    set gen[22].32B, 0x81
    stexp gen[22].32B, intlevel         ; mask 0: level 2 may nest
open:
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    N8
    set gen[22].32B, 0x91
close:
    stexp gen[22].32B, intlevel         ; mask 1
    ld.zero gen[23].32B, SAVE_TMP
    stexp gen[23].32B, inttmplv
    LOGEXT(inttmplv)
    HPUTC('z')
    iret
hi:                                     ; id 4, level 2
    st.zero gen[22].32B, H22
    st.zero gen[23].32B, H23
    HPUTC('B')
    LOGRET()
    LOGEXT(intlevel)
    LOGEXT(inttmplv)
    HPUTC('b')
    ld.zero gen[22].32B, H22
    ld.zero gen[23].32B, H23
    st.zero zero.32B, H23+4             ; not ld ; ld ; iret: that hangs (int_iretpre C5, item 22)
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(lo, IRQ, PKER + IRQ_LEV)
    SETVEC(hi, IRQ2, PKER + IRQ2_LEV)
    PUTC('L')
    PADDR(loop)
    PADDR(open)
    PADDR(close)
    PADDR(main)
    PUTC('S')
    set gen[5].32B, 0
loop:
    addi gen[5].32B, 1
    set gen[20].32B, '.'
    st.gen[24] gen[20].32B, TERM_OUT
    cmpi gen[5].32B, 60
    ijmp.zero zero.32B, loop
    PUTC('E')
    mov gen[1].32B, gen[5].32B
    PHEX()
    ldexp gen[1].32B, istack
    PHEX()
    ldexp gen[1].32B, intlevel
    PHEX()
    DUMPLOG()
    jmp.one pc.32B, 0
