#include "home_view.h"

#include <string.h>

/* ---- Colour palette (matches existing app theme) ---- */
#define COL_BG          0x0a0a0a
#define COL_ACCENT      0x4a9fd8
#define COL_BTN         0x2a7da8
#define COL_BTN_ABOUT   0x555555
#define COL_OVERLAY_BG  0x1a1a1a

/* ---- Callbacks ---- */
static void start_clicked(lv_event_t *e)
{
    home_view_t *v = (home_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_start)
        v->bindings.on_start(v->bindings.ctx);
}

static void settings_clicked(lv_event_t *e)
{
    home_view_t *v = (home_view_t *)lv_event_get_user_data(e);
    if (v && v->bindings.on_settings)
        v->bindings.on_settings(v->bindings.ctx);
}

static void about_close_clicked(lv_event_t *e)
{
    home_view_t *v = (home_view_t *)lv_event_get_user_data(e);
    if (!v || !v->about_overlay) return;
    lv_obj_add_flag(v->about_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void about_clicked(lv_event_t *e)
{
    home_view_t *v = (home_view_t *)lv_event_get_user_data(e);
    if (!v || !v->about_overlay) return;
    lv_obj_clear_flag(v->about_overlay, LV_OBJ_FLAG_HIDDEN);
}

/* ---- Helper: styled menu button ---- */
static lv_obj_t *make_menu_btn(lv_obj_t *parent, const char *text,
                               uint32_t color, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 260, 54);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ---- Build about-info overlay (hidden by default) ---- */
static lv_obj_t *build_about_overlay(lv_obj_t *parent, home_view_t *view)
{
    lv_obj_t *overlay = lv_obj_create(parent);
    lv_obj_set_size(overlay, 500, 300);
    lv_obj_center(overlay);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(COL_OVERLAY_BG), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(overlay, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_radius(overlay, 10, 0);
    lv_obj_set_style_pad_all(overlay, 20, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(overlay);
    lv_label_set_text(title, "About");
    lv_obj_set_style_text_color(title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *body = lv_label_create(overlay);
    lv_label_set_text(body,
        "EIT Firmware v1.0\n"
        "Electrical Impedance Tomography\n\n"
        "STM32F769I-DISCO + LVGL v9\n"
        "LBP Reconstruction Algorithm\n\n"
        "Developed by G. Kolotouros");
    lv_obj_set_style_text_color(body, lv_color_white(), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_14, 0);
    lv_obj_set_width(body, 440);
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 35);

    lv_obj_t *btn_close = lv_button_create(overlay);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_align(btn_close, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(btn_close, 6, 0);
    lv_obj_add_event_cb(btn_close, about_close_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl = lv_label_create(btn_close);
    lv_label_set_text(lbl, "CLOSE");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    return overlay;
}

/* ================================================================== */
void home_view_create(home_view_t *view, lv_obj_t *parent,
                      const home_view_bindings_t *bindings)
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
    lv_label_set_text(view->label_title, "EIT System");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 40);

    /* Subtitle */
    view->label_subtitle = lv_label_create(view->cont);
    lv_label_set_text(view->label_subtitle,
        "Electrical Impedance Tomography");
    lv_obj_set_style_text_color(view->label_subtitle, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(view->label_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_subtitle, LV_ALIGN_TOP_MID, 0, 72);

    /* Menu buttons — stacked vertically in the centre */
    view->btn_start = make_menu_btn(view->cont, LV_SYMBOL_PLAY "  START",
                                    COL_BTN, start_clicked, view);
    lv_obj_align(view->btn_start, LV_ALIGN_CENTER, 0, -50);

    view->btn_settings = make_menu_btn(view->cont, LV_SYMBOL_SETTINGS "  SETTINGS",
                                       COL_BTN, settings_clicked, view);
    lv_obj_align(view->btn_settings, LV_ALIGN_CENTER, 0, 20);

    view->btn_about = make_menu_btn(view->cont, LV_SYMBOL_LIST "  ABOUT",
                                    COL_BTN_ABOUT, about_clicked, view);
    lv_obj_align(view->btn_about, LV_ALIGN_CENTER, 0, 90);

    /* About overlay (hidden) */
    view->about_overlay = build_about_overlay(view->cont, view);
}
