#ifndef LBP_RECONSTRUCTION_H
#define LBP_RECONSTRUCTION_H

#include <stdint.h>

// Sensitivity matrix header (32 bytes)
typedef struct {
    uint32_t magic;           // 0x53454E53 ('SENS')
    uint32_t n_measurements;  // Total measurements (n_inj * n_meas)
    uint32_t n_pixels;        // Pixels in image (image_size^2)
    uint32_t image_size;      // Image size (32)
    uint32_t n_inj;           // Number of injections (79)
    uint32_t n_meas;          // Number of measurements (16)
    uint32_t reserved[2];     // Reserved
} __attribute__((packed)) SensitivityMatrixHeader;

// Reconstruction result structure
typedef struct {
    float* image_data;        // 32x32 reconstruction result
    uint16_t* color_buffer;   // Pre-upscaled RGB565 buffer (288x288)
    uint32_t image_size;      // Size of square image (32)
    uint32_t display_size;    // Display size (288)
    float vmin, vmax;         // Value range for colormap
    int success;              // 1 if successful, 0 otherwise
    char error_msg[128];      // Error message if failed
} ReconstructionResult;

// Initialize LBP reconstruction (load sensitivity matrix)
int lbp_init(void);

// Perform reconstruction using loaded data
// ref_uel: Reference Uel data (n_meas x n_inj)
// target_uel: Target Uel data (n_meas x n_inj)
// n_meas, n_inj: Dimensions
ReconstructionResult* lbp_reconstruct(const float* ref_uel, const float* target_uel, 
                                       uint16_t n_meas, uint16_t n_inj);

// Free reconstruction result
void lbp_free_result(ReconstructionResult* result);

// Get sensitivity matrix info
const SensitivityMatrixHeader* lbp_get_matrix_info(void);

#endif // LBP_RECONSTRUCTION_H
