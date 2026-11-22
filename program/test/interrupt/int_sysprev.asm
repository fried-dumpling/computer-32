; =====================================================================
;  int_sysprev.asm : syscall return PC for each kind of instruction before it
;
;  found 2026-11-18: the return PC was sometimes the word after the next
;  one, so one instruction was skipped. the cause is fetch timing (trap wait
;  one cycle before the syscall's clr, errors.md), not the instruction kind;
;  the kinds just give different timings. each case is
;  "<X> ; syscall ; aN: addi gen[7], 1". the handler (id 1) logs the
;  return PC, which must be aN. gen[7] counts the addi that ran (12). at the
;  end istack must be ISTACK and stack 0x3000 (the shared stack adder could
;  leave both 4 off after an iret that meets a flush). the handler does not
;  log istack itself: that changed the timing and hid the NPAD 0-3 stall.
;  the cases, in log order:
;    0 st   1 set   2 addi   3 nop   4 set+nop   5 mispredicted loop exit
;    6 jmp.one   7 not-taken jmp.zero (predicted)   8 ld   9 cmpi
;    10 mispredicted loop exit + nop   11 mispredicted loop exit + set
;  (2026-11-18, NPAD 4: 5 and 10 return to aN + 4, the others are right)
;
;  NPAD (default 0) adds NPAD words of .data, which changes how long the
;  boot copy runs. 2026-11-18, in an earlier layout of this file: with
;  NPAD 0-3 the first iret never returned (stall at 0x44), 4-8 ran. the
;  runner builds NPAD 0..23 (item 14c).
;
;  the runner takes aN = address of the Nth syscall word + 4 from the image.
;
;  output : "AG<R0> ... <R11> g<gen7> <istack> <stack> e"
; =====================================================================
#include<int.hsm>

#macro SC 1
    syscall
#1:
    addi gen[7].32B, 1
#end

global _start
.text
_start:
    jmp.one zero.32B, main
    nop
sysh:                                   ; syscall, id 1
    LOGRET()
    iret
main:
    INIT()
    INITIST()
    LOGINIT()
    SETVEC(sysh, 1, PKER + 1)
    PUTC('A')
    set gen[7].32B, 0
    st.zero gen[7].32B, ISTACK+16
    SC(a0)
    set gen[5].32B, 1
    SC(a1)
    addi gen[5].32B, 1
    SC(a2)
    nop
    SC(a3)
    set gen[5].32B, 1
    nop
    SC(a4)
    set gen[5].32B, 0
lp:
    addi gen[5].32B, 1
    cmpi gen[5].32B, 6
    ijmp.zero zero.32B, lp
    SC(a5)
    jmp.one zero.32B, j6
j6:
    SC(a6)
    cmpi gen[5].32B, 99
    jmp.zero zero.32B, 0
    SC(a7)
    ld.zero gen[5].32B, ISTACK+16
    SC(a8)
    cmpi gen[5].32B, 1
    SC(a9)
    set gen[5].32B, 0
lp10:
    addi gen[5].32B, 1
    cmpi gen[5].32B, 6
    ijmp.zero zero.32B, lp10
    nop
    SC(a10)
    set gen[5].32B, 0
lp11:
    addi gen[5].32B, 1
    cmpi gen[5].32B, 6
    ijmp.zero zero.32B, lp11
    set gen[6].32B, 1
    SC(a11)
    DUMPLOG()
    mov gen[1].32B, gen[7].32B
    PHEX()
    ldexp gen[1].32B, istack
    PHEX()
    mov gen[1].32B, stack.32B
    PHEX()
    PUTC('e')
    jmp.one pc.32B, 0

#ifndef NPAD
#define NPAD 0
#end
.data
pad: 4*NPAD+4, 0
