#include "lbp_reconstruction.h"   /* brings in eit_config.h */
#include "eidors_mask_32.h"
#include "app/app_coordinator.h"
#include "ff.h"
#include "stm32f769i_discovery_sdram.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static SensitivityMatrixHeader s_header;
static float* s_matrix = NULL;  // Will point to SDRAM
static int initialized = 0;

static float clamp01(float x)
{
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

/* Approximation of MATLAB/Octave jet colormap on [0,1]. */
static uint16_t jet_rgb565_from_norm(float t)
{
    float r = clamp01(1.5f - fabsf(4.0f * t - 3.0f));
    float g = clamp01(1.5f - fabsf(4.0f * t - 2.0f));
    float b = clamp01(1.5f - fabsf(4.0f * t - 1.0f));

    uint8_t r8 = (uint8_t)(255.0f * r);
    uint8_t g8 = (uint8_t)(255.0f * g);
    uint8_t b8 = (uint8_t)(255.0f * b);

    return (uint16_t)(((r8 & 0xF8u) << 8) | ((g8 & 0xFCu) << 3) | (b8 >> 3));
}

/* ---- Pre-allocated working buffers (no per-frame malloc) ---- */
static ReconstructionResult s_result;
static float    s_image_buf[EIT_MAX_PIXELS];        /* EIT_IMAGE_SIZE² floats */
static float    s_delta_v[EIT_MAX_MEASUREMENTS];    /* max measurements       */
static float    s_rot_buf[EIT_MAX_PIXELS];          /* rotation scratch       */

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

    // Strict mode: only accept sensitivity matrices compatible with
    // the EIDORS LUT mask used for scientific validation.
    if (s_header.image_size != EIDORS_MASK_SIZE) {
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

    if (s_header.image_size != EIDORS_MASK_SIZE) {
        snprintf(result->error_msg, sizeof(result->error_msg),
                 "Unsupported image_size=%lu for EIDORS LUT (expected %u)",
                 (unsigned long)s_header.image_size, (unsigned)EIDORS_MASK_SIZE);
        result->image_data = NULL;
        return result;
    }
    
    // Compute delta_v with EIDORS-compatible sign convention.
    uint32_t idx = 0;
    for (uint16_t inj = 0; inj < n_inj; inj++) {
        for (uint16_t meas = 0; meas < n_meas; meas++) {
            s_delta_v[idx] = ref_uel[meas * n_inj + inj] - target_uel[meas * n_inj + inj];
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

    // Rotate reconstruction 90 degrees left in firmware so host scripts
    // don't need to apply orientation correction on MCU CSVs.
    {
        uint32_t n = s_header.image_size;
        for (uint32_t y = 0; y < n; y++) {
            for (uint32_t x = 0; x < n; x++) {
                uint32_t dst = y * n + x;
                uint32_t src = x * n + (n - 1u - y);
                s_rot_buf[dst] = s_image_buf[src];
            }
        }
        memcpy(s_image_buf, s_rot_buf, s_header.n_pixels * sizeof(float));
    }
    
    // Apply EIDORS-derived mask LUT to match PC reconstruction support.
    for (uint32_t y = 0; y < s_header.image_size; y++) {
        for (uint32_t x = 0; x < s_header.image_size; x++) {
            uint32_t p = y * s_header.image_size + x;
            if (g_eidors_mask_32[p] == 0u) {
                s_image_buf[p] = NAN;
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
    
    // Pre-upscale to RGB565 buffer in SDRAM at runtime display size
    const app_state_t *app_st = app_coordinator_get_state();
    uint32_t disp_sz = eit_display_size_for_setting(app_st->settings.image_size);
    result->display_size = disp_sz;
    uint32_t scale = disp_sz / s_header.image_size;
    result->color_buffer = (uint16_t*)EIT_SDRAM_COLOR_BUF_ADDR;
    
    /* Light theme background: 0xf5f5f5 -> RGB565 = 0xF7BE */
    uint16_t bg_color = 0xF7BE;
    
    for (uint32_t dst_y = 0; dst_y < disp_sz; dst_y++) {
        for (uint32_t dst_x = 0; dst_x < disp_sz; dst_x++) {
            uint32_t src_x = dst_x / scale;
            uint32_t src_y = dst_y / scale;
            float val = s_image_buf[src_y * s_header.image_size + src_x];
            
            uint16_t color;
            if (isnan(val)) {
                color = bg_color;
            } else {
                float t;
                if (result->vmax - result->vmin > 0.0f) {
                    t = (val - result->vmin) / (result->vmax - result->vmin);
                } else {
                    t = 0.5f;
                }
                t = clamp01(t);
                color = jet_rgb565_from_norm(t);
            }
            
            result->color_buffer[dst_y * disp_sz + dst_x] = color;
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
