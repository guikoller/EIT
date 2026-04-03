#include "wifi_settings_view.h"

#include <string.h>

#define COL_BG       0xf5f5f5
#define COL_CARD     0xffffff
#define COL_BORDER   0xe0e0e0
#define COL_TEXT     0x333333
#define COL_LABEL    0x666666
#define COL_ACCENT   0x4a9fd8
#define COL_BTN      0x2a7da8
#define COL_BTN_SEC  0x888888

static void nav_home_clicked(void *ctx)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)ctx;
    if (v && v->bindings.on_nav_home) {
        v->bindings.on_nav_home(v->bindings.ctx);
    }
}

static void nav_eit_clicked(void *ctx)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)ctx;
    if (v && v->bindings.on_nav_eit) {
        v->bindings.on_nav_eit(v->bindings.ctx);
    }
}

static void nav_settings_clicked(void *ctx)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)ctx;
    if (v && v->bindings.on_nav_settings) {
        v->bindings.on_nav_settings(v->bindings.ctx);
    }
}

static void nav_return_clicked(void *ctx)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)ctx;
    if (v && v->bindings.on_back) {
        v->bindings.on_back(v->bindings.ctx);
    }
}

static void save_clicked(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_save) {
        return;
    }

    v->bindings.on_save(v->bindings.ctx,
                        lv_textarea_get_text(v->ta_ssid),
                        lv_textarea_get_text(v->ta_password),
                        lv_textarea_get_text(v->ta_server));
}

static void connect_clicked(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_connect) {
        return;
    }

    v->bindings.on_connect(v->bindings.ctx,
                           lv_textarea_get_text(v->ta_ssid),
                           lv_textarea_get_text(v->ta_password),
                           lv_textarea_get_text(v->ta_server));
}

static void textarea_focus_cb(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->keyboard) {
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(v->keyboard, ta);
    lv_obj_clear_flag(v->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_event_cb(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->keyboard) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(v->keyboard, NULL);
        lv_obj_add_flag(v->keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    return lbl;
}

static lv_obj_t *make_field(lv_obj_t *parent, const char *placeholder, int password)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, 520, 48);
    lv_textarea_set_one_line(ta, 1);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_password_mode(ta, password ? 1 : 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_22, 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 8, 0);
    return ta;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, uint32_t color)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 190, 56);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl);

    return btn;
}

void wifi_settings_view_create(wifi_settings_view_t *view,
                               lv_obj_t *parent,
                               const wifi_settings_view_bindings_t *bindings)
{
    if (!view || !parent) {
        return;
    }

    memset(view, 0, sizeof(*view));
    if (bindings) {
        view->bindings = *bindings;
    }

    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);

    left_menu_bindings_t menu_bindings;
    memset(&menu_bindings, 0, sizeof(menu_bindings));
    menu_bindings.ctx = view;
    menu_bindings.on_home = nav_home_clicked;
    menu_bindings.on_eit = nav_eit_clicked;
    menu_bindings.on_settings = nav_settings_clicked;
    menu_bindings.on_return = nav_return_clicked;
    left_menu_create(&view->menu, view->cont, LEFT_MENU_ITEM_SETTINGS, &menu_bindings);

    view->content = lv_obj_create(view->cont);
    lv_obj_set_size(view->content, left_menu_content_width(), LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, left_menu_content_x(), 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, LV_SYMBOL_WIFI "  WIFI SETTINGS");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 15);

    lv_obj_t *card = lv_obj_create(view->content);
    lv_obj_set_size(card, left_menu_content_width() - 34, 288);
    lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_color(card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    lv_obj_t *l1 = make_label(card, "SSID");
    (void)l1;
    view->ta_ssid = make_field(card, "Wi-Fi network name", 0);

    lv_obj_t *l2 = make_label(card, "Password");
    (void)l2;
    view->ta_password = make_field(card, "Wi-Fi password", 1);

    lv_obj_t *l3 = make_label(card, "Server URL");
    (void)l3;
    view->ta_server = make_field(card, "http://192.168.0.10:8080/eit", 0);

    lv_obj_add_event_cb(view->ta_ssid, textarea_focus_cb, LV_EVENT_FOCUSED, view);
    lv_obj_add_event_cb(view->ta_password, textarea_focus_cb, LV_EVENT_FOCUSED, view);
    lv_obj_add_event_cb(view->ta_server, textarea_focus_cb, LV_EVENT_FOCUSED, view);

    view->label_status = lv_label_create(view->content);
    lv_label_set_text(view->label_status, "");
    lv_obj_set_style_text_color(view->label_status, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_status, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_status, LV_ALIGN_TOP_MID, 0, 352);

    view->btn_back = NULL;

    view->btn_save = make_button(view->content, "SAVE", COL_BTN);
    lv_obj_align(view->btn_save, LV_ALIGN_BOTTOM_MID, -108, -16);
    lv_obj_add_event_cb(view->btn_save, save_clicked, LV_EVENT_CLICKED, view);

    view->btn_connect = make_button(view->content, "CONNECT", 0x2e7d32);
    lv_obj_align(view->btn_connect, LV_ALIGN_BOTTOM_MID, 108, -16);
    lv_obj_add_event_cb(view->btn_connect, connect_clicked, LV_EVENT_CLICKED, view);

    view->keyboard = lv_keyboard_create(view->content);
    lv_obj_set_size(view->keyboard, left_menu_content_width() - 6, 230);
    lv_obj_align(view->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(view->keyboard, &lv_font_montserrat_22, 0);
    lv_obj_add_flag(view->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(view->keyboard, keyboard_event_cb, LV_EVENT_ALL, view);
}

void wifi_settings_view_set_values(wifi_settings_view_t *view,
                                   const char *ssid,
                                   const char *password,
                                   const char *server)
{
    if (!view) {
        return;
    }

    if (view->ta_ssid) {
        lv_textarea_set_text(view->ta_ssid, ssid ? ssid : "");
    }
    if (view->ta_password) {
        lv_textarea_set_text(view->ta_password, password ? password : "");
    }
    if (view->ta_server) {
        lv_textarea_set_text(view->ta_server, server ? server : "");
    }
}

void wifi_settings_view_set_status(wifi_settings_view_t *view, const char *status)
{
    if (!view || !view->label_status) {
        return;
    }

    lv_label_set_text(view->label_status, status ? status : "");
}

void wifi_settings_view_set_actions_enabled(wifi_settings_view_t *view, int enabled)
{
    if (!view) {
        return;
    }

    lv_obj_t *buttons[2] = {view->btn_save, view->btn_connect};
    for (uint32_t i = 0; i < 2u; i++) {
        if (!buttons[i]) {
            continue;
        }

        if (enabled) {
            lv_obj_clear_state(buttons[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
        }
    }
}
