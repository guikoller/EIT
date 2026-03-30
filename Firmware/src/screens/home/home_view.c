#include "home_view.h"
#include "assets/time_logo.h"

#include <string.h>

/* ---- Colour palette (light theme) ---- */
#define COL_BG          0xf5f5f5
#define COL_ACCENT      0x4a9fd8
#define COL_BTN         0x2a7da8

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

/* ---- Helper: styled menu button ---- */
static lv_obj_t *make_menu_btn(lv_obj_t *parent, const char *text,
                               lv_event_cb_t cb, void *ud)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 280, 60);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 4, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ================================================================== */
void home_view_create(home_view_t *view, lv_obj_t *parent,
                      const home_view_bindings_t *bindings)
{
    if (!view || !parent) return;
    memset(view, 0, sizeof(*view));

    if (bindings) view->bindings = *bindings;

    /* Full-screen container - 800x480 */
    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_center(view->cont);

    /* Project logo - positioned at top */
    view->img_logo = lv_image_create(view->cont);
    lv_image_set_src(view->img_logo, &time_logo);
    lv_image_set_scale(view->img_logo, 380);
    lv_obj_align(view->img_logo, LV_ALIGN_TOP_MID, 0, 25);

    /* Subtitle label */
    view->label_subtitle = lv_label_create(view->cont);
    lv_label_set_text(view->label_subtitle, "Electrical Impedance Tomography");
    lv_obj_set_style_text_color(view->label_subtitle, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(view->label_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_subtitle, LV_ALIGN_TOP_MID, 0, 185);

    view->label_title = NULL;

    /* Menu buttons — centered vertically with proper spacing for 480px height */
    view->btn_start = make_menu_btn(view->cont, LV_SYMBOL_PLAY "  START",
                                    start_clicked, view);
    lv_obj_align(view->btn_start, LV_ALIGN_CENTER, 0, 70);

    view->btn_settings = make_menu_btn(view->cont, LV_SYMBOL_SETTINGS "  SETTINGS",
                                       settings_clicked, view);
    lv_obj_align(view->btn_settings, LV_ALIGN_CENTER, 0, 150);

    /* Removed about button and overlay */
    view->btn_about = NULL;
    view->about_overlay = NULL;
}
