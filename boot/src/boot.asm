; ========================================
; boot loader program
; org 0xFFFFFF00
;
; ram       0x00000000 ~ 0x03FFFFFF 
; key/term  0x04000000 ~ 0x04000003 (Data, Valid, Pop, Term)
; display   0x08000000 ~ 0x0801FFFF (160 x 80)
; rom       0xFC000000 ~ 0xFC3FFFFF (read only)
; boot      0xFFFFFF00 ~ 0xFFFFFFFF (inst only / word align)
; ========================================

#define ROM 0xFC000000
#define RAM 0x00000000

#macro LSET 2
    set #1.16L, (#2) & 0xFFFF
    set #1.S16H, (#2) >> 16
#end

#macro RJMP 1
    jmp.one pc.32B, #1-$
#end

#macro RCJMP 2
    jmp.#1 pc.32B, #2-$
#end

#macro RICJMP 2
    ijmp.#1 pc.32B, #2-$
#end

.text
    LSET(gen[23], ROM)
    LSET(gen[24], RAM)

    mov gen[1].32B, gen[23].32B
    mov gen[2].32B, gen[24].32B

    ; ram start
    ld.gen[1] gen[4].32B, 0
    addi gen[1].32B, 4
    add gen[4].32B, gen[24].32B

    ; program len
    ld.gen[1] gen[3].32B, 0
    addi gen[1].32B, 4
    add gen[3].32B, gen[1].32B

    ; copy loop (gen[1] < end)
    cmp gen[1].32B, gen[3].32B
    RICJMP(neg, move_end)
move_loop:
    ld.gen[1] gen[0].32B, 0
    st.gen[2] gen[0].32B, 0

    addi gen[1].32B, 4
    addi gen[2].32B, 4

    cmp gen[1].32B, gen[3].32B
    RCJMP(neg, move_loop)
move_end:

    ; main program entry
    block
    jmp.one gen[4].32B, 0
    