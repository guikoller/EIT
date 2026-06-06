#ifndef LBP_RECONSTRUCTION_H
#define LBP_RECONSTRUCTION_H

#include <stdint.h>

#include "eit_config.h"

// Sensitivity matrix header (32 bytes) — shared with calibration
#include "services/sensitivity_matrix_format.h"

// Reconstruction result structure (pre-allocated, owned by LBP module)
typedef struct {
    float* image_data;        // EIT_IMAGE_SIZE² reconstruction result (static)
    uint16_t* color_buffer;   // Pre-upscaled RGB565 buffer (EIT_DISPLAY_SIZE², SDRAM)
    uint32_t image_size;      // Size of square image (EIT_IMAGE_SIZE)
    uint32_t display_size;    // Display size (EIT_DISPLAY_SIZE)
    float vmin, vmax;         // Value range for colormap
    int success;              // 1 if successful, 0 otherwise
    char error_msg[128];      // Error message if failed
} ReconstructionResult;

// Initialize LBP reconstruction (load sensitivity matrix)
int lbp_init(void);

// Perform reconstruction using pre-allocated internal buffers.
// Returns pointer to static result — valid until next lbp_reconstruct() call.
// Caller must NOT free the returned pointer.
ReconstructionResult* lbp_reconstruct(const float* ref_uel, const float* target_uel, 
                                       uint16_t n_meas, uint16_t n_inj);

// Get sensitivity matrix info
const SensitivityMatrixHeader* lbp_get_matrix_info(void);

#endif // LBP_RECONSTRUCTION_H
