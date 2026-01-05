#include "data_viewer.h"
#include "sd_file_browser.h"
#include "reconstruction_viewer.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static lv_obj_t *viewer_screen = NULL;
static lv_obj_t *tabview = NULL;
static lv_obj_t *label_title = NULL;
static float *uel_data = NULL;
static int8_t *curr_pattern = NULL;
static int8_t *meas_pattern = NULL;
static uint16_t n_meas = 0;
static uint16_t n_inj = 0;
static uint16_t curr_pattern_rows = 0;
static uint16_t meas_pattern_rows = 0;

static lv_obj_t *table_curr = NULL;
static lv_obj_t *table_uel = NULL;
static lv_obj_t *table_meas = NULL;
static bool curr_populated = false;
static bool uel_populated = false;
static bool meas_populated = false;
static char loaded_filename[128];

static void run_async_cb(void * user_data)
{
    (void)user_data;
    reconstruction_viewer_create(loaded_filename);
}

static void return_async_cb(void * user_data)
{
    (void)user_data;
    data_viewer_destroy();
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
        lv_async_call(return_async_cb, NULL);
    }
}

/**
 * Run button click handler
 */
static void run_btn_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        /* Don't delete/clean objects inside the event call stack */
        lv_async_call(run_async_cb, NULL);
    }
}

/**
 * Load binary file from SD card
 */
static int load_binary_file(const char *filename)
{
    FIL file;
    FRESULT res;
    UINT bytes_read;
    char filepath[128];
    
    // Build full path
    snprintf(filepath, sizeof(filepath), "0:/%s", filename);
    
    // Open file
    res = f_open(&file, filepath, FA_READ);
    if(res != FR_OK)
    {
        return -1;
    }
    
    // Read header
    BinFileHeader header;
    res = f_read(&file, &header, sizeof(header), &bytes_read);
    if(res != FR_OK || bytes_read != sizeof(header))
    {
        f_close(&file);
        return -2;
    }
    
    // Verify magic number
    if(header.magic != 0x45495442)
    {
        f_close(&file);
        return -3;
    }
    
    n_meas = header.n_meas;
    n_inj = header.n_inj;
    curr_pattern_rows = header.curr_pattern_rows;
    meas_pattern_rows = header.meas_pattern_rows;
    
    // Allocate memory for Uel data
    uint32_t uel_size = n_meas * n_inj;
    if(uel_data != NULL)
    {
        free(uel_data);
    }
    uel_data = (float*)malloc(uel_size * sizeof(float));
    if(uel_data == NULL)
    {
        f_close(&file);
        return -4;
    }
    
    // Read Uel data
    res = f_read(&file, uel_data, uel_size * sizeof(float), &bytes_read);
    if(res != FR_OK || bytes_read != uel_size * sizeof(float))
    {
        free(uel_data);
        uel_data = NULL;
        f_close(&file);
        return -5;
    }
    
    // Allocate and read CurrentPattern if present
    if(curr_pattern_rows > 0)
    {
        uint32_t curr_size = curr_pattern_rows * n_inj;
        if(curr_pattern != NULL)
        {
            free(curr_pattern);
        }
        curr_pattern = (int8_t*)malloc(curr_size);
        if(curr_pattern != NULL)
        {
            res = f_read(&file, curr_pattern, curr_size, &bytes_read);
            if(res != FR_OK || bytes_read != curr_size)
            {
                free(curr_pattern);
                curr_pattern = NULL;
            }
        }
    }
    else
    {
        /* New file has no CurrentPattern; clear any previous buffer */
        if(curr_pattern != NULL)
        {
            free(curr_pattern);
            curr_pattern = NULL;
        }
    }
    
    // Allocate and read MeasPattern if present
    if(meas_pattern_rows > 0)
    {
        uint32_t meas_size = meas_pattern_rows * n_meas;
        if(meas_pattern != NULL)
        {
            free(meas_pattern);
        }
        meas_pattern = (int8_t*)malloc(meas_size);
        if(meas_pattern != NULL)
        {
            res = f_read(&file, meas_pattern, meas_size, &bytes_read);
            if(res != FR_OK || bytes_read != meas_size)
            {
                free(meas_pattern);
                meas_pattern = NULL;
            }
        }
    }
    else
    {
        /* New file has no MeasPattern; clear any previous buffer */
        if(meas_pattern != NULL)
        {
            free(meas_pattern);
            meas_pattern = NULL;
        }
    }
    
    f_close(&file);
    return 0;
}

/**
 * Populate table with CurrentPattern data
 */
static void populate_current_pattern(lv_obj_t *table)
{
    if(curr_pattern == NULL || curr_pattern_rows == 0) return;
    if(curr_populated) return;  // Skip if already populated
    
    uint16_t rows = curr_pattern_rows;
    uint16_t cols = n_inj;  // Show all columns
    
    lv_table_set_row_count(table, rows + 1);
    lv_table_set_column_count(table, cols + 1);
    
    lv_table_set_column_width(table, 0, 60);
    for(uint16_t i = 1; i <= cols; i++)
    {
        lv_table_set_column_width(table, i, 60);
    }
    
    lv_table_set_cell_value(table, 0, 0, "E");
    for(uint16_t j = 0; j < cols; j++)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "I%d", j);
        lv_table_set_cell_value(table, 0, j + 1, buf);
    }
    
    for(uint16_t i = 0; i < rows; i++)
    {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "E%d", i);
        lv_table_set_cell_value(table, i + 1, 0, row_label);
        
        for(uint16_t j = 0; j < cols; j++)
        {
            int8_t value = curr_pattern[i * n_inj + j];
            char buf[16];
            if(value == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if(value > 0) {
                snprintf(buf, sizeof(buf), "+%d", value);
            } else {
                snprintf(buf, sizeof(buf), "%d", value);
            }
            lv_table_set_cell_value(table, i + 1, j + 1, buf);
        }
    }
    
    // Force style refresh
    lv_obj_invalidate(table);
    curr_populated = true;
}

/**
 * Populate table with Uel data
 */
static void populate_uel(lv_obj_t *table)
{
    if(uel_data == NULL) return;
    if(uel_populated) return;  // Skip if already populated
    
    uint16_t rows = n_meas;
    uint16_t cols = (n_inj > 15) ? 15 : n_inj;  // Limit to 15 for performance (float conversion is expensive)
    
    lv_table_set_row_count(table, rows + 1);
    lv_table_set_column_count(table, cols + 1);
    
    lv_table_set_column_width(table, 0, 60);
    for(uint16_t i = 1; i <= cols; i++)
    {
        lv_table_set_column_width(table, i, 70);  // Narrower for more columns
    }
    
    lv_table_set_cell_value(table, 0, 0, "M");
    for(uint16_t j = 0; j < cols; j++)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "I%d", j);
        lv_table_set_cell_value(table, 0, j + 1, buf);
    }
    
    for(uint16_t i = 0; i < rows; i++)
    {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "M%d", i);
        lv_table_set_cell_value(table, i + 1, 0, row_label);
        
        for(uint16_t j = 0; j < cols; j++)
        {
            char buf[32];
            float value = uel_data[i * n_inj + j];
            
            // Convert float to millivolts (integer) for display
            // Multiply by 1000 and cast to int
            int32_t value_mv = (int32_t)(value * 1000.0f);
            
            // Display as integer millivolts
            if(value_mv == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if(value_mv >= 1000 || value_mv <= -1000) {
                // Display in volts with 3 decimals
                int32_t volts = value_mv / 1000;
                int32_t millis = value_mv % 1000;
                if(millis < 0) millis = -millis;
                snprintf(buf, sizeof(buf), "%ld.%03ld", (long)volts, (long)millis);
            } else {
                // Display in millivolts
                snprintf(buf, sizeof(buf), "%ldmV", (long)value_mv);
            }
            
            lv_table_set_cell_value(table, i + 1, j + 1, buf);
        }
    }
    
    // Apply styling after population
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(table, lv_color_hex(0x1a1a1a), LV_PART_ITEMS);
    
    // Force style refresh
    lv_obj_invalidate(table);
    uel_populated = true;
}

/**
 * Populate table with MeasPattern data
 */
static void populate_meas_pattern(lv_obj_t *table)
{
    if(meas_pattern == NULL || meas_pattern_rows == 0) return;
    if(meas_populated) return;  // Skip if already populated
    
    uint16_t rows = meas_pattern_rows;
    uint16_t cols = n_meas;
    
    lv_table_set_row_count(table, rows + 1);
    lv_table_set_column_count(table, cols + 1);
    
    lv_table_set_column_width(table, 0, 60);
    for(uint16_t i = 1; i <= cols; i++)
    {
        lv_table_set_column_width(table, i, 60);
    }
    
    lv_table_set_cell_value(table, 0, 0, "E");
    for(uint16_t j = 0; j < cols; j++)
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "M%d", j);
        lv_table_set_cell_value(table, 0, j + 1, buf);
    }
    
    for(uint16_t i = 0; i < rows; i++)
    {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "E%d", i);
        lv_table_set_cell_value(table, i + 1, 0, row_label);
        
        for(uint16_t j = 0; j < cols; j++)
        {
            int8_t value = meas_pattern[i * n_meas + j];
            char buf[16];
            if(value == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if(value > 0) {
                snprintf(buf, sizeof(buf), "+%d", value);
            } else {
                snprintf(buf, sizeof(buf), "%d", value);
            }
            lv_table_set_cell_value(table, i + 1, j + 1, buf);
        }
    }
    
    // Force style refresh
    lv_obj_invalidate(table);
    meas_populated = true;
}

/**
 * Tab changed event handler
 */
static void tab_changed_event_cb(lv_event_t *e)
{
    uint32_t tab_id = lv_tabview_get_tab_active(tabview);
    
    // Populate table on first access
    switch(tab_id)
    {
        case 0:  // Current Pattern
            if(!curr_populated && table_curr != NULL)
                populate_current_pattern(table_curr);
            break;
        case 1:  // Uel
            if(!uel_populated && table_uel != NULL)
                populate_uel(table_uel);
            break;
        case 2:  // Meas Pattern
            if(!meas_populated && table_meas != NULL)
                populate_meas_pattern(table_meas);
            break;
    }
}

/**
 * Style table with dark theme
 */
static void style_table(lv_obj_t *table)
{
    lv_obj_set_style_bg_color(table, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(table, 0, 0);
    
    lv_obj_set_style_bg_color(table, lv_color_hex(0x1a1a1a), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(table, lv_color_hex(0x555555), LV_PART_ITEMS);
}

/**
 * Create data viewer screen
 */
void data_viewer_create(const char *filename)
{
    // Save filename for later use
    strncpy(loaded_filename, filename, sizeof(loaded_filename) - 1);
    loaded_filename[sizeof(loaded_filename) - 1] = '\0';

    /* Always reset per-file population state.
     * If we navigated away via RUN, data_viewer_destroy() wasn't called.
     */
    curr_populated = false;
    uel_populated = false;
    meas_populated = false;
    
    // Load binary file
    int result = load_binary_file(filename);
    if(result != 0)
    {
        sd_file_browser_create();
        return;
    }
    
    lv_obj_clean(lv_screen_active());
    
    viewer_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(viewer_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(viewer_screen, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(viewer_screen, 0, 0);
    lv_obj_set_style_pad_all(viewer_screen, 0, 0);
    lv_obj_remove_flag(viewer_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(viewer_screen, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // Title
    label_title = lv_label_create(viewer_screen);
    char title_text[256];
    snprintf(title_text, sizeof(title_text), "%s | Uel: %d vals", filename, n_meas * n_inj);
    lv_label_set_text(label_title, title_text);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Create tabview
    tabview = lv_tabview_create(viewer_screen);
    lv_obj_set_size(tabview, LV_HOR_RES - 20, LV_VER_RES - 100);

    lv_obj_align(tabview, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(tabview, lv_color_hex(0x0a0a0a), 0);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 50);
    
    // Tab 1: Current Pattern
    lv_obj_t *tab_curr = lv_tabview_add_tab(tabview, "Current");
    table_curr = lv_table_create(tab_curr);
    lv_obj_set_size(table_curr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(table_curr, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(table_curr, lv_color_white(), 0);
    style_table(table_curr);
    
    // Tab 2: Uel
    lv_obj_t *tab_uel = lv_tabview_add_tab(tabview, "Uel");
    table_uel = lv_table_create(tab_uel);
    lv_obj_set_size(table_uel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(table_uel, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(table_uel, lv_color_white(), 0);
    style_table(table_uel);
    
    // Tab 3: Meas Pattern
    lv_obj_t *tab_meas = lv_tabview_add_tab(tabview, "Meas");
    table_meas = lv_table_create(tab_meas);
    lv_obj_set_size(table_meas, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(table_meas, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(table_meas, lv_color_white(), 0);
    style_table(table_meas);
    
    // Add tab change event
    lv_obj_add_event_cb(tabview, tab_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Populate only the first tab initially
    populate_current_pattern(table_curr);
    
    // Return button
    lv_obj_t *btn_return = lv_button_create(viewer_screen);
    lv_obj_set_size(btn_return, 150, 50);
    lv_obj_align(btn_return, LV_ALIGN_BOTTOM_LEFT, 25, -10);
    lv_obj_set_style_bg_color(btn_return, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(btn_return, 5, 0);
    lv_obj_add_event_cb(btn_return, return_btn_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *label_return = lv_label_create(btn_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " RETURN");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);
    
    // Run button
    lv_obj_t *btn_run = lv_button_create(viewer_screen);
    lv_obj_set_size(btn_run, 150, 50);
    lv_obj_align(btn_run, LV_ALIGN_BOTTOM_RIGHT, -25, -10);
    lv_obj_set_style_bg_color(btn_run, lv_color_hex(0x2a7da8), 0);
    lv_obj_set_style_radius(btn_run, 5, 0);
    lv_obj_add_event_cb(btn_run, run_btn_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *label_run = lv_label_create(btn_run);
    lv_label_set_text(label_run, "RUN " LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(label_run, lv_color_white(), 0);
    lv_obj_center(label_run);
}

/**
 * Destroy data viewer and return to file browser
 */
void data_viewer_destroy(void)
{
    // Free data
    if(uel_data != NULL)
    {
        free(uel_data);
        uel_data = NULL;
    }
    if(curr_pattern != NULL)
    {
        free(curr_pattern);
        curr_pattern = NULL;
    }
    if(meas_pattern != NULL)
    {
        free(meas_pattern);
        meas_pattern = NULL;
    }
    
    // Reset populated flags
    curr_populated = false;
    uel_populated = false;
    meas_populated = false;
    table_curr = NULL;
    table_uel = NULL;
    table_meas = NULL;
    
    // Clean screen
    lv_obj_clean(lv_screen_active());
    viewer_screen = NULL;
    
    // Recreate file browser
    sd_file_browser_create();
}

/**
 * Get loaded Uel data for reconstruction
 * Returns pointer to 2D array [n_meas][n_inj]
 */
float** data_viewer_get_uel(uint16_t *out_n_meas, uint16_t *out_n_inj)
{
    if (!uel_data) {
        return NULL;
    }
    
    *out_n_meas = n_meas;
    *out_n_inj = n_inj;
    
    // Allocate 2D array pointers
    float **uel_2d = (float**)malloc(n_meas * sizeof(float*));
    if (!uel_2d) {
        return NULL;
    }
    
    for (uint16_t i = 0; i < n_meas; i++) {
        uel_2d[i] = &uel_data[i * n_inj];
    }
    
    return uel_2d;
}
