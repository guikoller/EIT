#include "sd_file_browser.h"
#include "data_viewer.h"
#include "stm32f769i_discovery_sd.h"
#include "ff.h"
#include <string.h>
#include <stdio.h>

static lv_obj_t *file_list;
static lv_obj_t *btn_load;
static lv_obj_t *label_title;
static lv_obj_t *selected_file_obj = NULL;
static char selected_filename[64] = "";
static FATFS SDFatFs;
static uint8_t fs_mounted = 0;

#define MAX_FILES 50

typedef struct {
    char filename[64];
    uint32_t size;
    bool is_valid;
} FileEntry;

static FileEntry file_entries[MAX_FILES];
static int file_count = 0;

static char pending_open_filename[64] = "";

static void open_data_viewer_async_cb(void * user_data)
{
    (void)user_data;

    if (pending_open_filename[0] == '\0') return;
    data_viewer_create(pending_open_filename);
}

/**
 * FatFS get_fattime implementation
 * Returns current time for file timestamps (dummy implementation)
 */
DWORD get_fattime(void)
{
    // Return fixed date: 2026-01-04 12:00:00
    return ((2026 - 1980) << 25) | (1 << 21) | (4 << 16) | (12 << 11) | (0 << 5) | (0 >> 1);
}

/**
 * Initialize SD card
 */
int sd_card_init(void)
{
    uint8_t sd_state = BSP_SD_Init();
    
    if(sd_state != MSD_OK)
    {
        return -1;
    }
    
    // Try to mount the file system
    FRESULT res = f_mount(&SDFatFs, "0:", 1);
    if(res == FR_OK)
    {
        fs_mounted = 1;
    }
    else
    {
        fs_mounted = 0;
    }
    
    return 0;
}

/**
 * Scan SD card for .bin and .csv files
 */
static void scan_files(void)
{
    DIR dir;
    FILINFO fno;
    FRESULT res;
    
    file_count = 0;
    memset(file_entries, 0, sizeof(file_entries));
    
    if(!fs_mounted)
    {
        return;
    }
    
    res = f_opendir(&dir, "0:");
    if(res != FR_OK)
    {
        return;
    }
    
    while(file_count < MAX_FILES)
    {
        res = f_readdir(&dir, &fno);
        if(res != FR_OK || fno.fname[0] == 0) break;
        
        if(fno.fattrib & AM_DIR) continue;
        
        // Filter .bin files
        size_t len = strlen(fno.fname);
        if(len > 4 && strcmp(&fno.fname[len - 4], ".bin") == 0)
        {
            strncpy(file_entries[file_count].filename, fno.fname, 
                    sizeof(file_entries[file_count].filename) - 1);
            file_entries[file_count].size = fno.fsize;
            file_entries[file_count].is_valid = true;
            file_count++;
        }
    }
    
    f_closedir(&dir);
}

/**
 * File list item click handler
 */
static void file_item_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        lv_obj_t *btn = lv_event_get_target(e);
        
        // Deselect previous item
        if(selected_file_obj != NULL)
        {
            lv_obj_set_style_bg_color(selected_file_obj, lv_color_hex(0x2a2a2a), 0);
        }
        
        // Select new item
        selected_file_obj = btn;
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a5f7a), 0);
        
        // Get filename from label (child 1, after icon)
        lv_obj_t *label = lv_obj_get_child(btn, 1);
        if(label == NULL) return;
        const char *text = lv_label_get_text(label);
        
        // Extract just the filename (before |)
        char *pipe = strchr(text, '|');
        if(pipe)
        {
            size_t len = pipe - text - 1; // -1 for space before |
            if(len < sizeof(selected_filename))
            {
                strncpy(selected_filename, text, len);
                selected_filename[len] = '\0';
            }
        }
    }
}

/**
 * Load button click handler
 */
static void load_btn_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        if(strlen(selected_filename) > 0)
        {
            /* Don't create/clean screens inside the event call stack */
            strncpy(pending_open_filename, selected_filename, sizeof(pending_open_filename) - 1);
            pending_open_filename[sizeof(pending_open_filename) - 1] = '\0';
            lv_async_call(open_data_viewer_async_cb, NULL);
        }
    }
}

/**
 * Format file size to human readable
 */
static void format_size(uint32_t bytes, char *buf, size_t buf_size)
{
    if(bytes >= 1024 * 1024)
    {
        snprintf(buf, buf_size, "%luMB", bytes / (1024 * 1024));
    }
    else if(bytes >= 1024)
    {
        snprintf(buf, buf_size, "%luKB", bytes / 1024);
    }
    else
    {
        snprintf(buf, buf_size, "%lub", bytes);
    }
}

/**
 * Populate file list
 */
static void populate_file_list(void)
{
    // Clear selection
    selected_file_obj = NULL;
    selected_filename[0] = '\0';
    
    // Remove all children safely
    uint32_t child_cnt = lv_obj_get_child_count(file_list);
    for(uint32_t i = child_cnt; i > 0; i--)
    {
        lv_obj_t *child = lv_obj_get_child(file_list, i - 1);
        lv_obj_delete(child);
    }
    
    for(int i = 0; i < file_count; i++)
    {
        if(!file_entries[i].is_valid) continue;
        
        // Format: "filename | size"
        char size_str[16];
        format_size(file_entries[i].size, size_str, sizeof(size_str));
        
        char label_text[128];
        snprintf(label_text, sizeof(label_text), "%s | %s", 
                 file_entries[i].filename, size_str);
        
        // Create list button with text
        lv_obj_t *btn = lv_list_add_button(file_list, LV_SYMBOL_FILE, label_text);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 18, 0);
        lv_obj_set_style_min_height(btn, 60, 0);
        lv_obj_add_event_cb(btn, file_item_clicked, LV_EVENT_CLICKED, NULL);
        
        // Style the icon and label
        uint32_t child_count = lv_obj_get_child_count(btn);
        if(child_count > 0)
        {
            // Make icon white (child 0)
            lv_obj_t *icon = lv_obj_get_child(btn, 0);
            lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        }
        if(child_count > 1)
        {
            // Make label white (child 1)
            lv_obj_t *label = lv_obj_get_child(btn, 1);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
        }
    }
}

/**
 * Create SD card file browser UI
 */
void sd_file_browser_create(void)
{
    // Create main container
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_center(cont);
    
    // Title label
    label_title = lv_label_create(cont);
    lv_label_set_text(label_title, "SD STORAGE");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_LEFT, 20, 15);
    
    // File list
    file_list = lv_list_create(cont);
    lv_obj_set_size(file_list, LV_HOR_RES - 60, LV_VER_RES - 150);
    lv_obj_align(file_list, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(file_list, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_color(file_list, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(file_list, 2, 0);
    lv_obj_set_style_radius(file_list, 5, 0);
    
    // Scan and populate files
    scan_files();
    populate_file_list();
    
    // Load button
    btn_load = lv_button_create(cont);
    lv_obj_set_size(btn_load, 180, 50);
    lv_obj_align(btn_load, LV_ALIGN_BOTTOM_RIGHT, -25, -20);
    lv_obj_set_style_bg_color(btn_load, lv_color_hex(0x2a7da8), 0);
    lv_obj_set_style_radius(btn_load, 5, 0);
    lv_obj_add_event_cb(btn_load, load_btn_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t *btn_label = lv_label_create(btn_load);
    lv_label_set_text(btn_label, "LOAD DATASET");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_center(btn_label);
}
