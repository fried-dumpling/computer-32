; =====================================================================
;  float.asm : IEEE-754 binary32 software floating point library
;              (implementation, API and usage : float.hsm)
;  assemble separately : dasm floatingpoint/float.asm -o float.o
;  the first instruction jumps over the library, so the object may also
;  be linked first, but link the main program first anyway.
; =====================================================================

#define FP_IMPL 1
#include<float.hsm>

global fp_add
global fp_sub
global fp_mul
global fp_div
global fp_str2f
global fp_f2str

#define FP_BIGN 14              ; bigint limbs (16 bit payload / word)
#define FP_BIGSZ (4*14)
#define FP_MAXDIG 12            ; exact decimal digits kept by fp_str2f

; reg = (hi << 16) | lo. same order as add_terminal.txt (proven on the
; circuit) : S16H first, then 16L writes the lower half only
#macro FP_SET 3
    set #1.S16H, #2
    set #1.16L, #3
#end

; *out++ = reg   (out ptr = gen[5], used by fp_f2str)
#macro FP_PUTC 1
    st.gen[5] #1.32B, 0
    addi gen[5].32B, 4
#end

#macro FP_SAVE 0
    push gen[5].32B
    push gen[6].32B
    push gen[7].32B
    push gen[8].32B
    push gen[9].32B
    push gen[10].32B
    push gen[11].32B
    push gen[12].32B
#end

#macro FP_RESTORE 0
    pop gen[12].32B
    pop gen[11].32B
    pop gen[10].32B
    pop gen[9].32B
    pop gen[8].32B
    pop gen[7].32B
    pop gen[6].32B
    pop gen[5].32B
#end

.data
fp_pow10:
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
fp_bigA:    FP_BIGSZ, 0
fp_bigB:    FP_BIGSZ, 0
fp_digits:  4*10, 0

.text
    jmp.one zero.32B, fp_lib_end

; =====================================================================
;  bigint helpers : little endian, FP_BIGN limbs of 16 bit
;  gen[1] = ptr A, gen[2] = ptr B / value, destroys gen[13..18]
; =====================================================================

fp_big_zero:
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, FP_BIGN
fp_big_zero_lp:
    st.gen[13] zero.32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, fp_big_zero_lp
    ret

    ; A = gen[2] (32 bit)
fp_big_set32:
    call zero.32B, fp_big_zero
    mov gen[13].32B, gen[2].32B
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 0
    mov gen[13].32B, gen[2].32B
    shiftri gen[13].32B, 16
    bndi gen[13].32B, 0xFFFF
    st.gen[1] gen[13].32B, 4
    ret

    ; A = A * 10
fp_big_mul10:
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, FP_BIGN
    set gen[15].32B, 0                  ; carry
fp_big_mul10_lp:
    ld.gen[13] gen[16].32B, 0
    mov gen[17].32B, gen[16].32B
    shiftli gen[16].32B, 3
    shiftli gen[17].32B, 1
    add gen[16].32B, gen[17].32B
    add gen[16].32B, gen[15].32B        ; t = limb*10 + carry
    mov gen[15].32B, gen[16].32B
    shiftri gen[15].32B, 16
    bndi gen[16].32B, 0xFFFF
    st.gen[13] gen[16].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, fp_big_mul10_lp
    ret

    ; A = A * 10^gen[2]  (gen[2] >= 0)
fp_big_mul10n:
    mov gen[18].32B, gen[2].32B
fp_big_mul10n_lp:
    cmpi gen[18].32B, 0
    jmp.zero zero.32B, fp_big_mul10n_end
    call zero.32B, fp_big_mul10
    subi gen[18].32B, 1
    jmp.one zero.32B, fp_big_mul10n_lp
fp_big_mul10n_end:
    ret

    ; A = A + gen[2]  (gen[2] < 2^16)
fp_big_addsmall:
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, FP_BIGN
    mov gen[15].32B, gen[2].32B         ; carry
fp_big_addsmall_lp:
    ld.gen[13] gen[16].32B, 0
    add gen[16].32B, gen[15].32B
    mov gen[15].32B, gen[16].32B
    shiftri gen[15].32B, 16
    bndi gen[16].32B, 0xFFFF
    st.gen[13] gen[16].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, fp_big_addsmall_lp
    ret

    ; A = A << 1
fp_big_shl1:
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, FP_BIGN
    set gen[15].32B, 0                  ; carry
fp_big_shl1_lp:
    ld.gen[13] gen[16].32B, 0
    shiftli gen[16].32B, 1
    bor gen[16].32B, gen[15].32B
    mov gen[15].32B, gen[16].32B
    shiftri gen[15].32B, 16
    bndi gen[16].32B, 0xFFFF
    st.gen[13] gen[16].32B, 0
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, fp_big_shl1_lp
    ret

    ; A = A << gen[2]  (gen[2] >= 0)
fp_big_shln:
    mov gen[18].32B, gen[2].32B
fp_big_shln_lp:
    cmpi gen[18].32B, 0
    jmp.zero zero.32B, fp_big_shln_end
    call zero.32B, fp_big_shl1
    subi gen[18].32B, 1
    jmp.one zero.32B, fp_big_shln_lp
fp_big_shln_end:
    ret

    ; gen[0] = 1 (A > B), 0 (A == B), -1 (A < B)
fp_big_cmp:
    mov gen[13].32B, gen[1].32B
    addi gen[13].32B, 4*(FP_BIGN-1)
    mov gen[14].32B, gen[2].32B
    addi gen[14].32B, 4*(FP_BIGN-1)
    set gen[15].32B, FP_BIGN
fp_big_cmp_lp:
    ld.gen[13] gen[16].32B, 0
    ld.gen[14] gen[17].32B, 0
    cmp gen[16].32B, gen[17].32B
    jmp.pos zero.32B, fp_big_cmp_gt
    jmp.neg zero.32B, fp_big_cmp_lt
    subi gen[13].32B, 4
    subi gen[14].32B, 4
    subi gen[15].32B, 1
    cmpi gen[15].32B, 0
    ijmp.zero zero.32B, fp_big_cmp_lp
    set gen[0].32B, 0
    ret
fp_big_cmp_gt:
    set gen[0].32B, 1
    ret
fp_big_cmp_lt:
    mov gen[0].32B, full.32B
    ret

    ; A = A - B  (A >= B)
fp_big_sub:
    mov gen[13].32B, gen[1].32B
    mov gen[14].32B, gen[2].32B
    set gen[15].32B, FP_BIGN
    set gen[18].32B, 0                  ; borrow
fp_big_sub_lp:
    ld.gen[13] gen[16].32B, 0
    ld.gen[14] gen[17].32B, 0
    add gen[17].32B, gen[18].32B
    sub gen[16].32B, gen[17].32B        ; t = a - b - borrow
    set gen[18].32B, 0
    cmpi gen[16].32B, 0
    ijmp.neg zero.32B, fp_big_sub_nb
    set gen[18].32B, 1
fp_big_sub_nb:
    bndi gen[16].32B, 0xFFFF
    st.gen[13] gen[16].32B, 0
    addi gen[13].32B, 4
    addi gen[14].32B, 4
    subi gen[15].32B, 1
    cmpi gen[15].32B, 0
    ijmp.zero zero.32B, fp_big_sub_lp
    ret

    ; gen[0] = 1 if A == 0 else 0
fp_big_iszero:
    mov gen[13].32B, gen[1].32B
    set gen[14].32B, FP_BIGN
fp_big_iszero_lp:
    ld.gen[13] gen[16].32B, 0
    cmpi gen[16].32B, 0
    ijmp.zero zero.32B, fp_big_iszero_no
    addi gen[13].32B, 4
    subi gen[14].32B, 1
    cmpi gen[14].32B, 0
    ijmp.zero zero.32B, fp_big_iszero_lp
    set gen[0].32B, 1
    ret
fp_big_iszero_no:
    set gen[0].32B, 0
    ret

; =====================================================================
;  fp_bigdiv : fp_bigA / fp_bigB  (both nonzero, both destroyed)
;  out: gen[0] = q (31 bit, q[30] = 1), gen[3] = E, gen[4] = sticky
;       A / B = (q + frac) * 2^(E-30), sticky = (frac != 0)
;  destroys gen[1..4], gen[13..20]
; =====================================================================
fp_bigdiv:
    set gen[3].32B, 0                   ; E
fp_bigdiv_p1:                           ; while A >= B: B <<= 1, E++
    set gen[1].32B, fp_bigA
    set gen[2].32B, fp_bigB
    call zero.32B, fp_big_cmp
    cmpi gen[0].32B, 0
    jmp.neg zero.32B, fp_bigdiv_p2
    set gen[1].32B, fp_bigB
    call zero.32B, fp_big_shl1
    addi gen[3].32B, 1
    jmp.one zero.32B, fp_bigdiv_p1
fp_bigdiv_p2:                           ; do A <<= 1, E-- while A < B
    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_shl1
    subi gen[3].32B, 1
    set gen[1].32B, fp_bigA
    set gen[2].32B, fp_bigB
    call zero.32B, fp_big_cmp
    cmpi gen[0].32B, 0
    jmp.neg zero.32B, fp_bigdiv_p2
                                        ; now B <= A < 2B
    set gen[19].32B, 0                  ; q
    set gen[20].32B, 31                 ; bit count
fp_bigdiv_lp:
    shiftli gen[19].32B, 1
    set gen[1].32B, fp_bigA
    set gen[2].32B, fp_bigB
    call zero.32B, fp_big_cmp
    cmpi gen[0].32B, 0
    jmp.neg zero.32B, fp_bigdiv_skip
    set gen[1].32B, fp_bigA
    set gen[2].32B, fp_bigB
    call zero.32B, fp_big_sub
    bori gen[19].32B, 1
fp_bigdiv_skip:
    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_shl1
    subi gen[20].32B, 1
    cmpi gen[20].32B, 0
    ijmp.zero zero.32B, fp_bigdiv_lp

    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_iszero
    bxri gen[0].32B, 1
    mov gen[4].32B, gen[0].32B          ; sticky = remainder != 0
    mov gen[0].32B, gen[19].32B
    ret

; =====================================================================
;  fp_unpack : gen[1] = float
;  out: gen[0] = class (0 zero, 1 finite, 2 inf, 3 nan)
;       gen[13] = sign, gen[14] = exp, gen[15] = mant
;       finite : value = mant * 2^(exp-150), mant in [2^23, 2^24)
;                (subnormals are normalized, exp may be <= 0)
;  destroys gen[16]
; =====================================================================
fp_unpack:
    mov gen[13].32B, gen[1].32B
    shiftri gen[13].32B, 31
    bndi gen[13].32B, 1
    mov gen[14].32B, gen[1].32B
    shiftri gen[14].32B, 23
    bndi gen[14].32B, 0xFF
    FP_SET(gen[16], 0x007F, 0xFFFF)
    mov gen[15].32B, gen[1].32B
    bnd gen[15].32B, gen[16].32B
    FP_SET(gen[16], 0x0080, 0x0000)
    cmpi gen[14].32B, 0xFF
    jmp.zero zero.32B, fp_unpack_special
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, fp_unpack_sub
    bor gen[15].32B, gen[16].32B        ; hidden bit
    set gen[0].32B, 1
    ret
fp_unpack_sub:
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, fp_unpack_zero
    set gen[14].32B, 1
fp_unpack_norm:
    cmp gen[15].32B, gen[16].32B
    ijmp.neg zero.32B, fp_unpack_fin
    shiftli gen[15].32B, 1
    subi gen[14].32B, 1
    jmp.one zero.32B, fp_unpack_norm
fp_unpack_fin:
    set gen[0].32B, 1
    ret
fp_unpack_zero:
    set gen[0].32B, 0
    ret
fp_unpack_special:
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, fp_unpack_inf
    set gen[0].32B, 3
    ret
fp_unpack_inf:
    set gen[0].32B, 2
    ret

; =====================================================================
;  fp_pack : round & pack
;  in : gen[13] = sign, gen[14] = biased exp, gen[15] = q (0 <= q < 2^31)
;       value = q * 2^(exp-157), q's low bits may hold a sticky bit
;  out: gen[0] = float, destroys gen[13..17]
; =====================================================================
fp_pack:
    cmpi gen[15].32B, 0
    jmp.zero zero.32B, fp_pack_zero
    FP_SET(gen[16], 0x4000, 0x0000)
fp_pack_norm:                           ; normalize q[30] = 1
    cmp gen[15].32B, gen[16].32B
    ijmp.neg zero.32B, fp_pack_normd
    shiftli gen[15].32B, 1
    subi gen[14].32B, 1
    jmp.one zero.32B, fp_pack_norm
fp_pack_normd:
    cmpi gen[14].32B, 255
    ijmp.neg zero.32B, fp_pack_inf      ; exp >= 255
    cmpi gen[14].32B, 0
    jmp.pos zero.32B, fp_pack_round     ; exp > 0 : normal
                                        ; subnormal : q >>= (1 - exp) with sticky
    set gen[16].32B, 1
    sub gen[16].32B, gen[14].32B
    set gen[14].32B, 0
    cmpi gen[16].32B, 31
    ijmp.neg zero.32B, fp_pack_tiny
    set gen[17].32B, 1
    shiftl gen[17].32B, gen[16].32B
    subi gen[17].32B, 1
    bnd gen[17].32B, gen[15].32B        ; lost bits
    shiftr gen[15].32B, gen[16].32B
    cmpi gen[17].32B, 0
    jmp.zero zero.32B, fp_pack_round
    bori gen[15].32B, 1
    jmp.one zero.32B, fp_pack_round
fp_pack_tiny:
    set gen[15].32B, 1
fp_pack_round:                          ; 7 extra bits, round to nearest even
    mov gen[16].32B, gen[15].32B
    bndi gen[16].32B, 0x7F
    shiftri gen[15].32B, 7
    cmpi gen[16].32B, 0x40
    jmp.neg zero.32B, fp_pack_nornd
    jmp.pos zero.32B, fp_pack_up
    testi gen[15].32B, 1
    jmp.zero zero.32B, fp_pack_nornd
fp_pack_up:
    addi gen[15].32B, 1
fp_pack_nornd:
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, fp_pack_chk      ; subnormal : no hidden bit
    subi gen[14].32B, 1                 ; hidden bit adds the last 1
    shiftli gen[14].32B, 23
    add gen[15].32B, gen[14].32B
fp_pack_chk:
    FP_SET(gen[16], 0x7F80, 0x0000)
    cmp gen[15].32B, gen[16].32B
    ijmp.neg zero.32B, fp_pack_inf      ; rounded up to inf
    jmp.one zero.32B, fp_pack_sign
fp_pack_inf:
    FP_SET(gen[15], 0x7F80, 0x0000)
    jmp.one zero.32B, fp_pack_sign
fp_pack_zero:
    set gen[15].32B, 0
fp_pack_sign:
    shiftli gen[13].32B, 31
    bor gen[15].32B, gen[13].32B
    mov gen[0].32B, gen[15].32B
    ret

    ; gen[0] = gen[13] * gen[14]  (product < 2^32), destroys gen[13], gen[14]
fp_umul:
    set gen[0].32B, 0
fp_umul_lp:
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, fp_umul_end
    testi gen[14].32B, 1
    jmp.zero zero.32B, fp_umul_skip
    add gen[0].32B, gen[13].32B
fp_umul_skip:
    shiftli gen[13].32B, 1
    shiftri gen[14].32B, 1
    jmp.one zero.32B, fp_umul_lp
fp_umul_end:
    ret

; ---------------------------------------------------------------------
;  shared prologue for add/mul/div : unpack gen[1], gen[2]
;  a : gen[3] class, gen[5] sign, gen[6] exp, gen[7] mant, gen[11] raw
;  b : gen[4] class, gen[8] sign, gen[9] exp, gen[10] mant, gen[12] raw
; ---------------------------------------------------------------------
fp_unpack2:
    mov gen[11].32B, gen[1].32B
    mov gen[12].32B, gen[2].32B
    call zero.32B, fp_unpack
    mov gen[3].32B, gen[0].32B
    mov gen[5].32B, gen[13].32B
    mov gen[6].32B, gen[14].32B
    mov gen[7].32B, gen[15].32B
    mov gen[1].32B, gen[12].32B
    call zero.32B, fp_unpack
    mov gen[4].32B, gen[0].32B
    mov gen[8].32B, gen[13].32B
    mov gen[9].32B, gen[14].32B
    mov gen[10].32B, gen[15].32B
    ret

    ; quiet NaN result : a if a is NaN else b  (after fp_unpack2)
fp_ret_nan:
    FP_SET(gen[13], 0x0040, 0x0000)
    mov gen[0].32B, gen[11].32B
    cmpi gen[3].32B, 3
    jmp.zero zero.32B, fp_ret_nan_q
    mov gen[0].32B, gen[12].32B
fp_ret_nan_q:
    bor gen[0].32B, gen[13].32B
    jmp.one zero.32B, fp_ret

fp_ret_dnan:                            ; default NaN (invalid operation)
    FP_SET(gen[0], 0x7FC0, 0x0000)
    jmp.one zero.32B, fp_ret

fp_ret_inf:                             ; +-inf, sign in gen[5]
    FP_SET(gen[0], 0x7F80, 0x0000)
    jmp.one zero.32B, fp_ret_sign
fp_ret_zero:                            ; +-0, sign in gen[5]
    set gen[0].32B, 0
fp_ret_sign:
    mov gen[13].32B, gen[5].32B
    shiftli gen[13].32B, 31
    bor gen[0].32B, gen[13].32B
    jmp.one zero.32B, fp_ret

fp_ret_a:
    mov gen[0].32B, gen[11].32B
    jmp.one zero.32B, fp_ret
fp_ret_b:
    mov gen[0].32B, gen[12].32B
fp_ret:
    FP_RESTORE()
    ret

; =====================================================================
;  fp_add / fp_sub
; =====================================================================
fp_sub:
    FP_SET(gen[13], 0x8000, 0x0000)
    bxr gen[2].32B, gen[13].32B
fp_add:
    FP_SAVE()
    call zero.32B, fp_unpack2
    cmpi gen[3].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    cmpi gen[4].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    cmpi gen[3].32B, 2
    ijmp.zero zero.32B, fp_add_afin
    cmpi gen[4].32B, 2                  ; inf + inf
    ijmp.zero zero.32B, fp_ret_a
    cmp gen[5].32B, gen[8].32B
    jmp.zero zero.32B, fp_ret_a
    jmp.one zero.32B, fp_ret_dnan       ; inf - inf
fp_add_afin:
    cmpi gen[4].32B, 2
    jmp.zero zero.32B, fp_ret_b
    cmpi gen[3].32B, 0
    ijmp.zero zero.32B, fp_add_anz
    cmpi gen[4].32B, 0
    ijmp.zero zero.32B, fp_ret_b
    bnd gen[5].32B, gen[8].32B          ; 0 + 0 : -0 only if both -0
    jmp.one zero.32B, fp_ret_zero
fp_add_anz:
    cmpi gen[4].32B, 0
    jmp.zero zero.32B, fp_ret_a
                                        ; make |a| >= |b|
    cmp gen[6].32B, gen[9].32B
    jmp.pos zero.32B, fp_add_noswap
    jmp.neg zero.32B, fp_add_swap
    cmp gen[7].32B, gen[10].32B
    ijmp.neg zero.32B, fp_add_noswap
fp_add_swap:
    mov gen[13].32B, gen[5].32B
    mov gen[5].32B, gen[8].32B
    mov gen[8].32B, gen[13].32B
    mov gen[13].32B, gen[6].32B
    mov gen[6].32B, gen[9].32B
    mov gen[9].32B, gen[13].32B
    mov gen[13].32B, gen[7].32B
    mov gen[7].32B, gen[10].32B
    mov gen[10].32B, gen[13].32B
fp_add_noswap:
    shiftli gen[7].32B, 6               ; 6 guard bits
    shiftli gen[10].32B, 6
    mov gen[13].32B, gen[6].32B
    sub gen[13].32B, gen[9].32B         ; d = ea - eb
    cmpi gen[13].32B, 31
    ijmp.neg zero.32B, fp_add_far
    set gen[14].32B, 1                  ; mb >>= d with sticky
    shiftl gen[14].32B, gen[13].32B
    subi gen[14].32B, 1
    bnd gen[14].32B, gen[10].32B
    shiftr gen[10].32B, gen[13].32B
    cmpi gen[14].32B, 0
    jmp.zero zero.32B, fp_add_aligned
    bori gen[10].32B, 1
    jmp.one zero.32B, fp_add_aligned
fp_add_far:
    set gen[10].32B, 1
fp_add_aligned:
    cmp gen[5].32B, gen[8].32B
    jmp.zero zero.32B, fp_add_same
    sub gen[7].32B, gen[10].32B
    cmpi gen[7].32B, 0
    ijmp.zero zero.32B, fp_add_pack
    set gen[5].32B, 0                   ; exact cancel : +0
    jmp.one zero.32B, fp_ret_zero
fp_add_same:
    add gen[7].32B, gen[10].32B
fp_add_pack:
    mov gen[13].32B, gen[5].32B
    mov gen[14].32B, gen[6].32B
    addi gen[14].32B, 1
    mov gen[15].32B, gen[7].32B
    call zero.32B, fp_pack
    jmp.one zero.32B, fp_ret

; =====================================================================
;  fp_mul
; =====================================================================
fp_mul:
    FP_SAVE()
    call zero.32B, fp_unpack2
    cmpi gen[3].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    cmpi gen[4].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    bxr gen[5].32B, gen[8].32B          ; sign
    cmpi gen[3].32B, 2
    jmp.zero zero.32B, fp_mul_inf
    cmpi gen[4].32B, 2
    jmp.zero zero.32B, fp_mul_inf
    cmpi gen[3].32B, 0
    jmp.zero zero.32B, fp_ret_zero
    cmpi gen[4].32B, 0
    jmp.zero zero.32B, fp_ret_zero
    jmp.one zero.32B, fp_mul_fin
fp_mul_inf:                             ; inf * 0 = NaN
    cmpi gen[3].32B, 0
    jmp.zero zero.32B, fp_ret_dnan
    cmpi gen[4].32B, 0
    jmp.zero zero.32B, fp_ret_dnan
    jmp.one zero.32B, fp_ret_inf
fp_mul_fin:
    add gen[6].32B, gen[9].32B
    subi gen[6].32B, 126                ; exp = ea + eb - 126
                                        ; split mant to 12 bit halves
    mov gen[3].32B, gen[7].32B
    bndi gen[3].32B, 0xFFF              ; al
    mov gen[4].32B, gen[7].32B
    shiftri gen[4].32B, 12              ; ah
    mov gen[11].32B, gen[10].32B
    bndi gen[11].32B, 0xFFF             ; bl
    mov gen[12].32B, gen[10].32B
    shiftri gen[12].32B, 12             ; bh

    mov gen[13].32B, gen[3].32B
    mov gen[14].32B, gen[11].32B
    call zero.32B, fp_umul
    mov gen[19].32B, gen[0].32B         ; p0 = al*bl
    mov gen[13].32B, gen[4].32B
    mov gen[14].32B, gen[11].32B
    call zero.32B, fp_umul
    mov gen[20].32B, gen[0].32B
    mov gen[13].32B, gen[3].32B
    mov gen[14].32B, gen[12].32B
    call zero.32B, fp_umul
    add gen[20].32B, gen[0].32B         ; p1 = ah*bl + al*bh
    mov gen[13].32B, gen[4].32B
    mov gen[14].32B, gen[12].32B
    call zero.32B, fp_umul
    mov gen[9].32B, gen[0].32B          ; p2 = ah*bh

    mov gen[13].32B, gen[20].32B
    bndi gen[13].32B, 0xFFF
    shiftli gen[13].32B, 12
    add gen[19].32B, gen[13].32B        ; Lraw = p0 + (p1 & 0xFFF) << 12
    shiftri gen[20].32B, 12
    add gen[9].32B, gen[20].32B
    mov gen[13].32B, gen[19].32B
    shiftri gen[13].32B, 24
    add gen[9].32B, gen[13].32B         ; H = p2 + p1 >> 12 + Lraw >> 24
    FP_SET(gen[13], 0x00FF, 0xFFFF)
    bnd gen[19].32B, gen[13].32B        ; L = Lraw & 0xFFFFFF

    mov gen[15].32B, gen[9].32B
    shiftli gen[15].32B, 7
    mov gen[13].32B, gen[19].32B
    shiftri gen[13].32B, 17
    bor gen[15].32B, gen[13].32B        ; q = H << 7 | L >> 17
    FP_SET(gen[13], 0x0001, 0xFFFF)
    bnd gen[19].32B, gen[13].32B
    cmpi gen[19].32B, 0
    jmp.zero zero.32B, fp_mul_pack
    bori gen[15].32B, 1                 ; sticky
fp_mul_pack:
    mov gen[13].32B, gen[5].32B
    mov gen[14].32B, gen[6].32B
    call zero.32B, fp_pack
    jmp.one zero.32B, fp_ret

; =====================================================================
;  fp_div
; =====================================================================
fp_div:
    FP_SAVE()
    call zero.32B, fp_unpack2
    cmpi gen[3].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    cmpi gen[4].32B, 3
    jmp.zero zero.32B, fp_ret_nan
    bxr gen[5].32B, gen[8].32B          ; sign
    cmpi gen[3].32B, 2
    ijmp.zero zero.32B, fp_div_afin
    cmpi gen[4].32B, 2
    jmp.zero zero.32B, fp_ret_dnan      ; inf / inf
    jmp.one zero.32B, fp_ret_inf
fp_div_afin:
    cmpi gen[4].32B, 2
    jmp.zero zero.32B, fp_ret_zero      ; x / inf
    cmpi gen[4].32B, 0
    ijmp.zero zero.32B, fp_div_bnz
    cmpi gen[3].32B, 0
    jmp.zero zero.32B, fp_ret_dnan      ; 0 / 0
    jmp.one zero.32B, fp_ret_inf        ; x / 0
fp_div_bnz:
    cmpi gen[3].32B, 0
    jmp.zero zero.32B, fp_ret_zero
    sub gen[6].32B, gen[9].32B
    addi gen[6].32B, 127                ; exp = ea - eb + 127
    cmp gen[7].32B, gen[10].32B
    ijmp.neg zero.32B, fp_div_go
    shiftli gen[7].32B, 1               ; ma < mb : ma <<= 1
    subi gen[6].32B, 1
fp_div_go:
    set gen[15].32B, 0                  ; q
    set gen[3].32B, 31
fp_div_lp:
    shiftli gen[15].32B, 1
    cmp gen[7].32B, gen[10].32B
    jmp.neg zero.32B, fp_div_skip
    sub gen[7].32B, gen[10].32B
    bori gen[15].32B, 1
fp_div_skip:
    shiftli gen[7].32B, 1
    subi gen[3].32B, 1
    cmpi gen[3].32B, 0
    ijmp.zero zero.32B, fp_div_lp
    cmpi gen[7].32B, 0
    jmp.zero zero.32B, fp_div_pack
    bori gen[15].32B, 1                 ; sticky
fp_div_pack:
    mov gen[13].32B, gen[5].32B
    mov gen[14].32B, gen[6].32B
    call zero.32B, fp_pack
    jmp.one zero.32B, fp_ret

; =====================================================================
;  fp_str2f : gen[1] = string ptr -> gen[0] = float, gen[1] = end ptr
;             end = first char not used (like strtod). no number : +0,
;             end = start
;  gen[5] ptr, gen[6] sign, gen[7] digits kept, gen[8] decimal exp,
;  gen[9] dropped nonzero digit, gen[10] char, gen[11] exp value,
;  gen[12] mantissa : bit0 '.' seen, bit1 digit seen / exponent : sign
;  gen[3] first exponent digit ptr, gen[4] end ptr. the bigint calls in
;  the digit loop keep gen[3], gen[4]. fp_s2f_calc moves end to gen[10].
; =====================================================================
fp_str2f:
    FP_SAVE()
    mov gen[5].32B, gen[1].32B
    mov gen[4].32B, gen[1].32B          ; end = start
    set gen[6].32B, 0
    set gen[7].32B, 0
    set gen[8].32B, 0
    set gen[9].32B, 0
    set gen[12].32B, 0
    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_zero
fp_s2f_ws:
    ld.gen[5] gen[10].32B, 0
    cmpi gen[10].32B, ' '
    ijmp.zero zero.32B, fp_s2f_sign
    addi gen[5].32B, 4
    jmp.one zero.32B, fp_s2f_ws
fp_s2f_sign:
    cmpi gen[10].32B, '-'
    ijmp.zero zero.32B, fp_s2f_plus
    set gen[6].32B, 1
    addi gen[5].32B, 4
    jmp.one zero.32B, fp_s2f_word
fp_s2f_plus:
    cmpi gen[10].32B, '+'
    ijmp.zero zero.32B, fp_s2f_word
    addi gen[5].32B, 4
fp_s2f_word:                            ; inf / nan
    ld.gen[5] gen[10].32B, 0
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'i'
    jmp.zero zero.32B, fp_s2f_inf
    cmpi gen[10].32B, 'n'
    jmp.zero zero.32B, fp_s2f_nan

fp_s2f_dig:                             ; mantissa digits
    ld.gen[5] gen[10].32B, 0
    cmpi gen[10].32B, '.'
    ijmp.zero zero.32B, fp_s2f_num
    testi gen[12].32B, 1
    ijmp.zero zero.32B, fp_s2f_mend     ; second '.'
    bori gen[12].32B, 1
    jmp.one zero.32B, fp_s2f_next
fp_s2f_num:
    subi gen[10].32B, '0'
    cmpi gen[10].32B, 0
    jmp.neg zero.32B, fp_s2f_mend
    cmpi gen[10].32B, 10
    ijmp.neg zero.32B, fp_s2f_mend
    bori gen[12].32B, 2                 ; digit seen
    cmpi gen[7].32B, 0
    ijmp.zero zero.32B, fp_s2f_have
    cmpi gen[10].32B, 0
    ijmp.zero zero.32B, fp_s2f_have
    testi gen[12].32B, 1                ; leading zero
    jmp.zero zero.32B, fp_s2f_next
    subi gen[8].32B, 1
    jmp.one zero.32B, fp_s2f_next
fp_s2f_have:
    cmpi gen[7].32B, FP_MAXDIG
    ijmp.neg zero.32B, fp_s2f_extra
    set gen[1].32B, fp_bigA             ; N = N*10 + d
    call zero.32B, fp_big_mul10
    set gen[1].32B, fp_bigA
    mov gen[2].32B, gen[10].32B
    call zero.32B, fp_big_addsmall
    addi gen[7].32B, 1
    testi gen[12].32B, 1
    jmp.zero zero.32B, fp_s2f_next
    subi gen[8].32B, 1
    jmp.one zero.32B, fp_s2f_next
fp_s2f_extra:                           ; digits beyond FP_MAXDIG
    cmpi gen[10].32B, 0
    jmp.zero zero.32B, fp_s2f_extra2
    set gen[9].32B, 1
fp_s2f_extra2:
    testi gen[12].32B, 1
    ijmp.zero zero.32B, fp_s2f_next
    addi gen[8].32B, 1
fp_s2f_next:
    addi gen[5].32B, 4
    jmp.one zero.32B, fp_s2f_dig

fp_s2f_mend:                            ; exponent part
    testi gen[12].32B, 2
    jmp.zero zero.32B, fp_s2f_none      ; no digit at all
    mov gen[4].32B, gen[5].32B          ; end = after the mantissa
    ld.gen[5] gen[10].32B, 0
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'e'
    ijmp.zero zero.32B, fp_s2f_calc
    addi gen[5].32B, 4
    set gen[11].32B, 0
    set gen[12].32B, 0
    ld.gen[5] gen[10].32B, 0
    cmpi gen[10].32B, '-'
    ijmp.zero zero.32B, fp_s2f_eplus
    set gen[12].32B, 1
    addi gen[5].32B, 4
    jmp.one zero.32B, fp_s2f_estart
fp_s2f_eplus:
    cmpi gen[10].32B, '+'
    ijmp.zero zero.32B, fp_s2f_estart
    addi gen[5].32B, 4
fp_s2f_estart:
    mov gen[3].32B, gen[5].32B
fp_s2f_edig:
    ld.gen[5] gen[10].32B, 0
    subi gen[10].32B, '0'
    cmpi gen[10].32B, 0
    jmp.neg zero.32B, fp_s2f_eend
    cmpi gen[10].32B, 10
    ijmp.neg zero.32B, fp_s2f_eend
    addi gen[5].32B, 4
    cmpi gen[11].32B, 1000              ; clamp
    ijmp.neg zero.32B, fp_s2f_edig
    mov gen[13].32B, gen[11].32B
    shiftli gen[11].32B, 3
    shiftli gen[13].32B, 1
    add gen[11].32B, gen[13].32B
    add gen[11].32B, gen[10].32B
    jmp.one zero.32B, fp_s2f_edig
fp_s2f_eend:
    cmp gen[5].32B, gen[3].32B
    jmp.zero zero.32B, fp_s2f_calc      ; no exponent digit : 'e' not used
    mov gen[4].32B, gen[5].32B
    cmpi gen[12].32B, 0
    jmp.zero zero.32B, fp_s2f_epos
    sub gen[8].32B, gen[11].32B
    jmp.one zero.32B, fp_s2f_calc
fp_s2f_epos:
    add gen[8].32B, gen[11].32B

fp_s2f_calc:                            ; value = N * 10^K
    mov gen[10].32B, gen[4].32B         ; end ptr, kept through bigint
    mov gen[5].32B, gen[6].32B          ; sign -> gen[5] for fp_ret_*
    cmpi gen[7].32B, 0
    jmp.zero zero.32B, fp_s2f_zero
    cmpi gen[9].32B, 0                  ; dropped digits : append a 1
    jmp.zero zero.32B, fp_s2f_range
    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_mul10
    set gen[1].32B, fp_bigA
    set gen[2].32B, 1
    call zero.32B, fp_big_addsmall
    addi gen[7].32B, 1
    subi gen[8].32B, 1
fp_s2f_range:
    mov gen[3].32B, gen[7].32B
    add gen[3].32B, gen[8].32B          ; value in [10^(t-1), 10^t)
    cmpi gen[3].32B, 40
    ijmp.neg zero.32B, fp_s2f_big       ; >= 1e39
    addi gen[3].32B, 46
    cmpi gen[3].32B, 0
    jmp.neg zero.32B, fp_s2f_zero       ; < 1e-46
    jmp.zero zero.32B, fp_s2f_zero
    set gen[1].32B, fp_bigB
    set gen[2].32B, 1
    call zero.32B, fp_big_set32
    cmpi gen[8].32B, 0
    jmp.neg zero.32B, fp_s2f_kneg
    set gen[1].32B, fp_bigA
    mov gen[2].32B, gen[8].32B
    call zero.32B, fp_big_mul10n
    jmp.one zero.32B, fp_s2f_div
fp_s2f_kneg:
    set gen[1].32B, fp_bigB
    set gen[2].32B, 0
    sub gen[2].32B, gen[8].32B
    call zero.32B, fp_big_mul10n
fp_s2f_div:
    call zero.32B, fp_bigdiv
    bor gen[0].32B, gen[4].32B
    mov gen[15].32B, gen[0].32B
    mov gen[14].32B, gen[3].32B
    addi gen[14].32B, 127
    mov gen[13].32B, gen[5].32B
    call zero.32B, fp_pack
    mov gen[1].32B, gen[10].32B
    jmp.one zero.32B, fp_ret
fp_s2f_big:
    mov gen[1].32B, gen[10].32B
    jmp.one zero.32B, fp_ret_inf
fp_s2f_zero:
    mov gen[1].32B, gen[10].32B
    jmp.one zero.32B, fp_ret_zero

fp_s2f_none:                            ; no number : +0, end = start
    mov gen[1].32B, gen[4].32B
    set gen[0].32B, 0
    jmp.one zero.32B, fp_ret

fp_s2f_inf:                             ; "inf", any case
    ld.gen[5] gen[10].32B, 4
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'n'
    ijmp.zero zero.32B, fp_s2f_none
    ld.gen[5] gen[10].32B, 8
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'f'
    ijmp.zero zero.32B, fp_s2f_none
    mov gen[1].32B, gen[5].32B
    addi gen[1].32B, 12
    mov gen[5].32B, gen[6].32B
    jmp.one zero.32B, fp_ret_inf
fp_s2f_nan:                             ; "nan", any case
    ld.gen[5] gen[10].32B, 4
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'a'
    ijmp.zero zero.32B, fp_s2f_none
    ld.gen[5] gen[10].32B, 8
    bori gen[10].32B, 0x20
    cmpi gen[10].32B, 'n'
    ijmp.zero zero.32B, fp_s2f_none
    mov gen[1].32B, gen[5].32B
    addi gen[1].32B, 12
    FP_SET(gen[0], 0x7FC0, 0x0000)
    mov gen[13].32B, gen[6].32B
    shiftli gen[13].32B, 31
    bor gen[0].32B, gen[13].32B
    jmp.one zero.32B, fp_ret

; =====================================================================
;  fp_f2str : gen[1] = float, gen[2] = buffer, gen[3] = digits (1..9)
;  gen[5] out ptr, gen[6] P, gen[7] X (decimal exp), gen[8] mant,
;  gen[9] binary exp E2, gen[10] D, gen[11] last nonzero digit,
;  gen[12] buffer start
; =====================================================================
fp_f2str:
    FP_SAVE()
    mov gen[5].32B, gen[2].32B
    mov gen[12].32B, gen[2].32B
    mov gen[6].32B, gen[3].32B
    cmpi gen[6].32B, 1
    ijmp.neg zero.32B, fp_f2s_p1
    set gen[6].32B, 1
fp_f2s_p1:
    cmpi gen[6].32B, 9
    ijmp.pos zero.32B, fp_f2s_p2
    set gen[6].32B, 9
fp_f2s_p2:
    call zero.32B, fp_unpack
    cmpi gen[0].32B, 3
    jmp.zero zero.32B, fp_f2s_nan
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, fp_f2s_nosign
    set gen[1].32B, '-'
    FP_PUTC(gen[1])
fp_f2s_nosign:
    cmpi gen[0].32B, 2
    jmp.zero zero.32B, fp_f2s_inf
    cmpi gen[0].32B, 0
    jmp.zero zero.32B, fp_f2s_zero
    mov gen[8].32B, gen[15].32B
    mov gen[9].32B, gen[14].32B
    subi gen[9].32B, 150                ; value = mant * 2^E2
                                        ; X = floor((exp-127) * log10(2))
    mov gen[13].32B, gen[14].32B
    subi gen[13].32B, 127
    cmpi gen[13].32B, 0
    jmp.neg zero.32B, fp_f2s_xneg
    set gen[14].32B, 1233
    call zero.32B, fp_umul
    shiftri gen[0].32B, 12
    mov gen[7].32B, gen[0].32B
    jmp.one zero.32B, fp_f2s_try
fp_f2s_xneg:
    set gen[14].32B, 0
    sub gen[14].32B, gen[13].32B
    set gen[13].32B, 1233
    call zero.32B, fp_umul
    addi gen[0].32B, 4095
    shiftri gen[0].32B, 12
    set gen[7].32B, 0
    sub gen[7].32B, gen[0].32B

fp_f2s_try:                             ; D = round(value * 10^(P-1-X))
    set gen[1].32B, fp_bigA
    mov gen[2].32B, gen[8].32B
    call zero.32B, fp_big_set32
    set gen[1].32B, fp_bigB
    set gen[2].32B, 1
    call zero.32B, fp_big_set32
    cmpi gen[9].32B, 0
    jmp.neg zero.32B, fp_f2s_e2neg
    set gen[1].32B, fp_bigA
    mov gen[2].32B, gen[9].32B
    call zero.32B, fp_big_shln
    jmp.one zero.32B, fp_f2s_dec
fp_f2s_e2neg:
    set gen[1].32B, fp_bigB
    set gen[2].32B, 0
    sub gen[2].32B, gen[9].32B
    call zero.32B, fp_big_shln
fp_f2s_dec:
    mov gen[2].32B, gen[6].32B
    subi gen[2].32B, 1
    sub gen[2].32B, gen[7].32B          ; T = P-1-X
    cmpi gen[2].32B, 0
    jmp.neg zero.32B, fp_f2s_tneg
    set gen[1].32B, fp_bigA
    call zero.32B, fp_big_mul10n
    jmp.one zero.32B, fp_f2s_div
fp_f2s_tneg:
    set gen[1].32B, 0
    sub gen[1].32B, gen[2].32B
    mov gen[2].32B, gen[1].32B
    set gen[1].32B, fp_bigB
    call zero.32B, fp_big_mul10n
fp_f2s_div:
    call zero.32B, fp_bigdiv            ; gen[0] q, gen[3] E, gen[4] sticky
    cmpi gen[3].32B, 30
    ijmp.neg zero.32B, fp_f2s_xup       ; D >= 2^30 : X too small
    mov gen[13].32B, gen[3].32B
    addi gen[13].32B, 1
    cmpi gen[13].32B, 0
    jmp.neg zero.32B, fp_f2s_xdown      ; D < 0.5 : X too big
    set gen[13].32B, 30
    sub gen[13].32B, gen[3].32B         ; sh = 30 - E (1..31)
    mov gen[10].32B, gen[0].32B
    shiftr gen[10].32B, gen[13].32B     ; D = q >> sh
    set gen[14].32B, 1
    shiftl gen[14].32B, gen[13].32B
    subi gen[14].32B, 1
    bnd gen[14].32B, gen[0].32B         ; rem
    set gen[15].32B, 1
    subi gen[13].32B, 1
    shiftl gen[15].32B, gen[13].32B     ; half
    cmp gen[14].32B, gen[15].32B
    jmp.neg zero.32B, fp_f2s_rnd
    jmp.pos zero.32B, fp_f2s_rup
    cmpi gen[4].32B, 0
    ijmp.zero zero.32B, fp_f2s_rup
    testi gen[10].32B, 1
    jmp.zero zero.32B, fp_f2s_rnd
fp_f2s_rup:
    addi gen[10].32B, 1
fp_f2s_rnd:
    mov gen[13].32B, gen[6].32B
    shiftli gen[13].32B, 2
    ld.gen[13] gen[14].32B, fp_pow10    ; 10^P
    cmp gen[10].32B, gen[14].32B
    ijmp.neg zero.32B, fp_f2s_xup
    ld.gen[13] gen[14].32B, fp_pow10-4  ; 10^(P-1)
    cmp gen[10].32B, gen[14].32B
    jmp.neg zero.32B, fp_f2s_xdown
    jmp.one zero.32B, fp_f2s_digits
fp_f2s_xup:
    addi gen[7].32B, 1
    jmp.one zero.32B, fp_f2s_try
fp_f2s_xdown:
    subi gen[7].32B, 1
    jmp.one zero.32B, fp_f2s_try

fp_f2s_digits:                          ; D -> fp_digits[0..P-1]
    set gen[1].32B, fp_digits           ; digit ptr
    mov gen[2].32B, gen[6].32B
    shiftli gen[2].32B, 2               ; pow10 ptr offset (k+1)*4
    set gen[11].32B, 0                  ; last nonzero index
    set gen[3].32B, 0                   ; index
fp_f2s_dlp:
    subi gen[2].32B, 4
    ld.gen[2] gen[14].32B, fp_pow10
    set gen[13].32B, 0
fp_f2s_dsub:
    cmp gen[10].32B, gen[14].32B
    jmp.neg zero.32B, fp_f2s_dput
    sub gen[10].32B, gen[14].32B
    addi gen[13].32B, 1
    jmp.one zero.32B, fp_f2s_dsub
fp_f2s_dput:
    st.gen[1] gen[13].32B, 0
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, fp_f2s_dz
    mov gen[11].32B, gen[3].32B
fp_f2s_dz:
    addi gen[1].32B, 4
    addi gen[3].32B, 1
    cmpi gen[2].32B, 0
    ijmp.zero zero.32B, fp_f2s_dlp
                                        ; %g : fixed if -4 <= X < P
    cmp gen[7].32B, gen[6].32B
    ijmp.neg zero.32B, fp_f2s_sci
    mov gen[13].32B, gen[7].32B
    addi gen[13].32B, 4
    cmpi gen[13].32B, 0
    jmp.neg zero.32B, fp_f2s_sci
    cmpi gen[7].32B, 0
    jmp.neg zero.32B, fp_f2s_small
                                        ; fixed, X >= 0
    set gen[1].32B, 0
    mov gen[2].32B, gen[7].32B
    call zero.32B, fp_f2s_put
    cmp gen[11].32B, gen[7].32B
    ijmp.pos zero.32B, fp_f2s_end
    set gen[13].32B, '.'
    FP_PUTC(gen[13])
    mov gen[1].32B, gen[7].32B
    addi gen[1].32B, 1
    mov gen[2].32B, gen[11].32B
    call zero.32B, fp_f2s_put
    jmp.one zero.32B, fp_f2s_end
fp_f2s_small:                           ; fixed, X < 0 : 0.000ddd
    set gen[13].32B, '0'
    FP_PUTC(gen[13])
    set gen[13].32B, '.'
    FP_PUTC(gen[13])
    set gen[14].32B, '0'
    mov gen[13].32B, gen[7].32B
fp_f2s_zlp:
    addi gen[13].32B, 1
    cmpi gen[13].32B, 0
    jmp.zero zero.32B, fp_f2s_zend
    FP_PUTC(gen[14])
    jmp.one zero.32B, fp_f2s_zlp
fp_f2s_zend:
    set gen[1].32B, 0
    mov gen[2].32B, gen[11].32B
    call zero.32B, fp_f2s_put
    jmp.one zero.32B, fp_f2s_end
fp_f2s_sci:                             ; d.ddde+XX
    set gen[1].32B, 0
    set gen[2].32B, 0
    call zero.32B, fp_f2s_put
    cmpi gen[11].32B, 0
    jmp.zero zero.32B, fp_f2s_exp
    set gen[13].32B, '.'
    FP_PUTC(gen[13])
    set gen[1].32B, 1
    mov gen[2].32B, gen[11].32B
    call zero.32B, fp_f2s_put
fp_f2s_exp:
    set gen[13].32B, 'e'
    FP_PUTC(gen[13])
    set gen[13].32B, '+'
    cmpi gen[7].32B, 0
    ijmp.neg zero.32B, fp_f2s_esign
    set gen[13].32B, '-'
    set gen[14].32B, 0
    sub gen[14].32B, gen[7].32B
    mov gen[7].32B, gen[14].32B
fp_f2s_esign:
    FP_PUTC(gen[13])
    set gen[13].32B, '0'
fp_f2s_etens:
    cmpi gen[7].32B, 10
    jmp.neg zero.32B, fp_f2s_eones
    subi gen[7].32B, 10
    addi gen[13].32B, 1
    jmp.one zero.32B, fp_f2s_etens
fp_f2s_eones:
    FP_PUTC(gen[13])
    addi gen[7].32B, '0'
    FP_PUTC(gen[7])
    jmp.one zero.32B, fp_f2s_end

fp_f2s_nan:
    set gen[13].32B, 'n'
    FP_PUTC(gen[13])
    set gen[13].32B, 'a'
    FP_PUTC(gen[13])
    set gen[13].32B, 'n'
    FP_PUTC(gen[13])
    jmp.one zero.32B, fp_f2s_end
fp_f2s_inf:
    set gen[13].32B, 'i'
    FP_PUTC(gen[13])
    set gen[13].32B, 'n'
    FP_PUTC(gen[13])
    set gen[13].32B, 'f'
    FP_PUTC(gen[13])
    jmp.one zero.32B, fp_f2s_end
fp_f2s_zero:
    set gen[13].32B, '0'
    FP_PUTC(gen[13])
fp_f2s_end:
    st.gen[5] zero.32B, 0
    mov gen[0].32B, gen[5].32B
    sub gen[0].32B, gen[12].32B
    shiftri gen[0].32B, 2               ; length
    FP_RESTORE()
    ret

    ; put fp_digits[gen[1] .. gen[2]] as chars, destroys gen[1], gen[13]
fp_f2s_put:
    cmp gen[1].32B, gen[2].32B
    jmp.pos zero.32B, fp_f2s_put_end
    mov gen[13].32B, gen[1].32B
    shiftli gen[13].32B, 2
    ld.gen[13] gen[13].32B, fp_digits
    addi gen[13].32B, '0'
    FP_PUTC(gen[13])
    addi gen[1].32B, 1
    jmp.one zero.32B, fp_f2s_put
fp_f2s_put_end:
    ret

fp_lib_end:
