/**
 * EIT Simulated Acquisition Backend
 *
 * Replays pre-recorded data from SD card files while simulating
 * the timing of a real DAQ unit (per-injection inject + measure phases).
 *
 * When the real hardware backend is ready, swap this out in the
 * presenter with zero changes to the rest of the code.
 */
#ifndef EIT_ACQ_SIMULATED_H
#define EIT_ACQ_SIMULATED_H

#include "eit_acquisition.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a simulated backend that replays data from SD card .bin files.
 *
 * @param ref_filename    Reference measurement file (e.g. "datamat_1_0.bin")
 * @param target_filename Target measurement file
 * @return Backend pointer, or NULL on failure.
 *         Must be freed with eit_acq_simulated_destroy().
 */
eit_acq_backend_t *eit_acq_simulated_create(const char *ref_filename,
                                             const char *target_filename);

void eit_acq_simulated_destroy(eit_acq_backend_t *backend);

/**
 * Configure simulated per-injection timing.
 *
 * Total frame time = n_inj × (inject_ms + measure_ms).
 * Set both to 0 for maximum throughput (instant frames).
 * Default: inject_ms = 1, measure_ms = 1 → ~160 ms / frame at 79 inj.
 */
void eit_acq_simulated_set_timing(eit_acq_backend_t *backend,
                                   uint32_t inject_ms,
                                   uint32_t measure_ms);

/**
 * Configure noise injection on simulated target data.
 * Noise amplitude = signal_RMS × level_pct / 100.
 * Different random values each frame (xorshift32 PRNG).
 *
 * @param enabled   1 = add noise, 0 = clean data
 * @param level_pct Noise level as percentage of signal RMS (0..100)
 */
void eit_acq_simulated_set_noise(eit_acq_backend_t *backend,
                                  int enabled, int32_t level_pct);

#ifdef __cplusplus
}
#endif

#endif /* EIT_ACQ_SIMULATED_H */
