/******************************************************************************\
* Project:  MSP Simulation Layer for Vector Unit Computational Multiplies      *
* Authors:  Iconoclast                                                         *
* Release:  2018.03.18                                                         *
* License:  CC0 Public Domain Dedication                                       *
*                                                                              *
* To the extent possible under law, the author(s) have dedicated all copyright *
* and related and neighboring rights to this software to the public domain     *
* worldwide. This software is distributed without any warranty.                *
*                                                                              *
* You should have received a copy of the CC0 Public Domain Dedication along    *
* with this software.                                                          *
* If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.             *
\******************************************************************************/

#include "multiply.h"

VECTOR_OPERATION VMULF(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 negative;
    v16 round;
    v16 prod_hi, prod_lo;

/*
 * We cannot save register allocations by doing xmm0 *= xmm1 or xmm1 *= xmm0
 * because we need to do future computations on the original source factors.
 */
    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epi16(vs, vt);

/*
 * The final product is really 2*s*t + 32768.  Fortunately for us, however,
 * no two 16-bit values can cause overflow when <<= 1 the HIGH word, anyway.
 */
    prod_hi = _mm_add_epi16(prod_hi, prod_hi); /* fast way of doing <<= 1 */
    negative = _mm_srli_epi16(prod_lo, 15); /* shifting LOW overflows ? 1 : 0 */
    prod_hi = _mm_add_epi16(prod_hi, negative); /* hi<<1 += MSB of lo */
    prod_lo = _mm_add_epi16(prod_lo, prod_lo); /* fast way of doing <<= 1 */
    negative = _mm_srli_epi16(prod_lo, 15); /* Adding 0x8000 sets MSB to 0? */

/*
 * special fractional round value:  (32-bit product) += 32768 (0x8000)
 * two's compliment computation:  (0xFFFF << 15) & 0xFFFF
 */
    round = _mm_cmpeq_epi16(vs, vs); /* PCMPEQW xmmA, xmmA # all 1's forced */
    round = _mm_slli_epi16(round, 15);

    prod_lo = _mm_xor_si128(prod_lo, round); /* Or += 32768 works also. */
    *(v16 *)VACC_L = prod_lo;
    prod_hi = _mm_add_epi16(prod_hi, negative);
    *(v16 *)VACC_M = prod_hi;

/*
 * VMULF does signed clamping.  However, in VMULF's case, the only possible
 * combination of inputs to even cause a 32-bit signed clamp to a saturated
 * 16-bit result is (-32768 * -32768), so, rather than fully emulating a
 * signed clamp with SSE, we do an accurate-enough hack for this corner case.
 */
    negative = _mm_srai_epi16(prod_hi, 15);
    vs = _mm_cmpeq_epi16(vs, round); /* vs == -32768 ? ~0 : 0 */
    vt = _mm_cmpeq_epi16(vt, round); /* vt == -32768 ? ~0 : 0 */
    vs = _mm_and_si128(vs, vt); /* vs == vt == -32768:  corner case confirmed */

    negative = _mm_xor_si128(negative, vs);
    *(v16 *)VACC_H = negative; /* 2*i16*i16 only fills L/M; VACC_H = 0 or ~0. */
    return _mm_add_epi16(vs, prod_hi); /* prod_hi must be -32768; - 1 = +32767 */
#else
    word_64 product[N]; /* (-32768 * -32768)<<1 + 32768 confuses 32-bit type. */
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].W = vs[i] * vt[i];
    for (i = 0; i < N; i++)
        product[i].W <<= 1; /* special fractional shift value */
    for (i = 0; i < N; i++)
        product[i].W += 32768; /* special fractional round value */
    for (i = 0; i < N; i++)
        VACC_L[i] = (product[i].UW & 0x00000000FFFF) >>  0;
    for (i = 0; i < N; i++)
        VACC_M[i] = (product[i].UW & 0x0000FFFF0000) >> 16;
    for (i = 0; i < N; i++)
        VACC_H[i] = -(product[i].SW < 0); /* product>>32 & 0xFFFF */
    SIGNED_CLAMP_AM(V_result);
#endif
}

VECTOR_OPERATION VMULU(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 negative;
    v16 round;
    v16 prod_hi, prod_lo;

/*
 * Besides the unsigned clamping method (as opposed to VMULF's signed clamp),
 * this operation's multiplication matches VMULF.  See VMULF for annotations.
 */
    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epi16(vs, vt);

    prod_hi = _mm_add_epi16(prod_hi, prod_hi);
    negative = _mm_srli_epi16(prod_lo, 15);
    prod_hi = _mm_add_epi16(prod_hi, negative);
    prod_lo = _mm_add_epi16(prod_lo, prod_lo);
    negative = _mm_srli_epi16(prod_lo, 15);

    round = _mm_cmpeq_epi16(vs, vs);
    round = _mm_slli_epi16(round, 15);

    prod_lo = _mm_xor_si128(prod_lo, round);
    *(v16 *)VACC_L = prod_lo;
    prod_hi = _mm_add_epi16(prod_hi, negative);
    *(v16 *)VACC_M = prod_hi;

/*
 * VMULU does unsigned clamping.  However, in VMULU's case, the only possible
 * combinations that overflow, are either negative values or -32768 * -32768.
 */
    negative = _mm_srai_epi16(prod_hi, 15);
    vs = _mm_cmpeq_epi16(vs, round); /* vs == -32768 ? ~0 : 0 */
    vt = _mm_cmpeq_epi16(vt, round); /* vt == -32768 ? ~0 : 0 */
    vs = _mm_and_si128(vs, vt); /* vs == vt == -32768:  corner case confirmed */
    negative = _mm_xor_si128(negative, vs);
    *(v16 *)VACC_H = negative; /* 2*i16*i16 only fills L/M; VACC_H = 0 or ~0. */

    prod_lo = _mm_srai_epi16(prod_hi, 15); /* unsigned overflow mask */
    vs = _mm_or_si128(prod_hi, prod_lo);
    return _mm_andnot_si128(negative, vs); /* unsigned underflow mask */
#else
    word_64 product[N]; /* (-32768 * -32768)<<1 + 32768 confuses 32-bit type. */
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].W = vs[i] * vt[i];
    for (i = 0; i < N; i++)
        product[i].W <<= 1; /* special fractional shift value */
    for (i = 0; i < N; i++)
        product[i].W += 32768; /* special fractional round value */
    for (i = 0; i < N; i++)
        VACC_L[i] = (product[i].UW & 0x00000000FFFF) >>  0;
    for (i = 0; i < N; i++)
        VACC_M[i] = (product[i].UW & 0x0000FFFF0000) >> 16;
    for (i = 0; i < N; i++)
        VACC_H[i] = -(product[i].SW < 0); /* product>>32 & 0xFFFF */
    UNSIGNED_CLAMP(V_result);
#endif
}

VECTOR_OPERATION VMUDL(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    vs = _mm_mulhi_epu16(vs, vt);
    vector_wipe(vt); /* (UINT16_MAX * UINT16_MAX) >> 16 too small for MD/HI */
    *(v16 *)VACC_L = vs;
    *(v16 *)VACC_M = vt;
    *(v16 *)VACC_H = vt;
    return (vs); /* no possibilities to clamp */
#else
    word_32 product[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].UW = (u16)vs[i] * (u16)vt[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = product[i].UW >> 16; /* product[i].H[HES(0) >> 1] */
    vector_copy(V_result, VACC_L);
    vector_wipe(VACC_M);
    vector_wipe(VACC_H);
#endif
}

VECTOR_OPERATION VMUDM(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 prod_hi, prod_lo;

    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epu16(vs, vt);

/*
 * Based on a little pattern found by MarathonMan...
 * If (vs < 0), then high 16 bits of (u16)vs * (u16)vt += ~(vt) + 1, or -vt.
 */
    vs = _mm_srai_epi16(vs, 15);
    vt = _mm_and_si128(vt, vs);
    prod_hi = _mm_sub_epi16(prod_hi, vt);

    *(v16 *)VACC_L = prod_lo;
    *(v16 *)VACC_M = prod_hi;
    vs = prod_hi;
    prod_hi = _mm_srai_epi16(prod_hi, 15);
    *(v16 *)VACC_H = prod_hi;
    return (vs);
#else
    word_32 product[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (s16)vs[i] * (u16)vt[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = (product[i].W & 0x00000000FFFF) >>  0;
    for (i = 0; i < N; i++)
        VACC_M[i] = (product[i].W & 0x0000FFFF0000) >> 16;
    for (i = 0; i < N; i++)
        VACC_H[i] = -(VACC_M[i] < 0);
    vector_copy(V_result, VACC_M);
#endif
}

VECTOR_OPERATION VMUDN(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 prod_hi, prod_lo;

    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epu16(vs, vt);

/*
 * Based on the pattern discovered for the similar VMUDM operation.
 * If (vt < 0), then high 16 bits of (u16)vs * (u16)vt += ~(vs) + 1, or -vs.
 */
    vt = _mm_srai_epi16(vt, 15);
    vs = _mm_and_si128(vs, vt);
    prod_hi = _mm_sub_epi16(prod_hi, vs);

    *(v16 *)VACC_L = prod_lo;
    *(v16 *)VACC_M = prod_hi;
    prod_hi = _mm_srai_epi16(prod_hi, 15);
    *(v16 *)VACC_H = prod_hi;
    return (prod_lo);
#else
    word_32 product[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (u16)vs[i] * (s16)vt[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = (product[i].W & 0x00000000FFFF) >>  0;
    for (i = 0; i < N; i++)
        VACC_M[i] = (product[i].W & 0x0000FFFF0000) >> 16;
    for (i = 0; i < N; i++)
        VACC_H[i] = -(VACC_M[i] < 0);
    vector_copy(V_result, VACC_L);
#endif
}

VECTOR_OPERATION VMUDH(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 prod_high;

    prod_high = _mm_mulhi_epi16(vs, vt);
    vs        = _mm_mullo_epi16(vs, vt);

    *(v16 *)VACC_L = _mm_setzero_si128();
    *(v16 *)VACC_M = vs; /* acc 31..16 storing (VS*VT)15..0 */
    *(v16 *)VACC_H = prod_high; /* acc 47..32 storing (VS*VT)31..16 */

/*
 * "Unpack" the low 16 bits and the high 16 bits of each 32-bit product to a
 * couple xmm registers, re-storing them as 2 32-bit products each.
 */
    vt = _mm_unpackhi_epi16(vs, prod_high);
    vs = _mm_unpacklo_epi16(vs, prod_high);

/*
 * Re-interleave or pack both 32-bit products in both xmm registers with
 * signed saturation:  prod < -32768 to -32768 and prod > +32767 to +32767.
 */
    return _mm_packs_epi32(vs, vt);
#else
    word_32 product[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (s16)vs[i] * (s16)vt[i];
    vector_wipe(VACC_L);
    for (i = 0; i < N; i++)
        VACC_M[i] = (s16)(product[i].W >>  0); /* product[i].HW[HES(0) >> 1] */
    for (i = 0; i < N; i++)
        VACC_H[i] = (s16)(product[i].W >> 16); /* product[i].HW[HES(2) >> 1] */
    SIGNED_CLAMP_AM(V_result);
#endif
}

VECTOR_OPERATION VMACF(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_hi, acc_md, acc_lo;
    v16 prod_hi, prod_lo;
    v16 overflow, overflow_new;
    v16 prod_neg, carry;

    prod_hi = _mm_mulhi_epi16(vs, vt);
    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_neg = _mm_srli_epi16(prod_hi, 15);

    /* fractional adjustment by shifting left one bit */
    overflow = _mm_srli_epi16(prod_lo, 15); /* hi bit lost when s16 += s16 */
    prod_lo = _mm_add_epi16(prod_lo, prod_lo);
    prod_hi = _mm_add_epi16(prod_hi, prod_hi);
    prod_hi = _mm_or_si128(prod_hi, overflow); /* Carry lo's MSB to hi's LSB. */

    acc_lo = *(v16 *)VACC_L;
    acc_md = *(v16 *)VACC_M;
    acc_hi = *(v16 *)VACC_H;

    acc_lo = _mm_add_epi16(acc_lo, prod_lo);
    *(v16 *)VACC_L = acc_lo;
    overflow = _mm_cmplt_epu16(acc_lo, prod_lo); /* a + b < a + 0 ? ~0 : 0 */

    acc_md = _mm_add_epi16(acc_md, prod_hi);
    overflow_new = _mm_cmplt_epu16(acc_md, prod_hi);
    acc_md = _mm_sub_epi16(acc_md, overflow); /* m - (overflow = ~0) == m + 1 */
    carry = _mm_cmpeq_epi16(acc_md, _mm_setzero_si128());
    carry = _mm_and_si128(carry, overflow); /* ~0 - (-1) == 0 && (-1) != 0 */
    *(v16 *)VACC_M = acc_md;
    overflow = _mm_or_si128(carry, overflow_new);

    acc_hi = _mm_sub_epi16(acc_hi, overflow);
    acc_hi = _mm_sub_epi16(acc_hi, prod_neg);
    *(v16 *)VACC_H = acc_hi;

    vt = _mm_unpackhi_epi16(acc_md, acc_hi);
    vs = _mm_unpacklo_epi16(acc_md, acc_hi);
    return _mm_packs_epi32(vs, vt);
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = vs[i] * vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (product[i].SW << 1) & 0x00000000FFFF;
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)(VACC_L[i]) + addend[i].UW;
    for (i = 0; i < N; i++)
        VACC_L[i] = (i16)(addend[i].UW);
    for (i = 0; i < N; i++)
        addend[i].UW = (addend[i].UW >> 16) + (u16)(product[i].SW >> 15);
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)(VACC_M[i]) + addend[i].UW;
    for (i = 0; i < N; i++)
        VACC_M[i] = (i16)(addend[i].UW);
    for (i = 0; i < N; i++)
        VACC_H[i] -= (product[i].SW < 0);
    for (i = 0; i < N; i++)
        VACC_H[i] += addend[i].UW >> 16;
    SIGNED_CLAMP_AM(V_result);
#endif
}

VECTOR_OPERATION VMACU(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_hi, acc_md, acc_lo;
    v16 prod_hi, prod_lo;
    v16 overflow, overflow_new;
    v16 prod_neg, carry;

    prod_hi = _mm_mulhi_epi16(vs, vt);
    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_neg = _mm_srli_epi16(prod_hi, 15);

    /* fractional adjustment by shifting left one bit */
    overflow = _mm_srli_epi16(prod_lo, 15); /* hi bit lost when s16 += s16 */
    prod_lo = _mm_add_epi16(prod_lo, prod_lo);
    prod_hi = _mm_add_epi16(prod_hi, prod_hi);
    prod_hi = _mm_or_si128(prod_hi, overflow); /* Carry lo's MSB to hi's LSB. */

    acc_lo = *(v16 *)VACC_L;
    acc_md = *(v16 *)VACC_M;
    acc_hi = *(v16 *)VACC_H;

    acc_lo = _mm_add_epi16(acc_lo, prod_lo);
    *(v16 *)VACC_L = acc_lo;
    overflow = _mm_cmplt_epu16(acc_lo, prod_lo); /* a + b < a + 0 ? ~0 : 0 */

    acc_md = _mm_add_epi16(acc_md, prod_hi);
    overflow_new = _mm_cmplt_epu16(acc_md, prod_hi);
    acc_md = _mm_sub_epi16(acc_md, overflow); /* m - (overflow = ~0) == m + 1 */
    carry = _mm_cmpeq_epi16(acc_md, _mm_setzero_si128());
    carry = _mm_and_si128(carry, overflow); /* ~0 - (-1) == 0 && (-1) != 0 */
    *(v16 *)VACC_M = acc_md;
    overflow = _mm_or_si128(carry, overflow_new);

    acc_hi = _mm_sub_epi16(acc_hi, overflow);
    acc_hi = _mm_sub_epi16(acc_hi, prod_neg);
    *(v16 *)VACC_H = acc_hi;

    vt = _mm_unpackhi_epi16(acc_md, acc_hi);
    vs = _mm_unpacklo_epi16(acc_md, acc_hi);
    vs = _mm_packs_epi32(vs, vt);
    overflow = _mm_cmplt_epi16(acc_md, vs);
    vs = _mm_andnot_si128(_mm_srai_epi16(vs, 15), vs);
    return _mm_or_si128(vs, overflow);
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = vs[i] * vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (product[i].SW << 1) & 0x00000000FFFF;
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)(VACC_L[i]) + addend[i].UW;
    for (i = 0; i < N; i++)
        VACC_L[i] = (i16)(addend[i].UW);
    for (i = 0; i < N; i++)
        addend[i].UW = (addend[i].UW >> 16) + (u16)(product[i].SW >> 15);
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)(VACC_M[i]) + addend[i].UW;
    for (i = 0; i < N; i++)
        VACC_M[i] = (i16)(addend[i].UW);
    for (i = 0; i < N; i++)
        VACC_H[i] -= (product[i].SW < 0);
    for (i = 0; i < N; i++)
        VACC_H[i] += addend[i].UW >> 16;
    UNSIGNED_CLAMP(V_result);
#endif
}

VECTOR_OPERATION VMADL(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_hi, acc_md, acc_lo;
    v16 prod_hi;
    v16 overflow, overflow_new;

 /* prod_lo = _mm_mullo_epi16(vs, vt); */
    prod_hi = _mm_mulhi_epu16(vs, vt);

    acc_lo = *(v16 *)VACC_L;
    acc_md = *(v16 *)VACC_M;
    acc_hi = *(v16 *)VACC_H;

    acc_lo = _mm_add_epi16(acc_lo, prod_hi);
    *(v16 *)VACC_L = acc_lo;

    overflow = _mm_cmplt_epu16(acc_lo, prod_hi); /* overflow:  (x + y < y) */
    acc_md = _mm_sub_epi16(acc_md, overflow);
    *(v16 *)VACC_M = acc_md;

/*
 * Luckily for us, taking unsigned * unsigned always evaluates to something
 * nonnegative, so we only have to worry about overflow from accumulating.
 */
    overflow_new = _mm_cmpeq_epi16(acc_md, _mm_setzero_si128());
    overflow = _mm_and_si128(overflow, overflow_new);
    acc_hi = _mm_sub_epi16(acc_hi, overflow);
    *(v16 *)VACC_H = acc_hi;

/*
 * Do a signed clamp...sort of (VM?DM, VM?DH:  middle; VM?DL, VM?DN:  low).
 *     if (acc_47..16 < -32768) result = -32768 ^ 0x8000;      # 0000
 *     else if (acc_47..16 > +32767) result = +32767 ^ 0x8000; # FFFF
 *     else { result = acc_15..0 & 0xFFFF; }
 * So it is based on the standard signed clamping logic for VM?DM, VM?DH,
 * except that extra steps must be concatenated to that definition.
 */
    vt = _mm_unpackhi_epi16(acc_md, acc_hi);
    vs = _mm_unpacklo_epi16(acc_md, acc_hi);
    vs = _mm_packs_epi32(vs, vt);

    acc_md = _mm_cmpeq_epi16(acc_md, vs); /* (unclamped == clamped) ... */
    acc_lo = _mm_and_si128(acc_lo, acc_md); /* ... ? low : mid */
    vt = _mm_cmpeq_epi16(vt, vt);
    acc_md = _mm_xor_si128(acc_md, vt); /* (unclamped != clamped) ... */

    vs = _mm_and_si128(vs, acc_md); /* ... ? VS_clamped : 0x0000 */
    vs = _mm_or_si128(vs, acc_lo); /*                   : acc_lo */
    acc_md = _mm_slli_epi16(acc_md, 15); /* ... ? ^ 0x8000 : ^ 0x0000 */
    return _mm_xor_si128(vs, acc_md); /* stupid unsigned-clamp-ish adjustment */
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].UW = (u16)vs[i] * (u16)vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)(product[i].UW >> 16) + (u16)VACC_L[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        addend[i].UW = (addend[i].UW >> 16) + (0x000000000000 >> 16);
    for (i = 0; i < N; i++)
        addend[i].UW += (u16)VACC_M[i];
    for (i = 0; i < N; i++)
        VACC_M[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        VACC_H[i] += addend[i].UW >> 16;
    SIGNED_CLAMP_AL(V_result);
#endif
}

VECTOR_OPERATION VMADM(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_hi, acc_md, acc_lo;
    v16 prod_hi, prod_lo;
    v16 overflow;

    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epu16(vs, vt);

    vs = _mm_srai_epi16(vs, 15);
    vt = _mm_and_si128(vt, vs);
    prod_hi = _mm_sub_epi16(prod_hi, vt);

/*
 * Writeback phase to the accumulator.
 * VMADM stores accumulator += the product achieved by VMUDM.
 */
    acc_lo = *(v16 *)VACC_L;
    acc_md = *(v16 *)VACC_M;
    acc_hi = *(v16 *)VACC_H;

    acc_lo = _mm_add_epi16(acc_lo, prod_lo);
    *(v16 *)VACC_L = acc_lo;

    overflow = _mm_cmplt_epu16(acc_lo, prod_lo); /* overflow:  (x + y < y) */
    prod_hi = _mm_sub_epi16(prod_hi, overflow);
    acc_md = _mm_add_epi16(acc_md, prod_hi);
    *(v16 *)VACC_M = acc_md;

    overflow = _mm_cmplt_epu16(acc_md, prod_hi);
    prod_hi = _mm_srai_epi16(prod_hi, 15);
    acc_hi = _mm_add_epi16(acc_hi, prod_hi);
    acc_hi = _mm_sub_epi16(acc_hi, overflow);
    *(v16 *)VACC_H = acc_hi;

    vt = _mm_unpackhi_epi16(acc_md, acc_hi);
    vs = _mm_unpacklo_epi16(acc_md, acc_hi);
    return _mm_packs_epi32(vs, vt);
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (s16)vs[i] * (u16)vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (product[i].W & 0x0000FFFF) + (u16)VACC_L[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        addend[i].UW = (addend[i].UW >> 16) + (product[i].SW >> 16);
    for (i = 0; i < N; i++)
        addend[i].UW += (u16)VACC_M[i];
    for (i = 0; i < N; i++)
        VACC_M[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        VACC_H[i] += addend[i].UW >> 16;
    SIGNED_CLAMP_AM(V_result);
#endif
}

VECTOR_OPERATION VMADN(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_hi, acc_md, acc_lo;
    v16 prod_hi, prod_lo;
    v16 overflow;

    prod_lo = _mm_mullo_epi16(vs, vt);
    prod_hi = _mm_mulhi_epu16(vs, vt);

    vt = _mm_srai_epi16(vt, 15);
    vs = _mm_and_si128(vs, vt);
    prod_hi = _mm_sub_epi16(prod_hi, vs);

/*
 * Writeback phase to the accumulator.
 * VMADN stores accumulator += the product achieved by VMUDN.
 */
    acc_lo = *(v16 *)VACC_L;
    acc_md = *(v16 *)VACC_M;
    acc_hi = *(v16 *)VACC_H;

    acc_lo = _mm_add_epi16(acc_lo, prod_lo);
    *(v16 *)VACC_L = acc_lo;

    overflow = _mm_cmplt_epu16(acc_lo, prod_lo); /* overflow:  (x + y < y) */
    prod_hi = _mm_sub_epi16(prod_hi, overflow);
    acc_md = _mm_add_epi16(acc_md, prod_hi);
    *(v16 *)VACC_M = acc_md;

    overflow = _mm_cmplt_epu16(acc_md, prod_hi);
    prod_hi = _mm_srai_epi16(prod_hi, 15);
    acc_hi = _mm_add_epi16(acc_hi, prod_hi);
    acc_hi = _mm_sub_epi16(acc_hi, overflow);
    *(v16 *)VACC_H = acc_hi;

/*
 * Do a signed clamp...sort of (VM?DM, VM?DH:  middle; VM?DL, VM?DN:  low).
 *     if (acc_47..16 < -32768) result = -32768 ^ 0x8000;      # 0000
 *     else if (acc_47..16 > +32767) result = +32767 ^ 0x8000; # FFFF
 *     else { result = acc_15..0 & 0xFFFF; }
 * So it is based on the standard signed clamping logic for VM?DM, VM?DH,
 * except that extra steps must be concatenated to that definition.
 */
    vt = _mm_unpackhi_epi16(acc_md, acc_hi);
    vs = _mm_unpacklo_epi16(acc_md, acc_hi);
    vs = _mm_packs_epi32(vs, vt);

    acc_md = _mm_cmpeq_epi16(acc_md, vs); /* (unclamped == clamped) ... */
    acc_lo = _mm_and_si128(acc_lo, acc_md); /* ... ? low : mid */
    vt = _mm_cmpeq_epi16(vt, vt);
    acc_md = _mm_xor_si128(acc_md, vt); /* (unclamped != clamped) ... */

    vs = _mm_and_si128(vs, acc_md); /* ... ? VS_clamped : 0x0000 */
    vs = _mm_or_si128(vs, acc_lo); /*                   : acc_lo */
    acc_md = _mm_slli_epi16(acc_md, 15); /* ... ? ^ 0x8000 : ^ 0x0000 */
    return _mm_xor_si128(vs, acc_md); /* stupid unsigned-clamp-ish adjustment */
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (u16)vs[i] * (s16)vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (product[i].W & 0x0000FFFF) + (u16)VACC_L[i];
    for (i = 0; i < N; i++)
        VACC_L[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        addend[i].UW = (addend[i].UW >> 16) + (product[i].SW >> 16);
    for (i = 0; i < N; i++)
        addend[i].UW += (u16)VACC_M[i];
    for (i = 0; i < N; i++)
        VACC_M[i] = addend[i].UW & 0x0000FFFF;
    for (i = 0; i < N; i++)
        VACC_H[i] += addend[i].UW >> 16;
    SIGNED_CLAMP_AL(V_result);
#endif
}

VECTOR_OPERATION VMADH(v16 vs, v16 vt)
{
#ifdef ARCH_MIN_SSE2
    v16 acc_mid;
    v16 prod_high;

    prod_high = _mm_mulhi_epi16(vs, vt);
    vs        = _mm_mullo_epi16(vs, vt);

/*
 * We're required to load the source product from the accumulator to add to.
 * While we're at it, conveniently sneak in a acc[31..16] += (vs*vt)[15..0].
 */
    acc_mid = *(v16 *)VACC_M;
    vs = _mm_add_epi16(vs, acc_mid);
    *(v16 *)VACC_M = vs;
    vt = *(v16 *)VACC_H;

/*
 * While accumulating base_lo + product_lo is easy, getting the correct data
 * for base_hi + product_hi is tricky and needs unsigned overflow detection.
 *
 * The one-liner solution to detecting unsigned overflow (thus adding a carry
 * value of 1 to the higher word) is _mm_cmplt_epu16, but none of the Intel
 * MMX-based instruction sets define unsigned comparison ops FOR us, so...
 */
    vt = _mm_add_epi16(vt, prod_high);
    vs = _mm_cmplt_epu16(vs, acc_mid); /* acc.mid + prod.low < acc.mid */
    vt = _mm_sub_epi16(vt, vs); /* += 1 if overflow, by doing -= ~0 */
    *(v16 *)VACC_H = vt;

    vs = *(v16 *)VACC_M;
    prod_high = _mm_unpackhi_epi16(vs, vt);
    vs        = _mm_unpacklo_epi16(vs, vt);
    return _mm_packs_epi32(vs, prod_high);
#else
    word_32 product[N], addend[N];
    register unsigned int i;

    for (i = 0; i < N; i++)
        product[i].SW = (s16)vs[i] * (s16)vt[i];
    for (i = 0; i < N; i++)
        addend[i].UW = (u16)VACC_M[i] + (u16)(product[i].W);
    for (i = 0; i < N; i++)
        VACC_M[i] += (i16)product[i].SW;
    for (i = 0; i < N; i++)
        VACC_H[i] += (addend[i].UW >> 16) + (product[i].SW >> 16);
    SIGNED_CLAMP_AM(V_result);
#endif
}
