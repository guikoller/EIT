#include "wifi_settings_view.h"

#include <stdio.h>
#include <string.h>

#define COL_BG         0xf0f2f5
#define COL_CARD       0xffffff
#define COL_BORDER     0xdde1e6
#define COL_TEXT       0x1a1a2e
#define COL_LABEL      0x555770
#define COL_ACCENT     0x3a86c8
#define COL_BTN_SAVE   0x2a7da8
#define COL_BTN_CONN   0x2e7d32
#define COL_BTN_SCAN   0x5c6bc0
#define COL_STATUS_OK  0x2e7d32
#define COL_STATUS_ERR 0xc62828
#define COL_STATUS_BG  0xe8edf2
#define COL_FIELD_BG   0xfafbfc

/* Buffer for building dropdown options string */
static char s_dd_options[WIFI_SCAN_MAX * (WIFI_SSID_MAX + 8)];

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

    const char *ssid = wifi_settings_view_get_selected_ssid(v);
    v->bindings.on_save(v->bindings.ctx,
                        ssid,
                        lv_textarea_get_text(v->ta_password));
}

static void connect_clicked(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_connect) {
        return;
    }

    const char *ssid = wifi_settings_view_get_selected_ssid(v);
    v->bindings.on_connect(v->bindings.ctx,
                           ssid,
                           lv_textarea_get_text(v->ta_password));
}

static void scan_clicked(lv_event_t *e)
{
    wifi_settings_view_t *v = (wifi_settings_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_scan) {
        return;
    }

    v->bindings.on_scan(v->bindings.ctx);
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

static lv_obj_t *make_section_label(lv_obj_t *parent, const char *text,
                                     int x, int y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static lv_obj_t *make_field(lv_obj_t *parent, const char *placeholder,
                             int password, int w, int h, int x, int y)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, h);
    lv_obj_set_pos(ta, x, y);
    lv_textarea_set_one_line(ta, 1);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_password_mode(ta, password ? 1 : 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_22, 0);
    lv_obj_set_style_bg_color(ta, lv_color_hex(COL_FIELD_BG), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_radius(ta, 6, 0);
    return ta;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                              uint32_t color, int w, int h)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_shadow_width(btn, 3, 0);
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

    int16_t cw = left_menu_content_width();
    int16_t cx = left_menu_content_x();

    /* Root */
    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);

    /* Left menu */
    left_menu_bindings_t menu_bindings;
    memset(&menu_bindings, 0, sizeof(menu_bindings));
    menu_bindings.ctx = view;
    menu_bindings.on_home = nav_home_clicked;
    menu_bindings.on_eit = nav_eit_clicked;
    menu_bindings.on_settings = nav_settings_clicked;
    menu_bindings.on_return = nav_return_clicked;
    left_menu_create(&view->menu, view->cont, LEFT_MENU_ITEM_SETTINGS, &menu_bindings);

    /* Content area */
    view->content = lv_obj_create(view->cont);
    lv_obj_set_size(view->content, cw, LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, cx, 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, LV_SYMBOL_WIFI "  WiFi Configuration");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 10);

    /* Status bar */
    view->status_bar = lv_obj_create(view->content);
    lv_obj_set_size(view->status_bar, cw - 24, 32);
    lv_obj_set_pos(view->status_bar, 12, 40);
    lv_obj_set_style_bg_color(view->status_bar, lv_color_hex(COL_STATUS_BG), 0);
    lv_obj_set_style_radius(view->status_bar, 6, 0);
    lv_obj_set_style_border_width(view->status_bar, 0, 0);
    lv_obj_set_style_pad_left(view->status_bar, 8, 0);
    lv_obj_set_style_pad_top(view->status_bar, 4, 0);
    lv_obj_remove_flag(view->status_bar, LV_OBJ_FLAG_SCROLLABLE);

    view->label_status_icon = lv_label_create(view->status_bar);
    lv_label_set_text(view->label_status_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(view->label_status_icon, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(view->label_status_icon, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_status_icon, LV_ALIGN_LEFT_MID, 0, 0);

    view->label_status = lv_label_create(view->status_bar);
    lv_label_set_text(view->label_status, "Initializing...");
    lv_obj_set_style_text_color(view->label_status, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(view->label_status, &lv_font_montserrat_14, 0);
    lv_obj_align(view->label_status, LV_ALIGN_LEFT_MID, 22, 0);

    /* ---- Network card ---- */
    int card_x = 12;
    int card_w = cw - 24;
    int field_x = 12;
    int field_w = card_w - 24;

    lv_obj_t *net_card = lv_obj_create(view->content);
    lv_obj_set_size(net_card, card_w, 160);
    lv_obj_set_pos(net_card, card_x, 80);
    lv_obj_set_style_bg_color(net_card, lv_color_hex(COL_CARD), 0);
    lv_obj_set_style_border_color(net_card, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(net_card, 1, 0);
    lv_obj_set_style_radius(net_card, 10, 0);
    lv_obj_set_style_pad_all(net_card, 0, 0);
    lv_obj_remove_flag(net_card, LV_OBJ_FLAG_SCROLLABLE);

    (void)make_section_label(net_card, "NETWORK", field_x, 8);

    /* SSID dropdown + scan button on same row */
    view->dd_ssid = lv_dropdown_create(net_card);
    lv_obj_set_size(view->dd_ssid, field_w - 110, 44);
    lv_obj_set_pos(view->dd_ssid, field_x, 28);
    lv_dropdown_set_text(view->dd_ssid, "Select network...");
    lv_dropdown_set_options(view->dd_ssid, "");
    lv_obj_set_style_text_color(view->dd_ssid, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(view->dd_ssid, &lv_font_montserrat_22, 0);
    lv_obj_set_style_border_color(view->dd_ssid, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(view->dd_ssid, 1, 0);
    lv_obj_set_style_radius(view->dd_ssid, 6, 0);
    lv_obj_set_style_bg_color(view->dd_ssid, lv_color_hex(COL_FIELD_BG), 0);
    /* Style the dropdown list */
    lv_obj_set_style_text_font(lv_dropdown_get_list(view->dd_ssid),
                               &lv_font_montserrat_22, 0);
    lv_obj_set_style_max_height(lv_dropdown_get_list(view->dd_ssid), 200, 0);

    view->btn_scan = make_button(net_card, LV_SYMBOL_REFRESH " Scan",
                                 COL_BTN_SCAN, 100, 44);
    lv_obj_set_pos(view->btn_scan, field_x + field_w - 100, 28);
    lv_obj_add_event_cb(view->btn_scan, scan_clicked, LV_EVENT_CLICKED, view);

    /* Password */
    (void)make_section_label(net_card, "PASSWORD", field_x, 80);

    view->ta_password = make_field(net_card, "Wi-Fi password", 1,
                                   field_w, 44, field_x, 100);
    lv_obj_add_event_cb(view->ta_password, textarea_focus_cb,
                        LV_EVENT_FOCUSED, view);

    /* ---- Action buttons ---- */
    view->btn_save = make_button(view->content, LV_SYMBOL_SAVE " Save",
                                 COL_BTN_SAVE, 170, 50);
    lv_obj_set_pos(view->btn_save, card_x, 350);
    lv_obj_add_event_cb(view->btn_save, save_clicked, LV_EVENT_CLICKED, view);

    view->btn_connect = make_button(view->content,
                                    LV_SYMBOL_WIFI " Connect",
                                    COL_BTN_CONN, 190, 50);
    lv_obj_set_pos(view->btn_connect, card_x + 180, 350);
    lv_obj_add_event_cb(view->btn_connect, connect_clicked,
                        LV_EVENT_CLICKED, view);

    /* Keyboard (hidden by default) */
    view->keyboard = lv_keyboard_create(view->content);
    lv_obj_set_size(view->keyboard, cw - 6, 230);
    lv_obj_align(view->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(view->keyboard, &lv_font_montserrat_22, 0);
    lv_obj_add_flag(view->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(view->keyboard, keyboard_event_cb, LV_EVENT_ALL, view);
}

void wifi_settings_view_set_values(wifi_settings_view_t *view,
                                   const char *ssid,
                                   const char *password)
{
    if (!view) {
        return;
    }

    /* If there's a saved SSID but the dropdown is empty, add it as the
       sole entry so the user sees the current network. */
    if (view->dd_ssid && ssid && ssid[0] != '\0') {
        lv_dropdown_set_options(view->dd_ssid, ssid);
        lv_dropdown_set_selected(view->dd_ssid, 0);
        lv_dropdown_set_text(view->dd_ssid, NULL);
    }
    if (view->ta_password) {
        lv_textarea_set_text(view->ta_password, password ? password : "");
    }
}

void wifi_settings_view_set_status(wifi_settings_view_t *view,
                                   const char *status,
                                   int is_ok)
{
    if (!view) {
        return;
    }

    if (view->label_status) {
        lv_label_set_text(view->label_status, status ? status : "");
    }
    if (view->label_status_icon) {
        if (is_ok > 0) {
            lv_label_set_text(view->label_status_icon, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(view->label_status_icon,
                                        lv_color_hex(COL_STATUS_OK), 0);
        } else if (is_ok < 0) {
            lv_label_set_text(view->label_status_icon, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(view->label_status_icon,
                                        lv_color_hex(COL_STATUS_ERR), 0);
        } else {
            lv_label_set_text(view->label_status_icon, LV_SYMBOL_WARNING);
            lv_obj_set_style_text_color(view->label_status_icon,
                                        lv_color_hex(COL_LABEL), 0);
        }
    }
}

void wifi_settings_view_set_actions_enabled(wifi_settings_view_t *view, int enabled)
{
    if (!view) {
        return;
    }

    lv_obj_t *btns[] = {view->btn_save, view->btn_connect, view->btn_scan};
    for (uint32_t i = 0u; i < 3u; i++) {
        if (!btns[i]) {
            continue;
        }
        if (enabled) {
            lv_obj_clear_state(btns[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btns[i], LV_STATE_DISABLED);
        }
    }
}

void wifi_settings_view_populate_networks(wifi_settings_view_t *view,
                                          const wifi_scan_entry_t *entries,
                                          uint32_t count,
                                          const char *current_ssid)
{
    if (!view || !view->dd_ssid) {
        return;
    }

    if (!entries || count == 0u) {
        lv_dropdown_set_options(view->dd_ssid, "");
        lv_dropdown_set_text(view->dd_ssid, "No networks found");
        return;
    }

    /* Build newline-separated options: "SSID (RSSIdBm)" */
    s_dd_options[0] = '\0';
    uint32_t pos = 0u;
    uint32_t sel = 0u;

    for (uint32_t i = 0u; i < count; i++) {
        char entry_text[WIFI_SSID_MAX + 16];
        int n = snprintf(entry_text, sizeof(entry_text), "%s (%ddBm)",
                         entries[i].ssid, entries[i].rssi);
        if (n <= 0) {
            continue;
        }

        if (pos > 0u && pos + 1u < sizeof(s_dd_options)) {
            s_dd_options[pos++] = '\n';
        }

        uint32_t elen = (uint32_t)n;
        if (pos + elen >= sizeof(s_dd_options)) {
            break;
        }
        memcpy(s_dd_options + pos, entry_text, elen);
        pos += elen;

        if (current_ssid && strcmp(entries[i].ssid, current_ssid) == 0) {
            sel = i;
        }
    }
    s_dd_options[pos] = '\0';

    lv_dropdown_set_text(view->dd_ssid, NULL);
    lv_dropdown_set_options(view->dd_ssid, s_dd_options);
    lv_dropdown_set_selected(view->dd_ssid, (uint32_t)sel);
}

void wifi_settings_view_set_scanning(wifi_settings_view_t *view, int scanning)
{
    if (!view) {
        return;
    }

    if (view->btn_scan) {
        if (scanning) {
            lv_obj_add_state(view->btn_scan, LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(view->btn_scan, LV_STATE_DISABLED);
        }
    }
}

const char *wifi_settings_view_get_selected_ssid(wifi_settings_view_t *view)
{
    static char ssid_buf[WIFI_SSID_MAX];

    if (!view || !view->dd_ssid) {
        ssid_buf[0] = '\0';
        return ssid_buf;
    }

    lv_dropdown_get_selected_str(view->dd_ssid, ssid_buf, sizeof(ssid_buf));

    /* Strip the " (RSSIdBm)" suffix if present */
    char *paren = strrchr(ssid_buf, '(');
    if (paren && paren > ssid_buf && *(paren - 1) == ' ') {
        *(paren - 1) = '\0';
    }

    return ssid_buf;
}
