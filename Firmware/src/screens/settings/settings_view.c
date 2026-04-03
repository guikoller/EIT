#include "settings_view.h"

#include <string.h>
#include <stdio.h>

/* ---- Light theme color palette ---- */
#define COL_BG          0xf5f5f5
#define COL_ACCENT      0x4a9fd8
#define COL_CARD        0xffffff
#define COL_CARD_BORDER 0xe0e0e0
#define COL_BTN         0x2a7da8
#define COL_LABEL       0x666666
#define COL_TEXT        0x333333
#define COL_DROPDOWN_BG 0xffffff
#define COL_DROPDOWN_BD 0xe0e0e0

/* ---- Image-size option list (must match index_to_size / size_to_index) ---- */
static const char *IMAGE_SIZE_OPTIONS = "16\n32\n64";

static uint16_t index_to_size(uint16_t idx)
{
    switch (idx) {
        case 0: return 16;
        case 1: return 32;
        case 2: return 64;
        default: return 32;
    }
}

static uint16_t size_to_index(uint16_t sz)
{
    switch (sz) {
        case 16: return 0;
        case 32: return 1;
        case 64: return 2;
        default: return 1;
    }
}

/* ---- Widget callbacks ---- */
static void nav_home_clicked(void *ctx)
{
    settings_view_t *v = (settings_view_t *)ctx;
    if (v && v->bindings.on_nav_home)
        v->bindings.on_nav_home(v->bindings.ctx);
}

static void nav_eit_clicked(void *ctx)
{
    settings_view_t *v = (settings_view_t *)ctx;
    if (v && v->bindings.on_nav_eit)
        v->bindings.on_nav_eit(v->bindings.ctx);
}

static void nav_settings_clicked(void *ctx)
{
    settings_view_t *v = (settings_view_t *)ctx;
    if (v && v->bindings.on_nav_settings)
        v->bindings.on_nav_settings(v->bindings.ctx);
}

static void algo_changed(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (!v) return;
    uint16_t sel = lv_dropdown_get_selected(v->dd_algorithm);
    if (v->bindings.on_algorithm)
        v->bindings.on_algorithm(v->bindings.ctx, (eit_algorithm_t)sel);
}

static void imgsize_changed(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (!v) return;
    uint16_t sel = lv_dropdown_get_selected(v->dd_image_size);
    uint16_t sz = index_to_size(sel);
    if (v->bindings.on_image_size)
        v->bindings.on_image_size(v->bindings.ctx, sz);
}

static void datatable_changed(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (!v) return;
    uint8_t checked = lv_obj_has_state(v->sw_data_table, LV_STATE_CHECKED) ? 1u : 0u;
    if (v->bindings.on_show_data_table)
        v->bindings.on_show_data_table(v->bindings.ctx, checked);
}

static void calibrate_clicked(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_calibrate)
        v->bindings.on_calibrate(v->bindings.ctx);
}

static void batch_clicked(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_batch)
        v->bindings.on_batch(v->bindings.ctx);
}

static void wifi_clicked(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_wifi)
        v->bindings.on_wifi(v->bindings.ctx);
}

/* ---- Helper: section label ---- */
static lv_obj_t *make_section_label(lv_obj_t *parent, const char *text,
                                    lv_align_t align, int x, int y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_align(lbl, align, x, y);
    return lbl;
}

/* ================================================================== */
void settings_view_create(settings_view_t *view, lv_obj_t *parent,
                          const settings_view_bindings_t *bindings)
{
    if (!view || !parent) return;
    memset(view, 0, sizeof(*view));
    if (bindings) view->bindings = *bindings;

    /* Full-screen container - light background */
    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(view->cont);

    left_menu_bindings_t menu_bindings;
    memset(&menu_bindings, 0, sizeof(menu_bindings));
    menu_bindings.ctx = view;
    menu_bindings.on_home = nav_home_clicked;
    menu_bindings.on_eit = nav_eit_clicked;
    menu_bindings.on_settings = nav_settings_clicked;
    left_menu_create(&view->menu, view->cont, LEFT_MENU_ITEM_SETTINGS, &menu_bindings);

    view->content = lv_obj_create(view->cont);
    lv_obj_set_size(view->content, left_menu_content_width(), LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, left_menu_content_x(), 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 20);

    /* ---- Card area ---- */
    lv_obj_t *card = lv_obj_create(view->content);
    lv_obj_set_size(card, left_menu_content_width() - 34, 390);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_CARD_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_shadow_width(card, 8, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_shadow_ofs_y(card, 4, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 12, 0);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- Row 1: Algorithm ---- */
    lv_obj_t *row1 = lv_obj_create(card);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row1, 50, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);

    make_section_label(row1, "Algorithm", LV_ALIGN_LEFT_MID, 0, 0);

    view->dd_algorithm = lv_dropdown_create(row1);
    lv_dropdown_set_options(view->dd_algorithm, "LBP");
    lv_obj_set_size(view->dd_algorithm, 270, 50);
    lv_obj_align(view->dd_algorithm, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->dd_algorithm, lv_color_hex(COL_DROPDOWN_BG), 0);
    lv_obj_set_style_border_color(view->dd_algorithm, lv_color_hex(COL_DROPDOWN_BD), 0);
    lv_obj_set_style_border_width(view->dd_algorithm, 1, 0);
    lv_obj_set_style_text_color(view->dd_algorithm, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(view->dd_algorithm, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(view->dd_algorithm, algo_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 2: Image Size ---- */
    lv_obj_t *row2 = lv_obj_create(card);
    lv_obj_set_size(row2, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row2, 50, 0);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);

    make_section_label(row2, "Image Size", LV_ALIGN_LEFT_MID, 0, 0);

    view->dd_image_size = lv_dropdown_create(row2);
    lv_dropdown_set_options(view->dd_image_size, IMAGE_SIZE_OPTIONS);
    lv_obj_set_size(view->dd_image_size, 270, 50);
    lv_obj_align(view->dd_image_size, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->dd_image_size, lv_color_hex(COL_DROPDOWN_BG), 0);
    lv_obj_set_style_border_color(view->dd_image_size, lv_color_hex(COL_DROPDOWN_BD), 0);
    lv_obj_set_style_border_width(view->dd_image_size, 1, 0);
    lv_obj_set_style_text_color(view->dd_image_size, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(view->dd_image_size, &lv_font_montserrat_14, 0);
    lv_obj_add_event_cb(view->dd_image_size, imgsize_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 3: Show Data Table ---- */
    lv_obj_t *row3 = lv_obj_create(card);
    lv_obj_set_size(row3, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row3, 50, 0);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);

    make_section_label(row3, "Show Data Table", LV_ALIGN_LEFT_MID, 0, 0);

    view->sw_data_table = lv_switch_create(row3);
    lv_obj_set_size(view->sw_data_table, 72, 38);
    lv_obj_align(view->sw_data_table, LV_ALIGN_RIGHT_MID, 0, 0);
    /* Switch styling - off state */
    lv_obj_set_style_bg_color(view->sw_data_table, lv_color_hex(COL_DROPDOWN_BG), LV_PART_MAIN);
    lv_obj_set_style_border_color(view->sw_data_table, lv_color_hex(COL_DROPDOWN_BD), LV_PART_MAIN);
    lv_obj_set_style_border_width(view->sw_data_table, 1, LV_PART_MAIN);
    /* Switch styling - on state */
    lv_obj_set_style_bg_color(view->sw_data_table, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    /* Knob styling */
    lv_obj_set_style_bg_color(view->sw_data_table, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_shadow_width(view->sw_data_table, 2, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(view->sw_data_table, lv_color_hex(0xcccccc), LV_PART_KNOB);
    lv_obj_add_event_cb(view->sw_data_table, datatable_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 4: Calibrate ---- */
    lv_obj_t *row4 = lv_obj_create(card);
    lv_obj_set_size(row4, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row4, 50, 0);
    lv_obj_set_style_bg_opa(row4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row4, 0, 0);
    lv_obj_set_style_pad_all(row4, 0, 0);

    make_section_label(row4, "Calibrate", LV_ALIGN_LEFT_MID, 0, 0);

    view->btn_calibrate = lv_button_create(row4);
    lv_obj_set_size(view->btn_calibrate, 270, 50);
    lv_obj_align(view->btn_calibrate, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->btn_calibrate, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_calibrate, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_calibrate, 0, 0);
    lv_obj_set_style_shadow_color(view->btn_calibrate, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_calibrate, 0, 0);
    lv_obj_add_event_cb(view->btn_calibrate, calibrate_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *cal_lbl = lv_label_create(view->btn_calibrate);
    lv_label_set_text(cal_lbl, LV_SYMBOL_REFRESH "  CALIBRATE");
    lv_obj_set_style_text_color(cal_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(cal_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(cal_lbl);

    /* ---- Row 5: Batch Process ---- */
    lv_obj_t *row5 = lv_obj_create(card);
    lv_obj_set_size(row5, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row5, 50, 0);
    lv_obj_set_style_bg_opa(row5, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row5, 0, 0);
    lv_obj_set_style_pad_all(row5, 0, 0);

    make_section_label(row5, "Batch Process", LV_ALIGN_LEFT_MID, 0, 0);

    view->btn_batch = lv_button_create(row5);
    lv_obj_set_size(view->btn_batch, 270, 50);
    lv_obj_align(view->btn_batch, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->btn_batch, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_batch, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_batch, 0, 0);
    lv_obj_set_style_shadow_color(view->btn_batch, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_batch, 0, 0);
    lv_obj_add_event_cb(view->btn_batch, batch_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *batch_lbl = lv_label_create(view->btn_batch);
    lv_label_set_text(batch_lbl, LV_SYMBOL_DOWNLOAD "  BATCH PROCESS");
    lv_obj_set_style_text_color(batch_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(batch_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(batch_lbl);

    /* ---- Row 6: WiFi ---- */
    lv_obj_t *row6 = lv_obj_create(card);
    lv_obj_set_size(row6, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row6, 50, 0);
    lv_obj_set_style_bg_opa(row6, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row6, 0, 0);
    lv_obj_set_style_pad_all(row6, 0, 0);

    make_section_label(row6, "WiFi", LV_ALIGN_LEFT_MID, 0, 0);

    view->btn_wifi = lv_button_create(row6);
    lv_obj_set_size(view->btn_wifi, 270, 50);
    lv_obj_align(view->btn_wifi, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->btn_wifi, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_wifi, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_wifi, 0, 0);
    lv_obj_set_style_shadow_color(view->btn_wifi, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_wifi, 0, 0);
    lv_obj_add_event_cb(view->btn_wifi, wifi_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *wifi_lbl = lv_label_create(view->btn_wifi);
    lv_label_set_text(wifi_lbl, LV_SYMBOL_WIFI "  OPEN WIFI");
    lv_obj_set_style_text_color(wifi_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(wifi_lbl);

    /* Calibration status label */
    view->label_calib_status = lv_label_create(view->content);
    lv_label_set_text(view->label_calib_status, "");
    lv_obj_set_style_text_color(view->label_calib_status, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_calib_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_calib_status, LV_ALIGN_TOP_MID, 0, 452);

    /* Batch status label */
    view->label_batch_status = lv_label_create(view->content);
    lv_label_set_text(view->label_batch_status, "");
    lv_obj_set_style_text_color(view->label_batch_status, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_batch_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_batch_status, LV_ALIGN_TOP_MID, 0, 430);
    view->btn_back = NULL;
}

void settings_view_set_values(settings_view_t *view,
                              eit_algorithm_t algo,
                              uint16_t image_size,
                              uint8_t show_data_table)
{
    if (!view) return;

    if (view->dd_algorithm)
        lv_dropdown_set_selected(view->dd_algorithm, (uint16_t)algo);

    if (view->dd_image_size)
        lv_dropdown_set_selected(view->dd_image_size, size_to_index(image_size));

    if (view->sw_data_table) {
        if (show_data_table)
            lv_obj_add_state(view->sw_data_table, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(view->sw_data_table, LV_STATE_CHECKED);
    }
}

void settings_view_set_calib_status(settings_view_t *view, const char *text)
{
    if (!view || !view->label_calib_status) return;
    lv_label_set_text(view->label_calib_status, text ? text : "");
}

void settings_view_set_calib_enabled(settings_view_t *view, int enabled)
{
    if (!view || !view->btn_calibrate) return;
    if (enabled)
        lv_obj_clear_state(view->btn_calibrate, LV_STATE_DISABLED);
    else
        lv_obj_add_state(view->btn_calibrate, LV_STATE_DISABLED);
}

void settings_view_set_batch_status(settings_view_t *view, const char *text)
{
    if (!view || !view->label_batch_status) return;
    lv_label_set_text(view->label_batch_status, text ? text : "");
}

void settings_view_set_batch_enabled(settings_view_t *view, int enabled)
{
    if (!view || !view->btn_batch) return;
    if (enabled)
        lv_obj_clear_state(view->btn_batch, LV_STATE_DISABLED);
    else
        lv_obj_add_state(view->btn_batch, LV_STATE_DISABLED);
}
