#include "reconstruction_viewer.h"
#include "data_viewer.h"
#include "sd_file_browser.h"
#include "lbp_reconstruction.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// Reconstruction parameters
#define RECON_SIZE 32
#define DISPLAY_SIZE 288  // 32x9 upscaling (288x288 = 165KB buffer)

static lv_obj_t *recon_screen = NULL;
static lv_obj_t *canvas = NULL;
static lv_obj_t *label_title = NULL;
static lv_obj_t *label_status = NULL;
static char current_filename[128];
static ReconstructionResult* recon_result = NULL;

static void recon_return_async_cb(void * user_data)
{
    (void)user_data;

    if (recon_result) {
        lbp_free_result(recon_result);
        recon_result = NULL;
    }

    lv_obj_clean(lv_screen_active());
    sd_file_browser_create();
}

// Binary file header structure (20 bytes)
typedef struct {
    uint32_t magic;              // 0x45495442 ('EITB') - 4 bytes
    uint16_t n_meas;             // Number of measurements - 2 bytes
    uint16_t n_inj;              // Number of injections - 2 bytes
    uint16_t image_size;         // Reconstruction grid size - 2 bytes
    uint16_t curr_pattern_rows;  // CurrentPattern rows - 2 bytes
    uint16_t meas_pattern_rows;  // MeasPattern rows - 2 bytes
    uint16_t reserved;           // Padding - 2 bytes
    uint32_t reserved2;          // Padding - 4 bytes
} __attribute__((packed)) BinFileHeader;

/**
 * Load Uel data from binary file
 */
static float** load_uel_from_file(const char *filename, uint16_t *out_n_meas, uint16_t *out_n_inj)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    char filepath[128];
    
    // Build full path
    snprintf(filepath, sizeof(filepath), "0:/%s", filename);
    
    // Open file
    res = f_open(&file, filepath, FA_READ);
    if(res != FR_OK) {
        return NULL;
    }
    
    // Read header
    BinFileHeader header;
    res = f_read(&file, &header, sizeof(header), &bytes_read);
    if(res != FR_OK || bytes_read != sizeof(header)) {
        f_close(&file);
        return NULL;
    }
    
    // Verify magic number
    if(header.magic != 0x45495442) {
        f_close(&file);
        return NULL;
    }
    
    *out_n_meas = header.n_meas;
    *out_n_inj = header.n_inj;
    
    // Allocate 1D buffer for Uel data
    uint32_t uel_size = header.n_meas * header.n_inj;
    float *uel_buffer = (float*)malloc(uel_size * sizeof(float));
    if(uel_buffer == NULL) {
        f_close(&file);
        return NULL;
    }
    
    // Read Uel data
    res = f_read(&file, uel_buffer, uel_size * sizeof(float), &bytes_read);
    if(res != FR_OK || bytes_read != uel_size * sizeof(float)) {
        free(uel_buffer);
        f_close(&file);
        return NULL;
    }
    
    f_close(&file);
    
    // Create 2D array pointers
    float **uel_2d = (float**)malloc(header.n_meas * sizeof(float*));
    if (!uel_2d) {
        free(uel_buffer);
        return NULL;
    }
    
    for (uint16_t i = 0; i < header.n_meas; i++) {
        uel_2d[i] = &uel_buffer[i * header.n_inj];
    }
    
    return uel_2d;
}

/**
 * Free Uel 2D array
 */
static void free_uel(float **uel_2d)
{
    if (uel_2d) {
        if (uel_2d[0]) {
            free(uel_2d[0]);  // Free the 1D buffer
        }
        free(uel_2d);  // Free the pointer array
    }
}

/**
 * Return button click handler
 */
static void return_btn_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        /* Don't delete/clean objects inside the event call stack */
        lv_async_call(recon_return_async_cb, NULL);
    }
}

/**
 * Create canvas for displaying reconstruction
 */
static void create_canvas(lv_obj_t *parent)
{
    // Create canvas
    canvas = lv_canvas_create(parent);
    
    // Allocate buffer for canvas (RGB565, 2 bytes per pixel)
    static lv_color_t canvas_buf[DISPLAY_SIZE * DISPLAY_SIZE];
    lv_canvas_set_buffer(canvas, canvas_buf, DISPLAY_SIZE, DISPLAY_SIZE, LV_COLOR_FORMAT_RGB565);
    
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 20);

    // Fill with black background initially; decorations are drawn after reconstruction
    lv_canvas_fill_bg(canvas, lv_color_hex(0x000000), LV_OPA_COVER);
}

/**
 * Add electrode markers as labels
 */
static void add_electrode_markers(lv_obj_t *parent)
{
    int center_x = DISPLAY_SIZE / 2;
    int center_y = DISPLAY_SIZE / 2;
    int radius = DISPLAY_SIZE / 2 + 20;
    
    for(int i = 0; i < 16; i++)
    {
        float angle = (i * 2.0f * 3.14159f) / 16.0f;
        int x = center_x + (int)(radius * cosf(angle));
        int y = center_y - (int)(radius * sinf(angle));
        
        lv_obj_t *label = lv_label_create(parent);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_label_set_text(label, num);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        
        lv_obj_align_to(label, canvas, LV_ALIGN_TOP_LEFT, x - 8, y - 8);
    }
}

/**
 * Map value to blue-white-red colormap
 * (No longer used - colormap applied in lbp_reconstruct)
 */
static lv_color_t value_to_color(float value, float vmin, float vmax)
{
    // Normalize value to [-1, 1]
    float norm;
    if (vmax - vmin > 0) {
        norm = 2.0f * (value - vmin) / (vmax - vmin) - 1.0f;
    } else {
        norm = 0.0f;
    }
    
    // Clamp to [-1, 1]
    if (norm < -1.0f) norm = -1.0f;
    if (norm > 1.0f) norm = 1.0f;
    
    uint8_t r, g, b;
    
    if (norm < 0.0f) {
        // Blue to white: norm in [-1, 0]
        float t = (norm + 1.0f);  // [0, 1]
        r = (uint8_t)(t * 255);
        g = (uint8_t)(t * 255);
        b = 255;
    } else {
        // White to red: norm in [0, 1]
        float t = norm;
        r = 255;
        g = (uint8_t)((1.0f - t) * 255);
        b = (uint8_t)((1.0f - t) * 255);
    }
    
    return lv_color_make(r, g, b);
}

/**
 * Render reconstruction image on canvas (directly copy pre-upscaled buffer)
 */
static void render_reconstruction(void)
{
    if (!recon_result || !recon_result->success || !recon_result->color_buffer) {
        lv_label_set_text(label_status, "No reconstruction data");
        return;
    }
    
    // Copy pre-upscaled RGB565 data directly into canvas buffer
    lv_color_t * canvas_buf = (lv_color_t *)lv_canvas_get_buf(canvas);
    if (!canvas_buf) {
        lv_label_set_text(label_status, "Canvas buffer missing");
        return;
    }

    memcpy(canvas_buf, recon_result->color_buffer,
           DISPLAY_SIZE * DISPLAY_SIZE * sizeof(uint16_t));

    // Draw circle border and electrode dots directly into RGB565 buffer
    uint16_t * buf16 = (uint16_t *)canvas_buf;
    uint16_t white_color = 0xFFFF;
    int center = DISPLAY_SIZE / 2;
    int radius = DISPLAY_SIZE / 2;

    // Circle border (2 pixels wide)
    for (int angle_deg = 0; angle_deg < 360; angle_deg++) {
        float angle = angle_deg * 3.14159f / 180.0f;
        for (int r = radius - 2; r <= radius - 1; r++) {
            int x = center + (int)(r * cosf(angle));
            int y = center - (int)(r * sinf(angle));
            if (x >= 0 && x < DISPLAY_SIZE && y >= 0 && y < DISPLAY_SIZE) {
                buf16[y * DISPLAY_SIZE + x] = white_color;
            }
        }
    }

    // Electrode dots (filled circles, radius 5)
    int electrode_radius = DISPLAY_SIZE / 2 - 1;
    for (int i = 0; i < 16; i++) {
        float angle = (i * 2.0f * 3.14159f) / 16.0f;
        int ex = center + (int)(electrode_radius * cosf(angle));
        int ey = center - (int)(electrode_radius * sinf(angle));

        for (int dy = -5; dy <= 5; dy++) {
            for (int dx = -5; dx <= 5; dx++) {
                if (dx * dx + dy * dy <= 25) {
                    int px = ex + dx;
                    int py = ey + dy;
                    if (px >= 0 && px < DISPLAY_SIZE && py >= 0 && py < DISPLAY_SIZE) {
                        buf16[py * DISPLAY_SIZE + px] = white_color;
                    }
                }
            }
        }
    }

    lv_obj_invalidate(canvas);
    
    // Update status with range
    char status[128];
    int32_t vmin_int = (int32_t)(recon_result->vmin * 1000000);
    int32_t vmax_int = (int32_t)(recon_result->vmax * 1000000);
    snprintf(status, sizeof(status), "Range: [%ld.%lduV, %ld.%lduV]", 
             vmin_int/1000000, (vmin_int/1000)%1000,
             vmax_int/1000000, (vmax_int/1000)%1000);
    lv_label_set_text(label_status, status);
}

/**
 * Create reconstruction viewer screen
 */
void reconstruction_viewer_create(const char *filename)
{
    // Save filename
    strncpy(current_filename, filename, sizeof(current_filename) - 1);
    current_filename[sizeof(current_filename) - 1] = '\0';
    
    // Clean screen
    lv_obj_clean(lv_screen_active());
    
    // Create screen container
    recon_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(recon_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(recon_screen, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(recon_screen, 0, 0);
    lv_obj_set_style_pad_all(recon_screen, 0, 0);
    lv_obj_remove_flag(recon_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(recon_screen, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Title
    label_title = lv_label_create(recon_screen);
    char title_text[256];
    snprintf(title_text, sizeof(title_text), "EIT Reconstruction - %s", filename);
    lv_label_set_text(label_title, title_text);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Status label
    label_status = lv_label_create(recon_screen);
    lv_label_set_text(label_status, "Loading reference data...");
    lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 40);
    
    // Create canvas
    create_canvas(recon_screen);
    
    // Add electrode markers
    add_electrode_markers(recon_screen);
    
    // Return button
    lv_obj_t *btn_return = lv_button_create(recon_screen);
    lv_obj_set_size(btn_return, 180, 50);
    lv_obj_align(btn_return, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_return, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(btn_return, 5, 0);
    lv_obj_add_event_cb(btn_return, return_btn_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *label_return = lv_label_create(btn_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " RETURN TO MENU");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);
    
    // Load reference data (datamat_1_0.bin)
    uint16_t ref_n_meas, ref_n_inj;
    float **ref_uel = load_uel_from_file("datamat_1_0.bin", &ref_n_meas, &ref_n_inj);
    
    if (!ref_uel) {
        lv_label_set_text(label_status, "ERROR: Reference file datamat_1_0.bin not found on SD card");
        return;
    }
    
    // Update status
    lv_label_set_text(label_status, "Loading target data...");
    
    // Load target data
    uint16_t tgt_n_meas, tgt_n_inj;
    float **tgt_uel = load_uel_from_file(filename, &tgt_n_meas, &tgt_n_inj);
    
    if (!tgt_uel) {
        free_uel(ref_uel);
        char err[128];
        snprintf(err, sizeof(err), "ERROR: Failed to load target file %s", filename);
        lv_label_set_text(label_status, err);
        return;
    }
    
    // Validate dimensions match
    if (ref_n_meas != tgt_n_meas || ref_n_inj != tgt_n_inj) {
        free_uel(ref_uel);
        free_uel(tgt_uel);
        char err[128];
        snprintf(err, sizeof(err), "ERROR: Dimension mismatch ref[%dx%d] vs tgt[%dx%d]",
                 ref_n_meas, ref_n_inj, tgt_n_meas, tgt_n_inj);
        lv_label_set_text(label_status, err);
        return;
    }
    
    // Update status
    lv_label_set_text(label_status, "Performing reconstruction...");
    
    // Initialize LBP if not already done (loads sensitivity matrix)
    if (!lbp_get_matrix_info()) {
        lv_label_set_text(label_status, "Initializing LBP (loading sensitivity matrix)...");
        
        if (!lbp_init()) {
            free_uel(ref_uel);
            free_uel(tgt_uel);
            lv_label_set_text(label_status, "ERROR: Failed to load sensitivity_matrix.bin from SD card");
            return;
        }
    }
    
    // Perform reconstruction
    // lbp_reconstruct expects 1D arrays, so pass the underlying buffer (ref_uel[0])
    recon_result = lbp_reconstruct(ref_uel[0], tgt_uel[0], ref_n_meas, ref_n_inj);
    
    // Free Uel data
    free_uel(ref_uel);
    free_uel(tgt_uel);
    
    // Check result
    if (!recon_result || !recon_result->success) {
        char err[256];
        if (recon_result) {
            snprintf(err, sizeof(err), "RECON ERROR: %s", recon_result->error_msg);
        } else {
            snprintf(err, sizeof(err), "RECON ERROR: lbp_init not called or sensitivity_matrix.bin missing");
        }
        lv_label_set_text(label_status, err);
        return;
    }
    
    // Render result
    render_reconstruction();
}

/**
 * Destroy reconstruction viewer
 */
void reconstruction_viewer_destroy(void)
{
    // Free reconstruction result
    if (recon_result) {
        lbp_free_result(recon_result);
        recon_result = NULL;
    }
    
    lv_obj_clean(lv_screen_active());
    data_viewer_create(current_filename);
}
