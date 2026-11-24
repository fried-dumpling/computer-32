; =====================================================================
;  int_wpsys.asm : syscall on the wrong path of a mispredicted branch
;
;  each WP block is "ld 0 ; cmpi ; jmp.zero over ; syscall". the branch is
;  always taken, but it resolves late (load) and a BTB miss predicts not
;  taken, so the syscall is fetched on the wrong path. 24 blocks (more than
;  the 16 BTB entries), 4 passes. the syscall never commits, so INT_req must
;  stay 0 and the handler (id 1, prints S) must not run.
;
;  output : "A....E<passes> G<log>g"   (log empty, no S)
; =====================================================================
#include<int.hsm>

#define ZW 0x2010                       ; holds 0

#macro WP 0
    ld.zero gen[8].32B, ZW
    cmpi gen[8].32B, 0
    jmp.zero zero.32B, ##sk
    syscall
##sk:
#end

#macro WP4 0
    WP()
    WP()
    WP()
    WP()
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; syscall, id 1
    HPUTC('S')
    LOGRET()
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, PKER + 1)
    st.zero zero.32B, ZW
    PUTC('A')
    set gen[5].32B, 0
pass:
    WP4()
    WP4()
    WP4()
    WP4()
    WP4()
    WP4()
    PUTC('.')
    addi gen[5].32B, 1
    cmpi gen[5].32B, 4
    ijmp.zero zero.32B, pass
    PUTC('E')
    mov gen[1].32B, gen[5].32B
    PHEX()
    DUMPLOG()
    jmp.one pc.32B, 0
