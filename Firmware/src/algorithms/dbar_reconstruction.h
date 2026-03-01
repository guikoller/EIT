/**
 * D-bar Reconstruction Algorithm for EIT
 *
 * Implements the D-bar (∂̄) method for 2D Electrical Impedance Tomography.
 *
 * Theory overview:
 *   The D-bar method is a direct (non-iterative) reconstruction algorithm
 *   based on the mathematical framework of inverse scattering.  Given
 *   boundary voltage data the algorithm:
 *
 *   1. Computes the Dirichlet-to-Neumann (DN) map difference
 *      ΔΛ = Λ_target − Λ_reference  from measurement differences Δv.
 *
 *   2. Approximates the complex-valued scattering transform t(k) on a
 *      truncated spectral disk |k| ≤ R  using the Born approximation:
 *        t(k) ≈ ∫ e^{i(k+k̄)·z} ΔΛ(z) dz
 *      which, with the sensitivity matrix S already available from the
 *      LBP calibration step, reduces to a matrix–vector product followed
 *      by a 2-D DFT-like summation over the pixel grid.
 *
 *   3. Solves the D-bar integral equation on the k-grid via fixed-point
 *      iteration (typically 3–5 iterations are sufficient at low R):
 *        μ(z,k) = 1 + (1/4π²) ∫ t(k') / (k̄ - k̄') · μ(z,k') / (k'-k) dk'
 *
 *   4. Recovers the conductivity change  δσ(z) ≈ Re[ μ(z,0) ] − 1
 *      and maps it to the image grid.
 *
 * Implementation notes:
 *   - All buffers are statically allocated (zero per-frame heap usage).
 *   - The k-grid is DBAR_K_GRID_SIZE × DBAR_K_GRID_SIZE complex points.
 *   - The scattering radius R (DBAR_K_RADIUS) acts as a regularisation
 *     parameter: smaller R → smoother images, larger R → sharper but noisier.
 *   - Reuses the sensitivity matrix loaded by lbp_init() (shared SDRAM).
 *   - Produces the same ReconstructionResult* structure as LBP so that
 *     the reconstruction viewer can switch algorithms transparently.
 *
 *   - calibration service (writer) / sensitivity_matrix_format.h
 */
#ifndef DBAR_RECONSTRUCTION_H
#define DBAR_RECONSTRUCTION_H

#include <stdint.h>

#include "eit_config.h"
#include "algorithms/lbp_reconstruction.h"   /* ReconstructionResult, SensitivityMatrixHeader */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Tuneable D-bar parameters                                          */
/* ------------------------------------------------------------------ */

/** Side-length of the square k-plane grid.  Total points = N².
 *  16×16 = 256 is a good balance for Cortex-M7 at 200 MHz.         */
#ifndef DBAR_K_GRID_SIZE
#define DBAR_K_GRID_SIZE   16
#endif

/** Scattering-transform truncation radius.
 *  Larger → sharper (but noisier) images.  Typical range: 2–8.      */
#ifndef DBAR_K_RADIUS
#define DBAR_K_RADIUS      4.0f
#endif

/** Number of fixed-point iterations for the D-bar equation.
 *  3 is usually enough at moderate R.                                */
#ifndef DBAR_ITERATIONS
#define DBAR_ITERATIONS    3
#endif

/* ------------------------------------------------------------------ */
/*  Public API (mirrors LBP)                                           */
/* ------------------------------------------------------------------ */

/** Initialise the D-bar module.
 *  Requires that lbp_init() has already been called so that the
 *  sensitivity matrix is available in SDRAM.
 *  @return 1 on success, 0 on failure. */
int dbar_init(void);

/** Reconstruct a conductivity-change image using the D-bar method.
 *  @param ref_uel     Reference voltage vector  [n_meas × n_inj]
 *  @param target_uel  Target voltage vector     [n_meas × n_inj]
 *  @param n_meas      Number of measurement electrodes per injection
 *  @param n_inj       Number of injection patterns
 *  @return Pointer to internal static result (valid until next call),
 *          or NULL if the module has not been initialised.           */
ReconstructionResult *dbar_reconstruct(const float *ref_uel,
                                       const float *target_uel,
                                       uint16_t n_meas,
                                       uint16_t n_inj);

#ifdef __cplusplus
}
#endif

#endif /* DBAR_RECONSTRUCTION_H */
