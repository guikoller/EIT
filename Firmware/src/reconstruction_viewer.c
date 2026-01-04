#include "reconstruction_viewer.h"
#include "data_viewer.h"
#include "sd_file_browser.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Reconstruction parameters
#define RECON_SIZE 32
#define DISPLAY_SIZE 288  // 32x9 upscaling (288x288 = 165KB buffer)

static lv_obj_t *recon_screen = NULL;
static lv_obj_t *canvas = NULL;
static lv_obj_t *label_title = NULL;
static lv_obj_t *label_status = NULL;
static char current_filename[128];

/**
 * Return button click handler
 */
static void return_btn_clicked(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if(code == LV_EVENT_CLICKED)
    {
        lv_obj_clean(lv_screen_active());
        sd_file_browser_create();
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
    
    // Fill with dark background initially
    lv_canvas_fill_bg(canvas, lv_color_hex(0x1a1a1a), LV_OPA_COVER);
    
    // Draw circular boundary (placeholder)
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = lv_color_hex(0x0a0a0a);
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.radius = DISPLAY_SIZE / 2;
    rect_dsc.border_width = 2;
    rect_dsc.border_color = lv_color_white();
    
    lv_area_t area;
    area.x1 = 0;
    area.y1 = 0;
    area.x2 = DISPLAY_SIZE - 1;
    area.y2 = DISPLAY_SIZE - 1;
    
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);
    lv_draw_rect(&layer, &rect_dsc, &area);
    lv_canvas_finish_layer(canvas, &layer);
    
    // Draw electrode dots on circumference
    lv_draw_arc_dsc_t dot_dsc;
    lv_draw_arc_dsc_init(&dot_dsc);
    dot_dsc.color = lv_color_white();
    dot_dsc.width = 10;  // Thickness of the dot
    dot_dsc.rounded = 1;
    
    int center = DISPLAY_SIZE / 2;
    int electrode_radius = DISPLAY_SIZE / 2 - 1;  // On the border
    
    for(int i = 0; i < 16; i++)
    {
        float angle = (i * 2.0f * 3.14159f) / 16.0f;
        int x = center + (int)(electrode_radius * cosf(angle));
        int y = center - (int)(electrode_radius * sinf(angle));
        
        // Draw filled circle as electrode marker
        lv_draw_rect_dsc_t electrode_dsc;
        lv_draw_rect_dsc_init(&electrode_dsc);
        electrode_dsc.bg_color = lv_color_white();
        electrode_dsc.bg_opa = LV_OPA_COVER;
        electrode_dsc.radius = LV_RADIUS_CIRCLE;
        electrode_dsc.border_width = 0;
        
        lv_area_t dot_area;
        dot_area.x1 = x - 5;
        dot_area.y1 = y - 5;
        dot_area.x2 = x + 5;
        dot_area.y2 = y + 5;
        
        lv_canvas_init_layer(canvas, &layer);
        lv_draw_rect(&layer, &electrode_dsc, &dot_area);
        lv_canvas_finish_layer(canvas, &layer);
    }
}

/**
 * Add electrode markers as labels on top of canvas
 */
static void add_electrode_markers(lv_obj_t *parent)
{
    int center_x = DISPLAY_SIZE / 2;
    int center_y = DISPLAY_SIZE / 2;
    int radius = DISPLAY_SIZE / 2 + 20;  // Place outside the circle
    
    for(int i = 0; i < 16; i++)
    {
        float angle = (i * 2.0f * 3.14159f) / 16.0f;
        int x = center_x + (int)(radius * cosf(angle));
        int y = center_y - (int)(radius * sinf(angle));
        
        // Create label for electrode number
        lv_obj_t *label = lv_label_create(parent);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        lv_label_set_text(label, num);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        
        // Position relative to canvas position
        lv_obj_align_to(label, canvas, LV_ALIGN_TOP_LEFT, x - 8, y - 8);
    }
}

/**
 * Render reconstruction image on canvas
 */
static void render_reconstruction(void)
{
    // TODO: Implement actual rendering with upscaling
    // For now, just update status
    lv_label_set_text(label_status, "Ready for reconstruction");
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
    
    // Title with filename
    label_title = lv_label_create(recon_screen);
    char title_text[256];
    snprintf(title_text, sizeof(title_text), "EIT Reconstruction - %s", filename);
    lv_label_set_text(label_title, title_text);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Status label
    label_status = lv_label_create(recon_screen);
    lv_label_set_text(label_status, "Processing reconstruction...");
    lv_obj_set_style_text_color(label_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_14, 0);
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 40);
    
    // Create canvas for image display
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
    
    // Initialize reconstruction (placeholder)
    render_reconstruction();
}

/**
 * Destroy reconstruction viewer and return to data viewer
 */
void reconstruction_viewer_destroy(void)
{
    lv_obj_clean(lv_screen_active());
    data_viewer_create(current_filename);
}
