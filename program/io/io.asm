; =====================================================================
;  io.asm : keyboard / terminal library
;           (implementation, API and usage : io.hsm)
;  assemble separately : dasm io/io.asm -o io.o
;  the first instruction jumps over the library, so the object may also
;  be linked first, but link the main program first anyway.
; =====================================================================

#define IO_IMPL 1
#include<io.hsm>

global io_putc
global io_newline
global io_print
global io_println
global io_printint
global io_printhex
global io_getc
global io_trygetc
global io_getline

; reg = IO_BASE
#macro IO_LDBASE 1
    set #1.S16H, IO_BASE >> 16
    set #1.16L, IO_BASE & 0xFFFF
#end

.data
io_pow10:
    4, 1
    4, 10
    4, 100
    4, 1000
    4, 10000
    4, 100000
    4, 1000000
    4, 10000000
    4, 100000000
    4, 1000000000

.text
    jmp.one zero.32B, io_lib_end

; ---------------------------------------------------------------------
;  output
; ---------------------------------------------------------------------
io_putc:
    IO_LDBASE(gen[13])
    st.gen[13] gen[1].32B, IO_TERM_OUT
    ret

io_newline:
    IO_LDBASE(gen[13])
    set gen[14].32B, '\n'
    st.gen[13] gen[14].32B, IO_TERM_OUT
    ret

io_print:
    IO_LDBASE(gen[13])
    set gen[0].32B, 0
io_print_lp:
    IO_LDB(gen[14], gen[1], 0)
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, io_print_end
    st.gen[13] gen[14].32B, IO_TERM_OUT
    addi gen[1].32B, 1
    addi gen[0].32B, 1
    jmp.one zero.32B, io_print_lp
io_print_end:
    ret

io_println:
    call zero.32B, io_print
    jmp.one zero.32B, io_newline        ; io_newline keeps gen[0]

    ; gen[15] = pow10 table offset, gen[16] = 1 after the first digit
io_printint:
    IO_LDBASE(gen[13])
    cmpi gen[1].32B, 0
    ijmp.neg zero.32B, io_pi_abs
    set gen[14].32B, '-'
    st.gen[13] gen[14].32B, IO_TERM_OUT
    set gen[14].32B, 0
    sub gen[14].32B, gen[1].32B
    mov gen[1].32B, gen[14].32B         ; |x| <= 2^31, cmp below stays exact
io_pi_abs:
    set gen[15].32B, 4*9
    set gen[16].32B, 0
io_pi_dlp:
    ld.gen[15] gen[17].32B, io_pow10
    set gen[14].32B, '0'
io_pi_sub:
    cmp gen[1].32B, gen[17].32B
    jmp.neg zero.32B, io_pi_put
    sub gen[1].32B, gen[17].32B
    addi gen[14].32B, 1
    jmp.one zero.32B, io_pi_sub
io_pi_put:
    cmpi gen[14].32B, '0'
    ijmp.zero zero.32B, io_pi_emit      ; nonzero digit
    cmpi gen[16].32B, 0
    ijmp.zero zero.32B, io_pi_emit      ; zero after the first digit
    cmpi gen[15].32B, 0
    ijmp.zero zero.32B, io_pi_next      ; leading zero (the last one prints)
io_pi_emit:
    st.gen[13] gen[14].32B, IO_TERM_OUT
    set gen[16].32B, 1
io_pi_next:
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, io_pi_end
    subi gen[15].32B, 4
    jmp.one zero.32B, io_pi_dlp
io_pi_end:
    ret

io_printhex:
    IO_LDBASE(gen[13])
    set gen[15].32B, 28                 ; shift
io_ph_lp:
    mov gen[14].32B, gen[1].32B
    shiftr gen[14].32B, gen[15].32B
    bndi gen[14].32B, 0xF
    addi gen[14].32B, '0'
    cmpi gen[14].32B, '9'+1
    jmp.neg zero.32B, io_ph_put
    addi gen[14].32B, 'A'-'9'-1
io_ph_put:
    st.gen[13] gen[14].32B, IO_TERM_OUT
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, io_ph_end
    subi gen[15].32B, 4
    jmp.one zero.32B, io_ph_lp
io_ph_end:
    ret

; ---------------------------------------------------------------------
;  input : block before reading KEY_V, as in add_terminal.txt
; ---------------------------------------------------------------------
io_getc:
    IO_LDBASE(gen[13])
io_getc_wait:
    block
    ld.gen[13] gen[14].32B, IO_KEY_V
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, io_getc_wait
    ld.gen[13] gen[0].32B, IO_KEY_IN
    st.gen[13] full.32B, IO_KEY_R
    ret

io_trygetc:
    IO_LDBASE(gen[13])
    set gen[0].32B, 0
    block
    ld.gen[13] gen[14].32B, IO_KEY_V
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, io_trygetc_end
    ld.gen[13] gen[0].32B, IO_KEY_IN
    st.gen[13] full.32B, IO_KEY_R
io_trygetc_end:
    ret

    ; gen[1] = write ptr, gen[2] = max chars, gen[3] = prompt,
    ; gen[14] = key, gen[15..16] = reprint temp. buffer = bytes
io_getline:
    IO_LDBASE(gen[13])
    set gen[0].32B, 0
    subi gen[2].32B, 1
    jmp.one zero.32B, io_gl_prompt
io_gl_wait:
    block
    ld.gen[13] gen[14].32B, IO_KEY_V
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, io_gl_wait
    ld.gen[13] gen[14].32B, IO_KEY_IN
    st.gen[13] full.32B, IO_KEY_R
    cmpi gen[14].32B, '\n'
    jmp.zero zero.32B, io_gl_end
    cmpi gen[14].32B, 13
    jmp.zero zero.32B, io_gl_end
    cmpi gen[14].32B, 8
    jmp.zero zero.32B, io_gl_bs
    cmpi gen[14].32B, 127
    jmp.zero zero.32B, io_gl_bs
    cmp gen[0].32B, gen[2].32B
    ijmp.neg zero.32B, io_gl_wait       ; buffer full : ignore
    st.gen[13] gen[14].32B, IO_TERM_OUT ; echo
    st.gen[1] gen[14].8L, 0
    addi gen[1].32B, 1
    addi gen[0].32B, 1
    jmp.one zero.32B, io_gl_wait
io_gl_bs:                               ; the terminal cannot erase :
    cmpi gen[0].32B, 0                  ; new line, prompt, line so far
    jmp.zero zero.32B, io_gl_wait
    subi gen[1].32B, 1
    subi gen[0].32B, 1
    set gen[14].32B, '\n'
    st.gen[13] gen[14].32B, IO_TERM_OUT
io_gl_prompt:
    mov gen[15].32B, gen[3].32B
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, io_gl_line
io_gl_plp:
    IO_LDB(gen[14], gen[15], 0)
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, io_gl_line
    st.gen[13] gen[14].32B, IO_TERM_OUT
    addi gen[15].32B, 1
    jmp.one zero.32B, io_gl_plp
io_gl_line:
    mov gen[16].32B, gen[1].32B         ; line start = ptr - len
    sub gen[16].32B, gen[0].32B
io_gl_llp:
    cmp gen[16].32B, gen[1].32B
    jmp.zero zero.32B, io_gl_wait
    IO_LDB(gen[14], gen[16], 0)
    st.gen[13] gen[14].32B, IO_TERM_OUT
    addi gen[16].32B, 1
    jmp.one zero.32B, io_gl_llp
io_gl_end:
    set gen[14].32B, '\n'
    st.gen[13] gen[14].32B, IO_TERM_OUT
    st.gen[1] zero.8L, 0
    ret

io_lib_end:
