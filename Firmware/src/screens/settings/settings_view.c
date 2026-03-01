#include "settings_view.h"

#include <string.h>
#include <stdio.h>

/* ---- Colour palette ---- */
#define COL_BG          0x0a0a0a
#define COL_ACCENT      0x4a9fd8
#define COL_CARD        0x1a1a1a
#define COL_BTN         0x2a7da8
#define COL_LABEL       0x888888

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
static void back_clicked(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_back)
        v->bindings.on_back(v->bindings.ctx);
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
    uint8_t checked = lv_obj_has_state(v->cb_data_table, LV_STATE_CHECKED) ? 1u : 0u;
    if (v->bindings.on_show_data_table)
        v->bindings.on_show_data_table(v->bindings.ctx, checked);
}

static void calibrate_clicked(lv_event_t *e)
{
    settings_view_t *v = (settings_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_calibrate)
        v->bindings.on_calibrate(v->bindings.ctx);
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

    /* Full-screen container */
    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_center(view->cont);

    /* Title */
    view->label_title = lv_label_create(view->cont);
    lv_label_set_text(view->label_title, LV_SYMBOL_SETTINGS "  SETTINGS");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 20);

    /* ---- Card area ---- */
    lv_obj_t *card = lv_obj_create(view->cont);
    lv_obj_set_size(card, 700, 400);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 25, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 20, 0);

    /* ---- Row 1: Algorithm ---- */
    lv_obj_t *row1 = lv_obj_create(card);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row1, 60, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);

    make_section_label(row1, "Algorithm", LV_ALIGN_LEFT_MID, 0, 0);

    view->dd_algorithm = lv_dropdown_create(row1);
    lv_dropdown_set_options(view->dd_algorithm, "LBP");
    lv_obj_set_size(view->dd_algorithm, 280, 50);
    lv_obj_align(view->dd_algorithm, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->dd_algorithm, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(view->dd_algorithm, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->dd_algorithm, &lv_font_montserrat_22, 0);
    lv_obj_add_event_cb(view->dd_algorithm, algo_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 2: Image Size ---- */
    lv_obj_t *row2 = lv_obj_create(card);
    lv_obj_set_size(row2, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row2, 60, 0);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);

    make_section_label(row2, "Image Size", LV_ALIGN_LEFT_MID, 0, 0);

    view->dd_image_size = lv_dropdown_create(row2);
    lv_dropdown_set_options(view->dd_image_size, IMAGE_SIZE_OPTIONS);
    lv_obj_set_size(view->dd_image_size, 280, 50);
    lv_obj_align(view->dd_image_size, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->dd_image_size, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_color(view->dd_image_size, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->dd_image_size, &lv_font_montserrat_22, 0);
    lv_obj_add_event_cb(view->dd_image_size, imgsize_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 3: Show Data Table ---- */
    lv_obj_t *row3 = lv_obj_create(card);
    lv_obj_set_size(row3, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row3, 60, 0);
    lv_obj_set_style_bg_opa(row3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row3, 0, 0);
    lv_obj_set_style_pad_all(row3, 0, 0);

    make_section_label(row3, "Show Data Table", LV_ALIGN_LEFT_MID, 0, 0);

    view->cb_data_table = lv_checkbox_create(row3);
    lv_checkbox_set_text(view->cb_data_table, "");
    lv_obj_align(view->cb_data_table, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_pad_column(view->cb_data_table, 10, 0);
    lv_obj_set_size(view->cb_data_table, 50, 50);
    lv_obj_set_style_bg_color(view->cb_data_table, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_min_width(view->cb_data_table, 40, LV_PART_INDICATOR);
    lv_obj_set_style_min_height(view->cb_data_table, 40, LV_PART_INDICATOR);
    lv_obj_add_event_cb(view->cb_data_table, datatable_changed, LV_EVENT_VALUE_CHANGED, view);

    /* ---- Row 4: Calibrate ---- */
    lv_obj_t *row4 = lv_obj_create(card);
    lv_obj_set_size(row4, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row4, 60, 0);
    lv_obj_set_style_bg_opa(row4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row4, 0, 0);
    lv_obj_set_style_pad_all(row4, 0, 0);

    make_section_label(row4, "Calibrate", LV_ALIGN_LEFT_MID, 0, 0);

    view->btn_calibrate = lv_button_create(row4);
    lv_obj_set_size(view->btn_calibrate, 280, 50);
    lv_obj_align(view->btn_calibrate, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(view->btn_calibrate, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_calibrate, 8, 0);
    lv_obj_add_event_cb(view->btn_calibrate, calibrate_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *cal_lbl = lv_label_create(view->btn_calibrate);
    lv_label_set_text(cal_lbl, LV_SYMBOL_REFRESH "  CALIBRATE");
    lv_obj_set_style_text_color(cal_lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(cal_lbl, &lv_font_montserrat_22, 0);
    lv_obj_center(cal_lbl);

    /* Calibration status label (below card, above back button) */
    view->label_calib_status = lv_label_create(view->cont);
    lv_label_set_text(view->label_calib_status, "");
    lv_obj_set_style_text_color(view->label_calib_status, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_calib_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_calib_status, LV_ALIGN_BOTTOM_MID, 0, -72);

    /* ---- Back button ---- */
    view->btn_back = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_back, 200, 56);
    lv_obj_align(view->btn_back, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_bg_color(view->btn_back, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_back, 8, 0);
    lv_obj_add_event_cb(view->btn_back, back_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl = lv_label_create(view->btn_back);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT "  BACK");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl);
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

    if (view->cb_data_table) {
        if (show_data_table)
            lv_obj_add_state(view->cb_data_table, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(view->cb_data_table, LV_STATE_CHECKED);
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
