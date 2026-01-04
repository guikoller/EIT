#ifndef LBP_RECONSTRUCTION_H
#define LBP_RECONSTRUCTION_H

#include <stdint.h>

// Sensitivity matrix file header
typedef struct {
    uint32_t magic;           // 0x53454E53 ('SENS')
    uint32_t n_measurements;  // n_inj * n_meas (should be 1264)
    uint32_t n_pixels;        // image_size^2 (should be 1024)
    uint32_t image_size;      // 32
    uint32_t n_inj;           // Number of current injections (79)
    uint32_t n_meas;          // Number of measurements per injection (16)
    uint32_t reserved[2];     // Reserved for future use
} __attribute__((packed)) SensitivityMatrixHeader;

// Reconstruction result
typedef struct {
    float *image_data;        // 2D array of image_size x image_size
    uint32_t image_size;      // Size of reconstruction (32 typically)
    int success;              // 1 if successful, 0 otherwise
    char error_msg[128];      // Error message if failed
} ReconstructionResult;

// Initialize LBP reconstruction (load sensitivity matrix from SD card)
int lbp_init(void);

// Perform LBP reconstruction
// ref_uel: reference voltage measurements [n_meas][n_inj]
// target_uel: target voltage measurements [n_meas][n_inj]
// n_meas: number of measurements per injection
// n_inj: number of current injections
ReconstructionResult* lbp_reconstruct(
    float **ref_uel, 
    float **target_uel, 
    uint32_t n_meas, 
    uint32_t n_inj);

// Free reconstruction result
void lbp_free_result(ReconstructionResult *result);

// Get matrix info (for debugging)
const SensitivityMatrixHeader* lbp_get_matrix_info(void);

#endif // LBP_RECONSTRUCTION_H
