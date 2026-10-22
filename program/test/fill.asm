#define GR_BASE 0x8000000
#define GR_SIZE (160*90*4)

#macro LSET 2
    set #1.16L, (#2)&0xFFFF
    set #1.S16H, (#2)>>16
#end

#macro RGB 3 (((#1) << 10) + ((#2) << 5) + (#3) + 0x8000) #end

global _start

.text main
_start:
    LSET(gen[24], GR_BASE)
    LSET(gen[23], GR_SIZE)
    LSET(gen[1], RGB(0,0,0))
    

    mov gen[5].32B, gen[24].32B
    mov gen[6].32B, gen[24].32B
    add gen[6].32B, gen[23].32B

fill_loop:
    st.gen[5] gen[1].32B, 0
    addi gen[5].32B, 4
    cmp gen[5].32B, gen[6].32B
    jmp.neg zero.32B, fill_loop

    halt