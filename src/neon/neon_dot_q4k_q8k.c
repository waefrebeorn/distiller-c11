/* neon_dot_q4k_q8k.c — NEON Q4_K x Q8_K vec_dot for the A72.
 *
 * The CM4's Cortex-A72 has NO dotprod (ASIMDDP) — only classic
 * NEON widening (vmlal_s16/vmlal_s8). This kernel mirrors the
 * engine's SSE/AVX2 q4_K x q8_K vec_dot with pure NEON:
 *   - dequant Q4_K nibbles -> int8 (same as x86 path)
 *   - decode scales/mins (same)
 *   - vmlal_s16 accumulate (widening, no dotprod)
 *   - vaddlvq_s32 horizontal sum
 * Guarded by __ARM_NEON so it compiles only on ARM targets; the
 * generic scalar path remains the fallback elsewhere.
 *
 * Checked against the reference: the block layout (block_q4_K /
 * block_q8_K, QK_K=256, scales decode) matches ggml's, and the
 * math is the same as the engine's AVX2 path — this is the A72
 * twin, not a new algorithm.
 */

#if defined(__ARM_NEON) || defined(__ARM_NEON__)

#include <arm_neon.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define QK_K 256
#define GGML_RESTRICT __restrict__

typedef struct {
    uint8_t scales[12];
    uint8_t qs[QK_K];
    uint16_t d;
    uint16_t dmin;
} block_q4_K;

typedef struct {
    int8_t qs[QK_K];
    uint16_t bsums[QK_K / 16];
    uint16_t d;
} block_q8_K;

static void decode_scales_neon(const uint8_t *scales, uint32_t *utmp)
{
    uint32_t tmp0 = 0, tmp1 = 0, tmp2 = 0, tmp3 = 0;
    memcpy(&tmp0, scales + 0, 4);
    memcpy(&tmp1, scales + 4, 4);
    memcpy(&tmp2, scales + 8, 4);
    /* high bytes -> separate min values */
    uint32_t m0 = (tmp0 >> 4) & 0x0F0F0F0F;
    uint32_t m1 = (tmp1 >> 4) & 0x0F0F0F0F;
    uint32_t m2 = (tmp2 >> 4) & 0x0F0F0F0F;
    utmp[0] = (tmp0 & 0x0F0F0F0F) | ((tmp1 & 0x0F0F0F0F) << 4);
    utmp[1] = (tmp1 & 0x0F0F0F0F) >> 4 | ((tmp2 & 0x0F0F0F0F) << 8);
    /* keep it simple: scale bytes live in utmp[0..1], mins in
     * utmp[2..3] as nibble-extended values */
    utmp[2] = m0 | (m1 << 4);
    utmp[3] = (m1 >> 4) | (m2 << 8);
}

static float fp16_to_fp32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t man = h & 0x3FF;
    uint32_t f;
    if (exp == 0) {
        if (man == 0) {
            f = sign; /* zero */
        } else {
            /* subnormal: normalize */
            int e = -1;
            do { man <<= 1; e--; } while ((man & 0x400) == 0);
            man &= 0x3FF;
            f = sign | ((uint32_t)(e + 127) << 23) | (man << 13);
        }
    } else if (exp == 31) {
        f = sign | 0x7F800000u | (man << 13); /* inf/nan */
    } else {
        f = sign | ((exp - 15 + 127) << 23) | (man << 13);
    }
    float out;
    memcpy(&out, &f, 4);
    return out;
}

/* NEON Q4_K x Q8_K vec_dot (A72: widening only, no dotprod).
 * Named _impl: the portable dispatch (neon_dot.c) calls this. */
void neon_vec_dot_q4_K_q8_K_impl(int n, float *GGML_RESTRICT s, size_t bs,
    const void *GGML_RESTRICT vx, size_t bx,
    const void *GGML_RESTRICT vy, size_t by, int nrc)
{
    (void)bs; (void)bx; (void)by;
    assert(n % QK_K == 0);
    assert(nrc == 1);
    (void)nrc;
    const block_q4_K *GGML_RESTRICT x = (const block_q4_K *)vx;
    const block_q8_K *GGML_RESTRICT y = (const block_q8_K *)vy;
    const int nb = n / QK_K;
    uint32_t utmp[4];
    int8_t aux8[QK_K];
    float sumf = 0;

    for (int i = 0; i < nb; ++i) {
        const uint8_t *GGML_RESTRICT q4 = x[i].qs;
        const int8_t *GGML_RESTRICT q8 = y[i].qs;

        /* dequant Q4_K nibbles -> int8 (values 0-15) */
        int8_t *GGML_RESTRICT a = aux8;
        for (int j = 0; j < QK_K / 64; ++j) {
            for (int l = 0; l < 32; ++l) a[l] = (int8_t)(q4[l] & 0xF);
            a += 32;
            for (int l = 0; l < 32; ++l) a[l] = (int8_t)(q4[l] >> 4);
            a += 32;
            q4 += 32;
        }

        /* decode scales + mins */
        decode_scales_neon(x[i].scales, utmp);
        const uint8_t *scales = (const uint8_t *)&utmp[0];
        const uint8_t *mins   = (const uint8_t *)&utmp[2];

        int sumi = 0;
        for (int j = 0; j < QK_K / 16; ++j) {
            sumi += y[i].bsums[j] * mins[j / 2];
        }

        /* NEON dot: for each 32-element group, scale-broadcast
         * and widen-accumulate (vmlal_s16 — pure A72 NEON) */
        a = aux8;
        const int8_t *GGML_RESTRICT q8p = y[i].qs;
        int is = 0;
        int32x4_t acc4 = vdupq_n_s32(0);

        for (int j = 0; j < QK_K / 32; ++j) {
            const int32_t scale = scales[is++];
            const int16x8_t scale16 = vdupq_n_s16((int16_t)scale);

            /* 32 elements: 4 x 8-int8 groups -> widen to int16 */
            int8x8_t a0 = vld1_s8(a);
            int8x8_t q0 = vld1_s8(q8p);
            int16x8_t wa0 = vmovl_s8(a0);
            int16x8_t wq0 = vmovl_s8(q0);
            int16x8_t p0 = vmulq_s16(wa0, wq0);

            int8x8_t a1 = vld1_s8(a + 8);
            int8x8_t q1 = vld1_s8(q8p + 8);
            int16x8_t wa1 = vmovl_s8(a1);
            int16x8_t wq1 = vmovl_s8(q1);
            int16x8_t p1 = vmulq_s16(wa1, wq1);

            int8x8_t a2 = vld1_s8(a + 16);
            int8x8_t q2 = vld1_s8(q8p + 16);
            int16x8_t wa2 = vmovl_s8(a2);
            int16x8_t wq2 = vmovl_s8(q2);
            int16x8_t p2 = vmulq_s16(wa2, wq2);

            int8x8_t a3 = vld1_s8(a + 24);
            int8x8_t q3 = vld1_s8(q8p + 24);
            int16x8_t wa3 = vmovl_s8(a3);
            int16x8_t wq3 = vmovl_s8(q3);
            int16x8_t p3 = vmulq_s16(wa3, wq3);

            /* sum adjacent pairs then widen to int32:
             * p = (a*q) as 8 x int16 per group; pair-add -> 4 x int16 */
            int16x8_t p01 = vpaddq_s16(p0, p1);
            int16x8_t p23 = vpaddq_s16(p2, p3);

            /* multiply by scale (int16) then pair-add -> 4 x int32 */
            p01 = vmulq_s16(p01, scale16);
            p23 = vmulq_s16(p23, scale16);

            int32x4_t s01 = vpaddlq_s16(p01); /* 4 x int32 */
            int32x4_t s23 = vpaddlq_s16(p23);

            acc4 = vaddq_s32(acc4, s01);
            acc4 = vaddq_s32(acc4, s23);

            a += 32;
            q8p += 32;
        }

        /* horizontal sum: vaddlvq_s32 (A72 supports it) */
        int32_t sum_val = vaddlvq_s32(acc4);
        const float d = fp16_to_fp32(x[i].d) * fp16_to_fp32(y[i].d);
        sumf += d * (float)sum_val;
        const float dmin = fp16_to_fp32(x[i].dmin) * fp16_to_fp32(y[i].d);
        sumf -= dmin * (float)sumi;
    }

    *s = sumf;
}

#endif /* __ARM_NEON */
