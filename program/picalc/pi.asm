; =====================================================================
;  pi.asm : decimal digits of pi, streamed out of the BBP hex digits
;
;  build (in program/) :
;    dasm picalc/pi.asm -o pi.o
;    dasm io/io.asm -o io.o
;    dlnk pi.o io.o -o picalc/pi.bin
;
;  > 0 20                  (start position, digit count)
;  3.
;  0: 1415926535 8979323846
;  > 100 10
;  100: 8214808651
;  empty line : 0 50, q : quit (halt). pos + count <= MAXDEC.
;  position 0 is the first decimal digit after the point.
;
;  BBP only gives hex digits. every time it gives 4 more (one limb),
;  the decimal digits that limb settles are taken out by times 10 :
;
;  1. limb : chunk c = 4 hex digits from position 4c, one BBP
;     evaluation. x_d = frac(16^d pi) * 2^32 is good to about 2^8, so
;     its top 16 bits can be one off next to a 16 bit boundary. the next
;     chunk x_(d+4) holds the true low 16 bits of x_d, so
;       limb = (x_d + sext16(top16(x_(d+4)) - low16(x_d))) >> 16.
;  2. add : F_k = first k limbs as a fraction, j = decimal digits out.
;     sfrac = frac(10^j F_k) (k limbs, most significant first) and
;     pbig = 10^j (little endian). a new limb L adds 10^j L 2^-16(k+1),
;     i.e. pbig * L at limb k. a carry out of sfrac adds 1 to the
;     digits already out.
;  3. digits : while 10 * 10^j <= 2^16k : sfrac *= 10, the integer part
;     is the next decimal digit, pbig *= 10. this keeps everything still
;     to come below 1 unit of sfrac, so at most one carry can reach the
;     digits out, and only through the last one and the 9s after it.
;     those are held (g_h, g_n9) and printed when a digit < 9 follows;
;     a carry turns them into h+1 0 0 .. 0.
;  4 to 5 decimal digits come out per limb, about n/4.8 limbs for n.
;
;  BBP : pi = sum_k 16^-k (4/(8k+1) - 2/(8k+4) - 1/(8k+5) - 1/(8k+6))
;  frac(16^d pi) = frac(4 S1 - 2 S4 - S5 - S6),
;    S_j = sum_{k<=d} (16^(d-k) mod (8k+j)) / (8k+j)
;        + sum_{k>d} 16^(d-k) / (8k+j)
;  fractions are 32 bit fixed point (value * 2^32), so the sums wrap
;  mod 1 by themselves. measured error : under 2^8 of 2^32 up to
;  hex position 3000.
;
;  integer only : no mul / div instruction. 16^e mod m is a square and
;  shift ladder (modmul by shift-add), a / m is restoring division,
;  pbig * L is shift-add. values in compares stay below 2^31, so
;  cmp + neg is an unsigned compare.
;  cost : a chunk at hex position d is about 30k steps at d = 10,
;  0.4M at d = 100, 6.4M at d = 1000 (emulator). the decimal part is
;  small next to it.
; =====================================================================
#include<../io/io.hsm>

#define LINE 32                 ; input buffer (words)
#define DEFCOUNT 50             ; digits when the line is empty
#define PERLINE 50              ; digits per output line
#define GROUP 10                ; space every GROUP digits
#define MAXDEC 1000             ; pos + count limit
#define NLIMB (MAXDEC*27/128 + 16)  ; limbs : n/4.8 + room for 9 runs
#define PSAFE 6553              ; top limb of pbig below this : 10x fits

.data
    ; messages : packed strings for io_printb (size > length : 0 after).
    ; the prompt goes to io_getline, so it stays 1 char per word.
s_title:    36, "pi (BBP) - pos [count], q = quit"
s_prompt:   12(4), {'>', ' '}
s_three:    4, "3."
s_colon:    4, ": "
s_err:      16, "? pos [count]"

.bss
buf:        4*LINE
sfrac:      4*NLIMB             ; frac(10^j F_k), 16 bit limbs, MS first
pbig:       4*(NLIMB+1)         ; 10^j, 16 bit limbs, LS first
g_kb:       4                   ; k * 4 (bytes of sfrac in use)
g_lenpb:    4                   ; limbs of pbig * 4
g_h:        4                   ; held digit, -1 : none
g_n9:       4                   ; 9s held after it
g_idx:      4                   ; index of the next digit to print
g_pos:      4                   ; first index printed
g_n:        4                   ; pos + count
g_line:     4                   ; digits on the output line
g_grp:      4                   ; digits left in the group
stk:        4*256
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
    ld.zero gen[13].32B, buf            ; "q" : quit
    cmpi gen[13].32B, 'q'
    ijmp.zero zero.32B, parse
    ld.zero gen[13].32B, buf+4
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, quit

parse:                                  ; gen[5] = pos, gen[6] = count
    set gen[12].32B, buf
    set gen[5].32B, 0
    set gen[6].32B, DEFCOUNT
    call zero.32B, skip_ws
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, run              ; empty : defaults
    call zero.32B, read_num
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, bad              ; no digit
    mov gen[5].32B, gen[0].32B
    call zero.32B, skip_ws
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, run              ; no count
    call zero.32B, read_num
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, bad
    mov gen[6].32B, gen[0].32B
    call zero.32B, skip_ws
    ld.gen[12] gen[13].32B, 0
    cmpi gen[13].32B, 0
    ijmp.zero zero.32B, bad             ; junk after count

run:                                    ; gen[7] = n = pos + count
    mov gen[7].32B, gen[5].32B
    add gen[7].32B, gen[6].32B
    cmpi gen[7].32B, MAXDEC+1
    ijmp.neg zero.32B, bad
    cmpi gen[6].32B, 0
    jmp.zero zero.32B, loop             ; nothing to print

    st.zero gen[5].32B, g_pos
    st.zero gen[7].32B, g_n
    st.zero zero.32B, g_idx
    st.zero zero.32B, g_line
    st.zero zero.32B, g_n9
    st.zero full.32B, g_h
    set gen[13].32B, 1                  ; pbig = 1
    st.zero gen[13].32B, pbig
    set gen[13].32B, 4
    st.zero gen[13].32B, g_lenpb

    cmpi gen[5].32B, 0                  ; "3." before position 0
    ijmp.zero zero.32B, run_go
    set gen[1].32B, s_three
    call zero.32B, io_printlnb
run_go:                                 ; gen[9] = 4 * chunk, gen[10] = its x
    set gen[1].32B, 0
    call zero.32B, pi_frac
    mov gen[10].32B, gen[0].32B
    set gen[9].32B, 0

chunk_lp:
    cmpi gen[9].32B, 4*NLIMB
    jmp.zero zero.32B, fin              ; out of room (long 9 run)
    mov gen[1].32B, gen[9].32B          ; x of the next chunk
    addi gen[1].32B, 4
    call zero.32B, pi_frac
    mov gen[13].32B, gen[0].32B         ; e = top16(next) - low16(x)
    shiftri gen[13].32B, 16
    mov gen[14].32B, gen[10].32B
    bndi gen[14].32B, 0xFFFF
    sub gen[13].32B, gen[14].32B
    bndi gen[13].32B, 0xFFFF            ; sign extend 16 bits
    bxri gen[13].32B, 0x8000
    subi gen[13].32B, 0x8000
    add gen[13].32B, gen[10].32B        ; limb = (x + e) >> 16
    shiftri gen[13].32B, 16
    mov gen[10].32B, gen[0].32B
    st.gen[9] zero.32B, sfrac           ; new sfrac limb = 0
    addi gen[9].32B, 4
    st.zero gen[9].32B, g_kb
    mov gen[1].32B, gen[13].32B         ; sfrac += pbig * limb
    call zero.32B, dec_addlimb
    cmpi gen[0].32B, 0
    jmp.zero zero.32B, emit_lp
    call zero.32B, dig_carry

emit_lp:
    ld.zero gen[13].32B, g_idx          ; all printed : done
    cmp gen[13].32B, gen[7].32B
    ijmp.neg zero.32B, fin
    ld.zero gen[13].32B, g_lenpb        ; 10 pbig <= 2^16k ?
    cmp gen[13].32B, gen[9].32B
    jmp.neg zero.32B, emit_go           ; fewer limbs than sfrac
    ijmp.zero zero.32B, chunk_lp
    subi gen[13].32B, 4                 ; as many : top limb small
    ld.gen[13] gen[13].32B, pbig
    cmpi gen[13].32B, PSAFE
    ijmp.neg zero.32B, chunk_lp
emit_go:
    call zero.32B, dec_emit
    mov gen[1].32B, gen[0].32B
    call zero.32B, dig_in
    jmp.one zero.32B, emit_lp

fin:
    call zero.32B, dig_flush
    ld.zero gen[13].32B, g_line         ; close a partial line
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, loop
    call zero.32B, io_newline
    jmp.one zero.32B, loop

bad:
    set gen[1].32B, s_err
    call zero.32B, io_printlnb
    jmp.one zero.32B, loop

quit:
    halt

; ---------------------------------------------------------------------
;  input helpers : cursor gen[12]
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

    ; decimal number at the cursor. gen[0] = value, gen[1] = digit count
    ; values stop growing at 0xFFFF (over MAXDEC anyway)
read_num:
    set gen[0].32B, 0
    set gen[1].32B, 0
read_num_lp:
    ld.gen[12] gen[13].32B, 0
    subi gen[13].32B, '0'
    cmpi gen[13].32B, 10                ; below '0' is negative here
    jmp.neg zero.32B, read_num_dg
    ret
read_num_dg:
    cmpi gen[13].32B, 0
    jmp.neg zero.32B, read_num_end      ; below '0'
    addi gen[1].32B, 1
    addi gen[12].32B, 4
    cmpi gen[0].32B, 0xFFFF
    ijmp.neg zero.32B, read_num_lp      ; already too big
    mov gen[15].32B, gen[0].32B         ; v = v*10 + c
    shiftli gen[0].32B, 3
    shiftli gen[15].32B, 1
    add gen[0].32B, gen[15].32B
    add gen[0].32B, gen[13].32B
    jmp.one zero.32B, read_num_lp
read_num_end:
    ret

; ---------------------------------------------------------------------
;  decimal digits out of the limbs (sfrac, pbig : see the top)
; ---------------------------------------------------------------------

    ; sfrac += pbig * L at limb k-1 (the new one). L = gen[1] < 2^16
    ; gen[0] = carry out of sfrac (0 or 1)
    ; gen[2] = pbig offset, gen[3] = sfrac offset, gen[4] = mul carry,
    ; gen[13] = add carry, gen[14] = pbig size, gen[15..17] = mul
dec_addlimb:
    ld.zero gen[3].32B, g_kb
    subi gen[3].32B, 4
    ld.zero gen[14].32B, g_lenpb
    set gen[2].32B, 0
    set gen[4].32B, 0
    set gen[13].32B, 0
dal_lp:
    cmp gen[2].32B, gen[14].32B
    jmp.zero zero.32B, dal_tail
    ld.gen[2] gen[15].32B, pbig         ; p = pbig[i] * L + mul carry
    mov gen[16].32B, gen[4].32B
    mov gen[17].32B, gen[1].32B
dal_mul:
    cmpi gen[17].32B, 0
    jmp.zero zero.32B, dal_muld
    testi gen[17].32B, 1
    jmp.zero zero.32B, dal_mul2
    add gen[16].32B, gen[15].32B
dal_mul2:
    add gen[15].32B, gen[15].32B
    shiftri gen[17].32B, 1
    jmp.one zero.32B, dal_mul
dal_muld:
    mov gen[4].32B, gen[16].32B
    shiftri gen[4].32B, 16
    bndi gen[16].32B, 0xFFFF
    ld.gen[3] gen[15].32B, sfrac        ; sfrac[s] += low16(p) + carry
    add gen[15].32B, gen[16].32B
    add gen[15].32B, gen[13].32B
    mov gen[13].32B, gen[15].32B
    shiftri gen[13].32B, 16
    bndi gen[15].32B, 0xFFFF
    st.gen[3] gen[15].32B, sfrac
    subi gen[3].32B, 4
    addi gen[2].32B, 4
    jmp.one zero.32B, dal_lp
dal_tail:                               ; last mul carry + add carry, up
    add gen[13].32B, gen[4].32B
dal_prop:
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, dal_end
    cmpi gen[3].32B, 0
    jmp.neg zero.32B, dal_end           ; out of sfrac : carry out
    ld.gen[3] gen[15].32B, sfrac
    add gen[15].32B, gen[13].32B
    mov gen[13].32B, gen[15].32B
    shiftri gen[13].32B, 16
    bndi gen[15].32B, 0xFFFF
    st.gen[3] gen[15].32B, sfrac
    subi gen[3].32B, 4
    jmp.one zero.32B, dal_prop
dal_end:
    mov gen[0].32B, gen[13].32B
    ret

    ; sfrac *= 10, gen[0] = integer part (the next digit), pbig *= 10
    ; gen[2] = offset, gen[3] = pbig size, gen[13] = carry
dec_emit:
    ld.zero gen[2].32B, g_kb
    subi gen[2].32B, 4
    set gen[13].32B, 0
de_s:
    ld.gen[2] gen[14].32B, sfrac
    mov gen[15].32B, gen[14].32B
    shiftli gen[14].32B, 3
    shiftli gen[15].32B, 1
    add gen[14].32B, gen[15].32B
    add gen[14].32B, gen[13].32B
    mov gen[13].32B, gen[14].32B
    shiftri gen[13].32B, 16
    bndi gen[14].32B, 0xFFFF
    st.gen[2] gen[14].32B, sfrac
    cmpi gen[2].32B, 0
    jmp.zero zero.32B, de_p
    subi gen[2].32B, 4
    jmp.one zero.32B, de_s
de_p:
    mov gen[0].32B, gen[13].32B
    set gen[2].32B, 0
    ld.zero gen[3].32B, g_lenpb
    set gen[13].32B, 0
de_p_lp:
    cmp gen[2].32B, gen[3].32B
    jmp.zero zero.32B, de_p_end
    ld.gen[2] gen[14].32B, pbig
    mov gen[15].32B, gen[14].32B
    shiftli gen[14].32B, 3
    shiftli gen[15].32B, 1
    add gen[14].32B, gen[15].32B
    add gen[14].32B, gen[13].32B
    mov gen[13].32B, gen[14].32B
    shiftri gen[13].32B, 16
    bndi gen[14].32B, 0xFFFF
    st.gen[2] gen[14].32B, pbig
    addi gen[2].32B, 4
    jmp.one zero.32B, de_p_lp
de_p_end:
    cmpi gen[13].32B, 0                 ; grows by a limb
    jmp.zero zero.32B, de_end
    st.gen[2] gen[13].32B, pbig
    addi gen[3].32B, 4
    st.zero gen[3].32B, g_lenpb
de_end:
    ret

    ; next digit gen[1] : a 9 is held, anything else prints the held
    ; digit and 9s and is held itself
dig_in:
    cmpi gen[1].32B, 9
    ijmp.zero zero.32B, di_new
    ld.zero gen[13].32B, g_n9
    addi gen[13].32B, 1
    st.zero gen[13].32B, g_n9
    ret
di_new:
    push gen[5].32B
    mov gen[5].32B, gen[1].32B
    call zero.32B, dig_flush
    st.zero gen[5].32B, g_h
    pop gen[5].32B
    ret

    ; carry into the digits out : held h 9 .. 9 becomes h+1 0 .. 0
    ; (the last 0 stays held)
dig_carry:
    push gen[5].32B
    ld.zero gen[1].32B, g_h
    addi gen[1].32B, 1
    st.zero gen[1].32B, g_h
    ld.zero gen[5].32B, g_n9
    cmpi gen[5].32B, 0
    jmp.zero zero.32B, dc_end
    call zero.32B, put_dig              ; h+1
    subi gen[5].32B, 1
dc_lp:
    cmpi gen[5].32B, 0
    jmp.zero zero.32B, dc_held
    set gen[1].32B, 0
    call zero.32B, put_dig
    subi gen[5].32B, 1
    jmp.one zero.32B, dc_lp
dc_held:
    st.zero zero.32B, g_h
    st.zero zero.32B, g_n9
dc_end:
    pop gen[5].32B
    ret

    ; print the held digit and 9s, nothing held after
dig_flush:
    push gen[5].32B
    ld.zero gen[1].32B, g_h
    cmpi gen[1].32B, 0
    jmp.neg zero.32B, df_9
    call zero.32B, put_dig
df_9:
    ld.zero gen[5].32B, g_n9
df_lp:
    cmpi gen[5].32B, 0
    jmp.zero zero.32B, df_end
    set gen[1].32B, 9
    call zero.32B, put_dig
    subi gen[5].32B, 1
    jmp.one zero.32B, df_lp
df_end:
    st.zero zero.32B, g_n9
    st.zero full.32B, g_h
    pop gen[5].32B
    ret

    ; digit gen[1] at index g_idx (then g_idx + 1). printed only for
    ; pos <= index < n, with "index: " line labels and group spaces
put_dig:
    push gen[5].32B
    push gen[6].32B
    mov gen[5].32B, gen[1].32B
    ld.zero gen[6].32B, g_idx
    mov gen[13].32B, gen[6].32B
    addi gen[13].32B, 1
    st.zero gen[13].32B, g_idx
    ld.zero gen[13].32B, g_pos
    cmp gen[6].32B, gen[13].32B
    jmp.neg zero.32B, pd_end
    ld.zero gen[13].32B, g_n
    cmp gen[6].32B, gen[13].32B
    ijmp.neg zero.32B, pd_end
    ld.zero gen[13].32B, g_line
    cmpi gen[13].32B, 0
    ijmp.zero zero.32B, pd_sep
    mov gen[1].32B, gen[6].32B          ; line label "index: "
    call zero.32B, io_printint
    set gen[1].32B, s_colon
    call zero.32B, io_printb
    set gen[13].32B, GROUP
    st.zero gen[13].32B, g_grp
    jmp.one zero.32B, pd_put
pd_sep:
    ld.zero gen[13].32B, g_grp          ; space every GROUP digits
    cmpi gen[13].32B, 0
    ijmp.zero zero.32B, pd_put
    set gen[1].32B, ' '
    call zero.32B, io_putc
    set gen[13].32B, GROUP
    st.zero gen[13].32B, g_grp
pd_put:
    mov gen[1].32B, gen[5].32B
    addi gen[1].32B, '0'
    call zero.32B, io_putc
    ld.zero gen[13].32B, g_grp
    subi gen[13].32B, 1
    st.zero gen[13].32B, g_grp
    ld.zero gen[13].32B, g_line
    addi gen[13].32B, 1
    st.zero gen[13].32B, g_line
    cmpi gen[13].32B, PERLINE
    ijmp.zero zero.32B, pd_end
    st.zero zero.32B, g_line
    call zero.32B, io_newline
pd_end:
    pop gen[6].32B
    pop gen[5].32B
    ret

; ---------------------------------------------------------------------
;  BBP
; ---------------------------------------------------------------------

    ; gen[0] = frac(16^d pi) * 2^32 for d = gen[1]
    ; = 4 S1 - 2 S4 - S5 - S6 (mod 2^32). gen[5] = d, gen[6] = acc
pi_frac:
    push gen[5].32B
    push gen[6].32B
    mov gen[5].32B, gen[1].32B
    set gen[2].32B, 1
    call zero.32B, pi_series
    mov gen[6].32B, gen[0].32B
    shiftli gen[6].32B, 2
    mov gen[1].32B, gen[5].32B
    set gen[2].32B, 4
    call zero.32B, pi_series
    shiftli gen[0].32B, 1
    sub gen[6].32B, gen[0].32B
    mov gen[1].32B, gen[5].32B
    set gen[2].32B, 5
    call zero.32B, pi_series
    sub gen[6].32B, gen[0].32B
    mov gen[1].32B, gen[5].32B
    set gen[2].32B, 6
    call zero.32B, pi_series
    sub gen[6].32B, gen[0].32B
    mov gen[0].32B, gen[6].32B
    pop gen[6].32B
    pop gen[5].32B
    ret

    ; gen[0] = S_j * 2^32 (mod 2^32) for d = gen[1], j = gen[2]
    ; gen[5] = e = d-k, gen[6] = m = 8k+j, gen[7] = sum, gen[8] = shift
pi_series:
    push gen[5].32B
    push gen[6].32B
    push gen[7].32B
    push gen[8].32B
    mov gen[5].32B, gen[1].32B
    mov gen[6].32B, gen[2].32B
    set gen[7].32B, 0
pi_series_head:                         ; k = 0..d : (16^e mod m) / m
    mov gen[1].32B, gen[5].32B
    mov gen[2].32B, gen[6].32B
    call zero.32B, pi_powmod
    mov gen[1].32B, gen[0].32B
    mov gen[2].32B, gen[6].32B
    call zero.32B, pi_fracdiv
    add gen[7].32B, gen[0].32B
    addi gen[6].32B, 8
    subi gen[5].32B, 1
    cmpi gen[5].32B, 0
    ijmp.neg zero.32B, pi_series_head
    set gen[8].32B, 4
pi_series_tail:                         ; k > d : 16^-(k-d) / m, 7 terms
    set gen[1].32B, 1
    mov gen[2].32B, gen[6].32B
    call zero.32B, pi_fracdiv
    shiftr gen[0].32B, gen[8].32B
    add gen[7].32B, gen[0].32B
    addi gen[6].32B, 8
    addi gen[8].32B, 4
    cmpi gen[8].32B, 32
    ijmp.zero zero.32B, pi_series_tail
    mov gen[0].32B, gen[7].32B
    pop gen[8].32B
    pop gen[7].32B
    pop gen[6].32B
    pop gen[5].32B
    ret

    ; gen[0] = 16^e mod m, e = gen[1] >= 0, m = gen[2] (1 <= m < 2^30)
    ; left to right : r = r^2 for every bit of e, r = 16 r if it is set.
    ; r^2 is shift-add over the bits of r (r < m, so bits of m suffice).
    ; gen[13] = r, gen[14] = bit of e, gen[15] = top bit of m,
    ; gen[16] = t, gen[17] = bit of r, gen[18] = counter
pi_powmod:
    set gen[13].32B, 1
    cmpi gen[2].32B, 1
    ijmp.zero zero.32B, pi_pm_m
    set gen[13].32B, 0                  ; mod 1 : 0
    jmp.one zero.32B, pi_pm_end
pi_pm_m:
    set gen[15].32B, 1                  ; top bit of m
pi_pm_mtop:
    mov gen[16].32B, gen[15].32B
    shiftli gen[16].32B, 1
    cmp gen[2].32B, gen[16].32B
    jmp.neg zero.32B, pi_pm_e
    mov gen[15].32B, gen[16].32B
    jmp.one zero.32B, pi_pm_mtop
pi_pm_e:
    cmpi gen[1].32B, 0
    jmp.zero zero.32B, pi_pm_end        ; 16^0 = 1
    set gen[14].32B, 1                  ; top bit of e
pi_pm_etop:
    mov gen[16].32B, gen[14].32B
    shiftli gen[16].32B, 1
    cmp gen[1].32B, gen[16].32B
    jmp.neg zero.32B, pi_pm_bit
    mov gen[14].32B, gen[16].32B
    jmp.one zero.32B, pi_pm_etop

pi_pm_bit:                              ; r = r^2 mod m
    set gen[16].32B, 0
    mov gen[17].32B, gen[15].32B
pi_pm_sq:
    add gen[16].32B, gen[16].32B
    cmp gen[16].32B, gen[2].32B
    jmp.neg zero.32B, pi_pm_sq1
    sub gen[16].32B, gen[2].32B
pi_pm_sq1:
    test gen[13].32B, gen[17].32B
    jmp.zero zero.32B, pi_pm_sq2
    add gen[16].32B, gen[13].32B
    cmp gen[16].32B, gen[2].32B
    jmp.neg zero.32B, pi_pm_sq2
    sub gen[16].32B, gen[2].32B
pi_pm_sq2:
    shiftri gen[17].32B, 1
    cmpi gen[17].32B, 0
    ijmp.zero zero.32B, pi_pm_sq
    mov gen[13].32B, gen[16].32B

    test gen[1].32B, gen[14].32B        ; bit set : r = 16 r mod m
    jmp.zero zero.32B, pi_pm_next
    set gen[18].32B, 4
pi_pm_x16:
    add gen[13].32B, gen[13].32B
    cmp gen[13].32B, gen[2].32B
    jmp.neg zero.32B, pi_pm_x16b
    sub gen[13].32B, gen[2].32B
pi_pm_x16b:
    subi gen[18].32B, 1
    cmpi gen[18].32B, 0
    ijmp.zero zero.32B, pi_pm_x16
pi_pm_next:
    shiftri gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, pi_pm_bit
pi_pm_end:
    mov gen[0].32B, gen[13].32B
    ret

    ; gen[0] = floor(r * 2^32 / m), r = gen[1] < m = gen[2] < 2^30
    ; restoring division, 32 quotient bits. gen[13] = counter
pi_fracdiv:
    set gen[0].32B, 0
    set gen[13].32B, 32
pi_fd_lp:
    add gen[1].32B, gen[1].32B
    add gen[0].32B, gen[0].32B
    cmp gen[1].32B, gen[2].32B
    jmp.neg zero.32B, pi_fd_next
    sub gen[1].32B, gen[2].32B
    addi gen[0].32B, 1
pi_fd_next:
    subi gen[13].32B, 1
    cmpi gen[13].32B, 0
    ijmp.zero zero.32B, pi_fd_lp
    ret
