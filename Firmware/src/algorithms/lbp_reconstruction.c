#include "lbp_reconstruction.h"   /* brings in eit_config.h */
#include "ff.h"
#include "stm32f769i_discovery_sdram.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static SensitivityMatrixHeader s_header;
static float* s_matrix = NULL;  // Will point to SDRAM
static int initialized = 0;

/* ---- Pre-allocated working buffers (no per-frame malloc) ---- */
static ReconstructionResult s_result;
static float    s_image_buf[EIT_MAX_PIXELS];        /* EIT_IMAGE_SIZE² floats */
static float    s_delta_v[EIT_MAX_MEASUREMENTS];    /* max measurements       */

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

    /* SDRAM is initialized during display/system bring-up (see tft_init).
     * Re-initializing SDRAM here can disrupt the LCD framebuffer and cause
     * persistent display corruption.
     */
    
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
    if (s_header.magic != SENS_MATRIX_MAGIC) {
        f_close(&file);
        return 0;
    }
    
    // Verify dimensions
    if (s_header.n_measurements > EIT_MAX_MEASUREMENTS || s_header.n_pixels > EIT_MAX_PIXELS) {
        f_close(&file);
        return 0;
    }
    
    // Map SDRAM for sensitivity matrix
    s_matrix = (float*)EIT_SDRAM_SENSITIVITY_ADDR;
    
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
 * Uses pre-allocated static buffers — zero heap allocations per frame.
 * Returns pointer to internal static result, valid until next call.
 */
ReconstructionResult* lbp_reconstruct(const float* ref_uel, const float* target_uel,
                                       uint16_t n_meas, uint16_t n_inj)
{
    if (!initialized) {
        return NULL;
    }
    
    ReconstructionResult* result = &s_result;
    result->image_size = s_header.image_size;
    result->success = 0;
    result->image_data = s_image_buf;
    
    // Verify dimensions match
    uint32_t expected_measurements = (uint32_t)n_meas * (uint32_t)n_inj;
    if (expected_measurements != s_header.n_measurements) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Dimension mismatch: expected %lu, got %lu",
                 s_header.n_measurements, expected_measurements);
        result->image_data = NULL;
        return result;
    }
    
    // Compute delta_v = target - reference (into static buffer)
    uint32_t idx = 0;
    for (uint16_t inj = 0; inj < n_inj; inj++) {
        for (uint16_t meas = 0; meas < n_meas; meas++) {
            s_delta_v[idx] = target_uel[meas * n_inj + inj] - ref_uel[meas * n_inj + inj];
            idx++;
        }
    }
    
    // Perform S^T * delta_v
    for (uint32_t pixel = 0; pixel < s_header.n_pixels; pixel++) {
        float sum = 0.0f;
        
        for (uint32_t meas = 0; meas < s_header.n_measurements; meas++) {
            float s_val = s_matrix[meas * s_header.n_pixels + pixel];
            sum += s_val * s_delta_v[meas];
        }
        
        s_image_buf[pixel] = sum;
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
                s_image_buf[y * s_header.image_size + x] = NAN;
            }
        }
    }
    
    // Find min/max for color scaling (excluding NaN)
    float vmin = INFINITY, vmax = -INFINITY;
    for (uint32_t i = 0; i < s_header.n_pixels; i++) {
        float val = s_image_buf[i];
        if (!isnan(val)) {
            if (val < vmin) vmin = val;
            if (val > vmax) vmax = val;
        }
    }
    
    // Use symmetric range around zero
    float vabs = (fabsf(vmin) > fabsf(vmax)) ? fabsf(vmin) : fabsf(vmax);
    result->vmin = -vabs;
    result->vmax = vabs;
    
    // Pre-upscale to EIT_DISPLAY_SIZE² RGB565 buffer in SDRAM
    result->display_size = EIT_DISPLAY_SIZE;
    uint32_t scale = result->display_size / s_header.image_size;
    result->color_buffer = (uint16_t*)EIT_SDRAM_COLOR_BUF_ADDR;
    
    uint16_t bg_color = 0x0000;
    
    for (uint32_t dst_y = 0; dst_y < result->display_size; dst_y++) {
        for (uint32_t dst_x = 0; dst_x < result->display_size; dst_x++) {
            uint32_t src_x = dst_x / scale;
            uint32_t src_y = dst_y / scale;
            float val = s_image_buf[src_y * s_header.image_size + src_x];
            
            uint16_t color;
            if (isnan(val)) {
                color = bg_color;
            } else {
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
                    r = 0;
                    g = 0;
                    b = (uint8_t)((-norm) * 255);
                } else {
                    r = (uint8_t)(norm * 255);
                    g = 0;
                    b = 0;
                }
                
                color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
            }
            
            result->color_buffer[dst_y * result->display_size + dst_x] = color;
        }
    }
    
    result->success = 1;
    snprintf(result->error_msg, sizeof(result->error_msg), "Success");
    
    return result;
}

/**
 * Free reconstruction result — no-op since buffers are static.
 * Kept for backward compatibility with code that still calls it.
 */
void lbp_free_result(ReconstructionResult* result)
{
    (void)result;
    /* All buffers are static or SDRAM — nothing to free */
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
