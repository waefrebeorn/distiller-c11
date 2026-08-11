/* neon_dot.c — A72 NEON quantized dot dispatch (C11).
 *
 * The kernel itself lives in neon_dot_q4k_q8k.c (ARM-gated). This
 * file provides the portable dispatch: on ARM targets it calls the
 * NEON kernel; elsewhere it returns -1 (not available). */
#include "neon_dot.h"

#include <stddef.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
extern int neon_vec_dot_q4_K_q8_K_impl(int n, float *s,
    const void *vx, const void *vy);
#endif

int neon_dot_available(void)
{
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return 1;
#else
    return 0;
#endif
}

int neon_vec_dot_q4_K_q8_K(int n, float *s, const void *vx, const void *vy)
{
    if (n <= 0 || n % 256 != 0 || s == NULL || vx == NULL || vy == NULL) {
        return -1;
    }
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    return neon_vec_dot_q4_K_q8_K_impl(n, s, vx, vy);
#else
    (void)n; (void)s; (void)vx; (void)vy;
    return -1;
#endif
}
