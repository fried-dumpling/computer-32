; =====================================================================
;  calc.asm : float calculator on the terminal (floatingpoint + io)
;
;  build (in program/) :
;    dasm calc/calc.asm -o calc.o
;    dasm floatingpoint/float.asm -o float.o
;    dasm io/io.asm -o io.o
;    dlnk calc.o float.o io.o -o calc/calc.bin
;
;  > 1.5 * (2 + -0.5) / 4
;  = 0.5625
;  > 1 + * 2
;        ^ error
;  q : quit (halt), empty line : ignored
;
;  grammar (usual precedence, left to right) :
;    expr   = term   { ('+' | '-') term }
;    term   = factor { ('*' | '/') factor }
;    factor = '-' factor | '+' factor | '(' expr ')' | number
;    number = fp_str2f syntax (1, 2.5, .5, 1e-3, inf, nan)
;  spaces are allowed between tokens.
;
;  registers kept for the whole parse (the libraries preserve them) :
;    gen[12] = cursor, gen[11] = first error position (0 : none)
; =====================================================================
#include<../io/io.hsm>
#include<../floatingpoint/float.hsm>

#define LINE 128                ; input buffer (words)
#define DIGITS 7                ; significant digits of the result
#define PROMPT_W 2              ; width of "> ", for the error caret

.data
    ; messages : packed strings for io_printb (size > length : 0 after).
    ; the prompt goes to io_getline, so it stays 1 char per word.
s_title:    36, "float calc (+ - * / (), q = quit)"
s_prompt:   12(4), {'>', ' '}
s_eq:       4, "= "
s_err:      8, "^ error"

.bss
buf:        4*LINE
obuf:       4*FP_STRBUF
stk:        4*512
stk_end:

.text
main:
    set stack.32B, stk_end
    set gen[1].32B, s_title
    call zero.32B, io_printlnb

loop:
    set gen[1].32B, buf
    set gen[2].32B, LINE
    set gen[3].32B, s_prompt
    call zero.32B, io_getline
    cmpi gen[0].32B, 0
    jmp.zero zero.32B, loop             ; empty line
    ld.zero gen[13].32B, buf            ; "q" : quit
    cmpi gen[13].32B, 'q'
    ijmp.zero zero.32B, eval
    ld.zero gen[13].32B, buf+4
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, quit

eval:
    set gen[12].32B, buf
    set gen[11].32B, 0
    call zero.32B, parse_expr
    mov gen[5].32B, gen[0].32B
    call zero.32B, skip_ws              ; anything left is an error
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, eval_chk
    call zero.32B, set_err
eval_chk:
    cmpi gen[11].32B, 0
    ijmp.zero zero.32B, show_err

    set gen[1].32B, s_eq                ; = value
    call zero.32B, io_printb
    mov gen[1].32B, gen[5].32B
    set gen[2].32B, obuf
    set gen[3].32B, DIGITS
    call zero.32B, fp_f2str
    set gen[1].32B, obuf
    call zero.32B, io_println
    jmp.one zero.32B, loop

show_err:                               ; caret under the bad char
    mov gen[6].32B, gen[11].32B
    subi gen[6].32B, buf
    shiftri gen[6].32B, 2
    addi gen[6].32B, PROMPT_W
show_err_sp:
    cmpi gen[6].32B, 0
    jmp.zero zero.32B, show_err_msg
    set gen[1].32B, ' '
    call zero.32B, io_putc
    subi gen[6].32B, 1
    jmp.one zero.32B, show_err_sp
show_err_msg:
    set gen[1].32B, s_err
    call zero.32B, io_printlnb
    jmp.one zero.32B, loop

quit:
    halt

; ---------------------------------------------------------------------
;  parser helpers : cursor gen[12], error gen[11]
; ---------------------------------------------------------------------

    ; skip spaces, destroys gen[13]
skip_ws:
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, ' '
    ijmp.zero zero.32B, skip_ws_end
    addi gen[12].32B, 4
    jmp.one zero.32B, skip_ws
skip_ws_end:
    ret

    ; remember the cursor as the error position (first error only)
set_err:
    cmpi gen[11].32B, 0
    ijmp.zero zero.32B, set_err_end
    mov gen[11].32B, gen[12].32B
set_err_end:
    ret

    ; gen[0] = expr, gen[5] = acc, gen[6] = operator
parse_expr:
    push gen[5].32B
    push gen[6].32B
    call zero.32B, parse_term
    mov gen[5].32B, gen[0].32B
expr_lp:
    call zero.32B, skip_ws
    ld.gen[12] gen[6].32B, 0
    cmpi gen[6].32B, '+'
    jmp.zero zero.32B, expr_op
    cmpi gen[6].32B, '-'
    ijmp.zero zero.32B, expr_end
expr_op:
    addi gen[12].32B, 4
    call zero.32B, parse_term
    mov gen[1].32B, gen[5].32B
    mov gen[2].32B, gen[0].32B
    cmpi gen[6].32B, '+'
    ijmp.zero zero.32B, expr_sub
    call zero.32B, fp_add
    jmp.one zero.32B, expr_next
expr_sub:
    call zero.32B, fp_sub
expr_next:
    mov gen[5].32B, gen[0].32B
    jmp.one zero.32B, expr_lp
expr_end:
    mov gen[0].32B, gen[5].32B
    pop gen[6].32B
    pop gen[5].32B
    ret

    ; gen[0] = term, gen[5] = acc, gen[6] = operator
parse_term:
    push gen[5].32B
    push gen[6].32B
    call zero.32B, parse_factor
    mov gen[5].32B, gen[0].32B
term_lp:
    call zero.32B, skip_ws
    ld.gen[12] gen[6].32B, 0
    cmpi gen[6].32B, '*'
    jmp.zero zero.32B, term_op
    cmpi gen[6].32B, '/'
    ijmp.zero zero.32B, term_end
term_op:
    addi gen[12].32B, 4
    call zero.32B, parse_factor
    mov gen[1].32B, gen[5].32B
    mov gen[2].32B, gen[0].32B
    cmpi gen[6].32B, '*'
    ijmp.zero zero.32B, term_div
    call zero.32B, fp_mul
    jmp.one zero.32B, term_next
term_div:
    call zero.32B, fp_div
term_next:
    mov gen[5].32B, gen[0].32B
    jmp.one zero.32B, term_lp
term_end:
    mov gen[0].32B, gen[5].32B
    pop gen[6].32B
    pop gen[5].32B
    ret

    ; gen[0] = factor
parse_factor:
    call zero.32B, skip_ws
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, '('
    jmp.zero zero.32B, factor_paren
    cmpi gen[13].32B, '-'
    jmp.zero zero.32B, factor_neg
    cmpi gen[13].32B, '+'
    jmp.zero zero.32B, factor_pos
    mov gen[1].32B, gen[12].32B         ; number
    call zero.32B, fp_str2f
    cmp gen[1].32B, gen[12].32B
    jmp.zero zero.32B, set_err          ; nothing parsed (gen[0] = 0)
    mov gen[12].32B, gen[1].32B
    ret
factor_pos:
    addi gen[12].32B, 4
    jmp.one zero.32B, parse_factor
factor_neg:
    addi gen[12].32B, 4
    call zero.32B, parse_factor
    set gen[13].S16H, 0x8000
    set gen[13].16L, 0
    bxr gen[0].32B, gen[13].32B         ; flip the sign bit
    ret
factor_paren:
    addi gen[12].32B, 4
    call zero.32B, parse_expr
    call zero.32B, skip_ws
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, ')'
    ijmp.zero zero.32B, set_err         ; missing ')'
    addi gen[12].32B, 4
    ret
