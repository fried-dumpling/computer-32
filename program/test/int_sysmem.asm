; =====================================================================
;  int_sysmem.asm : syscall right after st / ld, memory precise
;
;    st X = 0x11111111 ; syscall ; st Z = 0x33333333 ; ld gen[8] = W
;    ld gen[6] = Y (0x22222222) ; syscall
;  the handler (id 1) logs {return PC, mem[X], mem[Z], gen[6]} and writes
;  W = return PC. so:
;    1st: R = a1, X = 0x11111111 (store before syscall done), Z = SENT
;         (store after syscall not done), gen[6] = 0
;    2nd: R = a2, X, Z = 0x33333333, gen[6] = 0x22222222 (load done)
;  and main's gen[8] = a1 (the load after syscall sees the handler's store).
;
;  output : "L<a1> <a2> E<gen8> G<8 words>g"
; =====================================================================
#include<int.hsm>

#define XA 0x2100
#define YA 0x2104
#define ZA 0x2108
#define WA 0x210C

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; syscall, id 1
    LOGRET()
    st.zero gen[22].32B, WA
    ld.zero gen[22].32B, XA
    LOGK()
    ld.zero gen[22].32B, ZA
    LOGK()
    LOGREG(gen[6].32B)
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, 1)
    LSET(gen[1], SENT)
    st.zero gen[1].32B, XA
    st.zero gen[1].32B, ZA
    st.zero gen[1].32B, WA
    LSET(gen[1], 0x22222222)
    st.zero gen[1].32B, YA
    PUTC('L')
    PADDR(a1)
    PADDR(a2)
    LSET(gen[5], 0x11111111)
    LSET(gen[7], 0x33333333)
    set gen[6].32B, 0
    set gen[8].32B, 0
    st.zero gen[5].32B, XA
    syscall
a1:
    st.zero gen[7].32B, ZA
    ld.zero gen[8].32B, WA
    ld.zero gen[6].32B, YA
    syscall
a2:
    PUTC('E')
    mov gen[1].32B, gen[8].32B
    PHEX()
    DUMPLOG()
    jmp.one pc.32B, 0
