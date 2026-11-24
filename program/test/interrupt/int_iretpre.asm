; =====================================================================
;  int_iretpre.asm : iret after different instructions
;
;  found 2026-11-18: iret never returned when the word before it was a nop
;  or a set (IRQ handler of int_iret), and in int_nest's level 2 handler
;  (LOGRET ; HPUTC ; ld ; ld ; iret), while ld or st right before iret
;  was fine. the runner builds one image per case (wrapper defines Cn) and
;  enters the handler twice, once by syscall and once by IRQ, so a hang in
;  one case does not hide the others.
;  every case is "HPUTC('I') ; LOGRET ; [st st] ; HPUTC('i') ; <X> ; iret":
;    C0 X empty (st)   C1 nop   C2 set   C3 4 nops   C4 ld
;    C5 ld ld (int_nest's level 2 handler)   C6 = C5   C7 st st before HPUTC, ld ld
;  output : "L<loop> S" ... "Ii" (syscall) ... "Ii" (IRQ) ... "E<gen5> G<R> <R>g"
; =====================================================================
#include<int.hsm>

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
handler:                                ; IRQ id 3 and syscall id 1
    HPUTC('I')
    LOGRET()
#ifdef C7
    st.zero gen[22].32B, 0x27E4
    st.zero gen[23].32B, 0x27E8
#end
    HPUTC('i')
#ifdef C1
    nop
#end
#ifdef C2
    set gen[22].32B, 1
#end
#ifdef C3
    nop
    nop
    nop
    nop
#end
#ifdef C4
    ld.zero gen[22].32B, 0x27E4
#end
#ifdef C5
    ld.zero gen[22].32B, 0x27E4
    ld.zero gen[23].32B, 0x27E8
#end
#ifdef C6
    ld.zero gen[22].32B, 0x27E4
    ld.zero gen[23].32B, 0x27E8
#end
#ifdef C7
    ld.zero gen[22].32B, 0x27E4
    ld.zero gen[23].32B, 0x27E8
#end
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    INTVEC()
    SETVEC(handler, 1, PKER + 1)
    PUTC('L')
    PADDR(loop)
    PUTC('S')
    syscall
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
    DUMPLOG()
    jmp.one pc.32B, 0
