#include "lbp_reconstruction.h"
#include "ff.h"
#include "stm32f769i_discovery_sdram.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// SDRAM address for sensitivity matrix (after LCD frame buffers)
// LCD uses first ~4MB, we use area after that
#define S_MATRIX_ADDR       ((uint32_t)0xC0400000)  // Offset 4MB into SDRAM
#define COLOR_BUFFER_ADDR   ((uint32_t)0xC0900000)  // Offset 9MB into SDRAM (after 5MB matrix)
#define MAX_MEASUREMENTS 1264  // 79 inj * 16 meas
#define MAX_PIXELS      1024   // 32 * 32

static SensitivityMatrixHeader s_header;
static float* s_matrix = NULL;  // Will point to SDRAM
static int initialized = 0;

/**
 * Initialize LBP reconstruction - load sensitivity matrix from SD card into SDRAM
 */
int lbp_init(void)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    
    if (initialized) {
        return 1;  // Already initialized
    }
    
    // Initialize SDRAM if not already done
    BSP_SDRAM_Init();
    
    // Open sensitivity matrix file
    res = f_open(&file, "0:/sensitivity_matrix.bin", FA_READ);
    if (res != FR_OK) {
        return 0;
    }
    
    // Read header
    res = f_read(&file, &s_header, sizeof(SensitivityMatrixHeader), &bytes_read);
    if (res != FR_OK || bytes_read != sizeof(SensitivityMatrixHeader)) {
        f_close(&file);
        return 0;
    }
    
    // Verify magic number
    if (s_header.magic != 0x53454E53) {
        f_close(&file);
        return 0;
    }
    
    // Verify dimensions
    if (s_header.n_measurements > MAX_MEASUREMENTS || s_header.n_pixels > MAX_PIXELS) {
        f_close(&file);
        return 0;
    }
    
    // Map SDRAM for sensitivity matrix
    s_matrix = (float*)S_MATRIX_ADDR;
    
    // Read matrix data directly into SDRAM (5.2 MB)
    uint32_t matrix_size = s_header.n_measurements * s_header.n_pixels * sizeof(float);
    uint32_t remaining = matrix_size;
    uint32_t offset = 0;
    uint32_t chunk_size = 32768;  // 32 KB chunks
    
    while (remaining > 0) {
        uint32_t to_read = (remaining > chunk_size) ? chunk_size : remaining;
        res = f_read(&file, (uint8_t*)s_matrix + offset, to_read, &bytes_read);
        
        if (res != FR_OK || bytes_read != to_read) {
            f_close(&file);
            return 0;
        }
        
        offset += bytes_read;
        remaining -= bytes_read;
    }
    
    f_close(&file);
    initialized = 1;
    
    return 1;
}

/**
 * Perform LBP reconstruction: result = S^T * delta_v
 */
ReconstructionResult* lbp_reconstruct(const float* ref_uel, const float* target_uel,
                                       uint16_t n_meas, uint16_t n_inj)
{
    if (!initialized) {
        return NULL;
    }
    
    // Allocate result structure
    ReconstructionResult* result = (ReconstructionResult*)malloc(sizeof(ReconstructionResult));
    if (!result) {
        return NULL;
    }
    
    result->image_size = s_header.image_size;
    result->success = 0;
    result->image_data = NULL;
    
    // Verify dimensions match
    uint32_t expected_measurements = n_meas * n_inj;
    if (expected_measurements != s_header.n_measurements) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Dimension mismatch: expected %lu, got %lu",
                 s_header.n_measurements, expected_measurements);
        return result;
    }
    
    // Allocate image buffer in internal RAM (4KB)
    result->image_data = (float*)malloc(s_header.n_pixels * sizeof(float));
    if (!result->image_data) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Failed to allocate image buffer");
        return result;
    }
    
    // Allocate delta_v in internal RAM (5KB)
    float* delta_v = (float*)malloc(s_header.n_measurements * sizeof(float));
    if (!delta_v) {
        snprintf(result->error_msg, sizeof(result->error_msg), "Failed to allocate delta_v");
        free(result->image_data);
        result->image_data = NULL;
        return result;
    }
    
    // Compute delta_v = target - reference
    // Flatten in row-major order (injection-major)
    uint32_t idx = 0;
    for (uint16_t inj = 0; inj < n_inj; inj++) {
        for (uint16_t meas = 0; meas < n_meas; meas++) {
            delta_v[idx] = target_uel[meas * n_inj + inj] - ref_uel[meas * n_inj + inj];
            idx++;
        }
    }
    
    // Perform S^T * delta_v
    // result[pixel] = sum over measurements of S[meas][pixel] * delta_v[meas]
    // Initialize result to zero
    for (uint32_t p = 0; p < s_header.n_pixels; p++) {
        result->image_data[p] = 0.0f;
    }
    
    // Matrix multiplication: iterate by pixel (better cache locality)
    for (uint32_t pixel = 0; pixel < s_header.n_pixels; pixel++) {
        float sum = 0.0f;
        
        // Dot product: S[0..n_measurements-1][pixel] · delta_v[0..n_measurements-1]
        for (uint32_t meas = 0; meas < s_header.n_measurements; meas++) {
            // Access S matrix from SDRAM: S[meas][pixel] = s_matrix[meas * n_pixels + pixel]
            float s_val = s_matrix[meas * s_header.n_pixels + pixel];
            sum += s_val * delta_v[meas];
        }
        
        result->image_data[pixel] = sum;
    }
    
    // Apply circular mask (pixels outside circle = NaN)
    float center = (s_header.image_size - 1) / 2.0f;
    float radius = s_header.image_size / 2.0f;
    
    for (uint32_t y = 0; y < s_header.image_size; y++) {
        for (uint32_t x = 0; x < s_header.image_size; x++) {
            float dx = x - center;
            float dy = y - center;
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist > radius) {
                result->image_data[y * s_header.image_size + x] = NAN;
            }
        }
    }
    
    // Find min/max for color scaling (excluding NaN)
    float vmin = INFINITY, vmax = -INFINITY;
    for (uint32_t i = 0; i < s_header.n_pixels; i++) {
        float val = result->image_data[i];
        if (!isnan(val)) {
            if (val < vmin) vmin = val;
            if (val > vmax) vmax = val;
        }
    }
    
    // Use symmetric range around zero
    float vabs = (fabsf(vmin) > fabsf(vmax)) ? fabsf(vmin) : fabsf(vmax);
    result->vmin = -vabs;
    result->vmax = vabs;
    
    // Pre-upscale to 288×288 RGB565 buffer in SDRAM (166KB)
    result->display_size = 288;
    uint32_t scale = result->display_size / s_header.image_size;  // 9
    result->color_buffer = (uint16_t*)COLOR_BUFFER_ADDR;  // Use SDRAM, no malloc needed
    
    // Generate upscaled RGB565 image with high-contrast colormap
    uint16_t bg_color = 0x0000;  // RGB565 for black background
    
    for (uint32_t dst_y = 0; dst_y < result->display_size; dst_y++) {
        for (uint32_t dst_x = 0; dst_x < result->display_size; dst_x++) {
            // Map back to source pixel
            uint32_t src_x = dst_x / scale;
            uint32_t src_y = dst_y / scale;
            float val = result->image_data[src_y * s_header.image_size + src_x];
            
            uint16_t color;
            if (isnan(val)) {
                color = bg_color;
            } else {
                // Apply blue-black-red colormap (high contrast)
                float norm;
                if (result->vmax - result->vmin > 0) {
                    norm = 2.0f * (val - result->vmin) / (result->vmax - result->vmin) - 1.0f;
                } else {
                    norm = 0.0f;
                }
                if (norm < -1.0f) norm = -1.0f;
                if (norm > 1.0f) norm = 1.0f;
                
                uint8_t r, g, b;
                if (norm < 0.0f) {
                    // Bright blue to black (negative values)
                    r = 0;
                    g = 0;
                    b = (uint8_t)((-norm) * 255);  // At -1: b=255 (blue), at 0: b=0 (black)
                } else {
                    // Black to bright red (positive values)
                    r = (uint8_t)(norm * 255);     // At 0: r=0 (black), at 1: r=255 (red)
                    g = 0;
                    b = 0;
                }
                
                // Convert to RGB565
                color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }
            
            result->color_buffer[dst_y * result->display_size + dst_x] = color;
        }
    }
    
    // Free delta_v
    free(delta_v);
    
    result->success = 1;
    snprintf(result->error_msg, sizeof(result->error_msg), "Success");
    
    return result;
}

/**
 * Free reconstruction result
 */
/**
 * Free reconstruction result
 */
void lbp_free_result(ReconstructionResult* result)
{
    if (result) {
        if (result->image_data) {
            free(result->image_data);
        }
        // color_buffer is in SDRAM, don't free it
        free(result);
    }
}

/**
 * Get sensitivity matrix info
 */
const SensitivityMatrixHeader* lbp_get_matrix_info(void)
{
    if (initialized) {
        return &s_header;
    }
    return NULL;
}
