#ifndef NEON_DOT_H
#define NEON_DOT_H

/* neon_dot.h — A72 NEON quantized dot kernels (C11).
 *
 * The CM4's Cortex-A72 has NO dotprod instructions; these kernels
 * use classic NEON widening (vmovl + vmul + vpaddl), mirroring
 * the x86 AVX2/SSE paths of the wubuwizard engine so the same
 * Q4_K x Q8_K blocks run on the device. On non-ARM targets the
 * symbols are absent (guarded), so callers must check.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 1 when the NEON kernel is compiled in (ARM target). */
int neon_dot_available(void);

/* Q4_K x Q8_K vec_dot (same signature shape as ggml vec_dot).
 * n must be a multiple of 256. Writes the dot product to *s.
 * Returns 0 on success, -1 if not compiled in / bad args. */
int neon_vec_dot_q4_K_q8_K(int n, float *s,
    const void *vx, const void *vy);

#ifdef __cplusplus
}
#endif

#endif /* NEON_DOT_H */
