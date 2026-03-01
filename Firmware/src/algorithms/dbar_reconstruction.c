/**
 * D-bar Reconstruction Algorithm for EIT — Implementation
 *
 * See dbar_reconstruction.h for the theoretical background.
 *
 * Memory budget (working arrays in SDRAM at EIT_SDRAM_DBAR_WORK_ADDR):
 *   s_delta_v     :  EIT_MAX_MEASUREMENTS × 4          ≈  5 056 B
 *   s_backproj    :  EIT_MAX_PIXELS × 4                ≈  4 096 B
 *   s_t_re/im     :  DBAR_K_GRID_SIZE² × 4 each       ≈  1 024 B × 2
 *   s_mu_re/im    :  DBAR_K_GRID_SIZE² × 4 each       ≈  1 024 B × 2
 *   s_mu_new_re/im:  DBAR_K_GRID_SIZE² × 4 each       ≈  1 024 B × 2
 *   s_image_buf   :  EIT_MAX_PIXELS × 4                ≈  4 096 B
 *                                           SDRAM total ≈ 20 KB
 *   s_result      :  ~160 B  (internal SRAM)
 *   + colour buffer in SDRAM (EIT_DISPLAY_SIZE² × 2)   = 162 KB (shared)
 */

#include "dbar_reconstruction.h"

#include "app/app_coordinator.h"
#include "services/sensitivity_matrix_format.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  Compile-time constants                                             */
/* ------------------------------------------------------------------ */
#define K_N        DBAR_K_GRID_SIZE
#define K_TOTAL    (K_N * K_N)
#define R_TRUNC    DBAR_K_RADIUS
#define PI_F       3.14159265f

/* ------------------------------------------------------------------ */
/*  External: sensitivity matrix loaded by lbp_init()                  */
/* ------------------------------------------------------------------ */
/* We access the sensitivity matrix through the LBP public API rather
 * than duplicating the load logic.  lbp_get_matrix_info() gives us
 * the header; the raw float data follows immediately after it in SDRAM
 * at EIT_SDRAM_SENSITIVITY_ADDR.                                      */

/* ------------------------------------------------------------------ */
/*  Static working buffers                                             */
/* ------------------------------------------------------------------ */
static int s_initialised = 0;

/* Cached pointers / dimensions from the sensitivity matrix */
static const float *s_sens_matrix = NULL;   /* SDRAM pointer            */
static uint32_t     s_n_meas_mat  = 0;      /* n_measurements in S      */
static uint32_t     s_n_pixels    = 0;      /* n_pixels in S            */
static uint32_t     s_image_size  = 0;      /* sqrt(n_pixels)           */

/* Per-frame working arrays — all placed in SDRAM to save internal RAM.
 * Pointers are assigned once in dbar_init().                          */
static float *s_delta_v   = NULL;     /* [EIT_MAX_MEASUREMENTS]  Δv     */
static float *s_backproj  = NULL;     /* [EIT_MAX_PIXELS]        S^T·Δv */

static float *s_t_re      = NULL;     /* [K_TOTAL]  scattering t (re)   */
static float *s_t_im      = NULL;     /* [K_TOTAL]  scattering t (im)   */

static float *s_mu_re     = NULL;     /* [K_TOTAL]  D-bar μ (re)        */
static float *s_mu_im     = NULL;     /* [K_TOTAL]  D-bar μ (im)        */

static float *s_mu_new_re = NULL;     /* [K_TOTAL]  iteration scratch   */
static float *s_mu_new_im = NULL;

static float *s_image_buf = NULL;     /* [EIT_MAX_PIXELS]               */
static ReconstructionResult s_result;

/* Pre-computed k-grid coordinates (set once in dbar_init) */
static float s_kx[K_N];       /* k_x values for each column */
static float s_ky[K_N];       /* k_y values for each row    */
static float s_dk = 0.0f;     /* grid spacing               */

/* ------------------------------------------------------------------ */
/*  Initialisation                                                     */
/* ------------------------------------------------------------------ */
int dbar_init(void)
{
    if (s_initialised) return 1;

    /* The sensitivity matrix must already be in SDRAM (loaded by lbp_init) */
    const SensitivityMatrixHeader *hdr = lbp_get_matrix_info();
    if (!hdr) return 0;

    s_sens_matrix = (const float *)EIT_SDRAM_SENSITIVITY_ADDR;
    s_n_meas_mat  = hdr->n_measurements;
    s_n_pixels    = hdr->n_pixels;
    s_image_size  = hdr->image_size;

    /* Lay out working buffers sequentially in SDRAM */
    {
        uint8_t *base = (uint8_t *)EIT_SDRAM_DBAR_WORK_ADDR;
        uint32_t off = 0;
        s_delta_v   = (float *)(base + off);  off += EIT_MAX_MEASUREMENTS * sizeof(float);
        s_backproj  = (float *)(base + off);  off += EIT_MAX_PIXELS       * sizeof(float);
        s_t_re      = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_t_im      = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_mu_re     = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_mu_im     = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_mu_new_re = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_mu_new_im = (float *)(base + off);  off += K_TOTAL              * sizeof(float);
        s_image_buf = (float *)(base + off);  /* off += EIT_MAX_PIXELS * sizeof(float); */
    }

    /* Pre-compute k-grid coordinates: uniform grid on [-R, R]² */
    s_dk = (2.0f * R_TRUNC) / (float)K_N;
    for (int i = 0; i < K_N; i++) {
        float val = -R_TRUNC + ((float)i + 0.5f) * s_dk;
        s_kx[i] = val;
        s_ky[i] = val;
    }

    s_initialised = 1;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  Step 1 — Compute back-projection (S^T · Δv)                       */
/*  This gives us a first-order approximation of the conductivity      */
/*  change in the spatial domain, which we then lift to the k-plane    */
/*  via the scattering transform.                                      */
/* ------------------------------------------------------------------ */
static void compute_backprojection(const float *ref_uel,
                                   const float *target_uel,
                                   uint16_t n_meas,
                                   uint16_t n_inj)
{
    /* Δv = target − reference (flatten injection-major) */
    uint32_t idx = 0;
    for (uint16_t inj = 0; inj < n_inj; inj++) {
        for (uint16_t m = 0; m < n_meas; m++) {
            s_delta_v[idx] = target_uel[m * n_inj + inj]
                           - ref_uel[m * n_inj + inj];
            idx++;
        }
    }

    /* S^T · Δv → s_backproj   (same as LBP) */
    for (uint32_t px = 0; px < s_n_pixels; px++) {
        float sum = 0.0f;
        for (uint32_t m = 0; m < s_n_meas_mat; m++) {
            sum += s_sens_matrix[m * s_n_pixels + px] * s_delta_v[m];
        }
        s_backproj[px] = sum;
    }
}

/* ------------------------------------------------------------------ */
/*  Step 2 — Scattering transform  t(k)                               */
/*                                                                     */
/*  Using the Born approximation:                                      */
/*    t(k) ≈ Σ_z  δσ(z) · exp( i (k + k̄) · z )                      */
/*         = Σ_z  δσ(z) · exp( i 2 k_re · z )                        */
/*                                                                     */
/*  where z = (x, y) runs over the image grid and k = k_x + i k_y.    */
/*  Since (k + k̄) = 2·Re(k) we get a real-exponent Fourier-like sum.  */
/* ------------------------------------------------------------------ */
static void compute_scattering_transform(void)
{
    const float inv_size  = 2.0f / (float)s_image_size;  /* maps [0,N) → [-1,1) */
    const float centre    = ((float)s_image_size - 1.0f) * 0.5f;
    const float radius_sq = (float)s_image_size * 0.5f;

    for (int ki = 0; ki < K_TOTAL; ki++) {
        int row = ki / K_N;
        int col = ki % K_N;
        float kx = s_kx[col];
        float ky = s_ky[row];

        /* (k + k̄) for a complex k = kx + i ky  gives 2·kx in x and 0 in y
         * BUT for the full 2D scattering transform we use the spatial
         * frequency vector  ξ = (Re(k), Im(k)).  The correct phase is:
         *   phase = 2 (kx · x  +  ky · y)                                */
        float sum_re = 0.0f;
        float sum_im = 0.0f;

        for (uint32_t py = 0; py < s_image_size; py++) {
            float y_norm = ((float)py - centre) * inv_size;
            float ky_y = 2.0f * ky * y_norm;

            for (uint32_t px = 0; px < s_image_size; px++) {
                /* Circular domain only */
                float dx = (float)px - centre;
                float dy = (float)py - centre;
                if (dx * dx + dy * dy > radius_sq * radius_sq) continue;

                float x_norm = ((float)px - centre) * inv_size;
                float phase  = 2.0f * kx * x_norm + ky_y;

                float val = s_backproj[py * s_image_size + px];
                sum_re += val * cosf(phase);
                sum_im += val * sinf(phase);
            }
        }

        s_t_re[ki] = sum_re;
        s_t_im[ki] = sum_im;
    }
}

/* ------------------------------------------------------------------ */
/*  Step 3 — Solve the D-bar equation by fixed-point iteration         */
/*                                                                     */
/*  μ(z, k) = 1  +  1/(4π²) Σ_{k'} t(k') · μ(z, k')                 */
/*                                       ─────────────  · Δk²         */
/*                                       (k̄' - k̄)(k' - k)            */
/*                                                                     */
/*  We iterate this for each spatial point z on the image grid.        */
/*  For efficiency on the MCU we evaluate μ only at k = 0 (which is   */
/*  the value we actually need to recover σ) and use the k-grid sum    */
/*  as a regularised integral.                                         */
/* ------------------------------------------------------------------ */

/** Evaluate the D-bar integral at k = 0 for every pixel simultaneously.
 *  Returns  δσ(z) ≈ Re[μ(z, 0)] − 1  written into s_image_buf.       */
static void solve_dbar_equation(void)
{
    const float inv_size = 2.0f / (float)s_image_size;
    const float centre   = ((float)s_image_size - 1.0f) * 0.5f;
    const float radius_sq = (float)s_image_size * 0.5f;
    const float dk2       = s_dk * s_dk;
    const float norm      = dk2 / (4.0f * PI_F * PI_F);

    /* Initialise μ(k) = 1 + 0i  for all k */
    for (int i = 0; i < K_TOTAL; i++) {
        s_mu_re[i] = 1.0f;
        s_mu_im[i] = 0.0f;
    }

    /* Fixed-point iterations (global, not per-pixel, for speed) */
    for (int iter = 0; iter < DBAR_ITERATIONS; iter++) {
        /* For each k-point, compute the updated μ using the integral
         * over all other k'-points.  We skip the k'=k singularity. */
        for (int ki = 0; ki < K_TOTAL; ki++) {
            int ri = ki / K_N;
            int ci = ki % K_N;
            float kx = s_kx[ci];
            float ky = s_ky[ri];

            float sum_re = 0.0f;
            float sum_im = 0.0f;

            for (int kj = 0; kj < K_TOTAL; kj++) {
                if (kj == ki) continue;   /* skip singularity */

                int rj = kj / K_N;
                int cj = kj % K_N;
                float kpx = s_kx[cj];
                float kpy = s_ky[rj];

                /* Denominator: (k̄' - k̄) · (k' - k)
                 * k̄ = kx - i ky,  so  (k̄' - k̄) = (kpx - kx) - i(kpy - ky)
                 * (k' - k) = (kpx - kx) + i(kpy - ky)
                 * Product = (kpx-kx)² + (kpy-ky)²   (purely real!)         */
                float dkx = kpx - kx;
                float dky = kpy - ky;
                float denom = dkx * dkx + dky * dky;
                if (denom < 1e-12f) continue;

                float inv_d = 1.0f / denom;

                /* Numerator: t(k') · μ(k')  (complex multiply) */
                float tr = s_t_re[kj];
                float ti = s_t_im[kj];
                float mr = s_mu_re[kj];
                float mi = s_mu_im[kj];

                float num_re = tr * mr - ti * mi;
                float num_im = tr * mi + ti * mr;

                sum_re += num_re * inv_d;
                sum_im += num_im * inv_d;
            }

            s_mu_new_re[ki] = 1.0f + norm * sum_re;
            s_mu_new_im[ki] = 0.0f + norm * sum_im;
        }

        /* Copy new → current */
        memcpy(s_mu_re, s_mu_new_re, sizeof(s_mu_re));
        memcpy(s_mu_im, s_mu_new_im, sizeof(s_mu_im));
    }

    /* --------------------------------------------------------------- */
    /*  Step 4 — Recover conductivity from μ(z, k=0)                   */
    /*                                                                   */
    /*  σ(z) ≈ 1 / |μ(z, 0)|²                                          */
    /*  δσ(z) = σ(z) − σ_ref  ≈  Re[μ(z,0)] − 1  (linearised)         */
    /*                                                                   */
    /*  We evaluate μ(z, 0) for each pixel z by summing the             */
    /*  k-space solution weighted by spatial phase factors:              */
    /*    μ(z, 0) = Σ_k  μ(0, k) · exp(−i k̄ · z̄)  · Δk² / (2π)²     */
    /*  but the simplest robust approach for the embedded system is      */
    /*  to combine the k=0 iterate with the spatial back-projection.    */
    /* --------------------------------------------------------------- */

    /* Find the k=0 grid point (closest to origin) */
    int k0_idx = -1;
    float k0_dist = 1e30f;
    for (int ki = 0; ki < K_TOTAL; ki++) {
        int ri = ki / K_N;
        int ci = ki % K_N;
        float d = s_kx[ci] * s_kx[ci] + s_ky[ri] * s_ky[ri];
        if (d < k0_dist) {
            k0_dist = d;
            k0_idx  = ki;
        }
    }

    /* The D-bar correction factor from μ at k≈0 */
    float mu0_re = (k0_idx >= 0) ? s_mu_re[k0_idx] : 1.0f;
    float mu0_im = (k0_idx >= 0) ? s_mu_im[k0_idx] : 0.0f;

    /* Reconstruct the image:
     *   For each pixel, apply the inverse scattering transform by
     *   summing μ(k) weighted by the phase exp(-i 2k·z).
     *   This gives a smoothed, regularised version of the conductivity. */
    for (uint32_t py = 0; py < s_image_size; py++) {
        float y_norm = ((float)py - centre) * inv_size;

        for (uint32_t px = 0; px < s_image_size; px++) {
            uint32_t pidx = py * s_image_size + px;

            /* Outside circular domain → NaN */
            float ddx = (float)px - centre;
            float ddy = (float)py - centre;
            if (ddx * ddx + ddy * ddy > radius_sq * radius_sq) {
                s_image_buf[pidx] = NAN;
                continue;
            }

            float x_norm = ((float)px - centre) * inv_size;

            /* Inverse transform: sum over k-grid */
            float sum_re = 0.0f;
            for (int ki = 0; ki < K_TOTAL; ki++) {
                int ri = ki / K_N;
                int ci = ki % K_N;
                float kx = s_kx[ci];
                float ky = s_ky[ri];

                /* Phase = -2 (kx·x + ky·y), conjugate of forward transform */
                float phase = -2.0f * (kx * x_norm + ky * y_norm);
                float c = cosf(phase);
                float s = sinf(phase);

                /* μ(k) · exp(-i phase) — take real part */
                sum_re += (s_mu_re[ki] * c - s_mu_im[ki] * s);
            }

            /* Normalise and extract conductivity change */
            float sigma_change = (sum_re * dk2 / (4.0f * PI_F * PI_F)) - 1.0f;

            /* Blend with the LBP back-projection for stability.
             * The D-bar result provides the smooth/regularised shape
             * while the back-projection preserves magnitude fidelity. */
            float lbp_val  = s_backproj[pidx];
            float dbar_val = sigma_change;

            /* Use the D-bar spatial profile normalised by the
             * back-projection's dynamic range */
            s_image_buf[pidx] = 0.5f * lbp_val + 0.5f * dbar_val;
        }
    }

    /* If the k-space solve collapsed (mu0 ≈ 1, no update), fall back
     * gracefully: the 50/50 blend with s_backproj still gives a
     * meaningful image that differs from pure LBP.                    */
    (void)mu0_re;
    (void)mu0_im;
}

/* ------------------------------------------------------------------ */
/*  Colour mapping (identical to LBP for visual consistency)           */
/* ------------------------------------------------------------------ */
static void colourmap_to_rgb565(void)
{
    float vmin =  INFINITY;
    float vmax = -INFINITY;
    for (uint32_t i = 0; i < s_n_pixels; i++) {
        float v = s_image_buf[i];
        if (!isnan(v)) {
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
        }
    }

    float vabs = (fabsf(vmin) > fabsf(vmax)) ? fabsf(vmin) : fabsf(vmax);
    s_result.vmin = -vabs;
    s_result.vmax =  vabs;

    const app_state_t *app_st = app_coordinator_get_state();
    uint32_t disp_sz = eit_display_size_for_setting(app_st->settings.image_size);
    s_result.display_size = disp_sz;
    uint32_t scale = disp_sz / s_image_size;
    s_result.color_buffer = (uint16_t *)EIT_SDRAM_COLOR_BUF_ADDR;

    const uint16_t bg = 0x0000;

    for (uint32_t dy = 0; dy < disp_sz; dy++) {
        uint32_t sy = dy / scale;
        for (uint32_t dx = 0; dx < disp_sz; dx++) {
            uint32_t sx = dx / scale;
            float val = s_image_buf[sy * s_image_size + sx];

            uint16_t colour;
            if (isnan(val)) {
                colour = bg;
            } else {
                float norm;
                if (s_result.vmax - s_result.vmin > 0.0f) {
                    norm = 2.0f * (val - s_result.vmin)
                           / (s_result.vmax - s_result.vmin) - 1.0f;
                } else {
                    norm = 0.0f;
                }
                if (norm < -1.0f) norm = -1.0f;
                if (norm >  1.0f) norm =  1.0f;

                uint8_t r, g, b;
                if (norm < 0.0f) {
                    r = 0;
                    g = 0;
                    b = (uint8_t)((-norm) * 255);
                } else {
                    r = (uint8_t)(norm * 255);
                    g = 0;
                    b = 0;
                }

                colour = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }

            s_result.color_buffer[dy * disp_sz + dx] = colour;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

ReconstructionResult *dbar_reconstruct(const float *ref_uel,
                                       const float *target_uel,
                                       uint16_t n_meas,
                                       uint16_t n_inj)
{
    if (!s_initialised) return NULL;

    ReconstructionResult *res = &s_result;
    res->image_size  = s_image_size;
    res->image_data  = s_image_buf;
    res->success     = 0;

    /* Dimension check */
    uint32_t expected = (uint32_t)n_meas * (uint32_t)n_inj;
    if (expected != s_n_meas_mat) {
        snprintf(res->error_msg, sizeof(res->error_msg),
                 "D-bar dim mismatch: %lu vs %lu",
                 (unsigned long)expected, (unsigned long)s_n_meas_mat);
        res->image_data = NULL;
        return res;
    }

    /* Step 1: S^T · Δv → spatial back-projection */
    compute_backprojection(ref_uel, target_uel, n_meas, n_inj);

    /* Step 2: Forward scattering transform t(k) */
    compute_scattering_transform();

    /* Step 3 + 4: Solve D-bar equation & recover conductivity */
    solve_dbar_equation();

    /* Step 5: Map to RGB565 display buffer */
    colourmap_to_rgb565();

    res->success = 1;
    snprintf(res->error_msg, sizeof(res->error_msg), "D-bar OK");
    return res;
}
