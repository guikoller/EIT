#include "serial_monitor_view.h"

#include <string.h>

#define COL_BG       0xf5f5f5
#define COL_CARD     0xffffff
#define COL_BORDER   0xe0e0e0
#define COL_TEXT     0x333333
#define COL_LOG_BG   0x1e1e1e
#define COL_LOG_TEXT 0x00ff00
#define COL_ACCENT   0x4a9fd8
#define COL_BTN      0x2a7da8
#define COL_BTN_CLR  0x888888

#define LOG_MAX_LEN  SERIAL_MONITOR_LOG_MAX_LEN

static void nav_home_clicked(void *ctx)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)ctx;
    if (v && v->bindings.on_nav_home) {
        v->bindings.on_nav_home(v->bindings.ctx);
    }
}

static void nav_eit_clicked(void *ctx)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)ctx;
    if (v && v->bindings.on_nav_eit) {
        v->bindings.on_nav_eit(v->bindings.ctx);
    }
}

static void nav_settings_clicked(void *ctx)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)ctx;
    if (v && v->bindings.on_nav_settings) {
        v->bindings.on_nav_settings(v->bindings.ctx);
    }
}

static void nav_return_clicked(void *ctx)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)ctx;
    if (v && v->bindings.on_back) {
        v->bindings.on_back(v->bindings.ctx);
    }
}

static void send_clicked(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_send) {
        return;
    }

    const char *cmd = lv_textarea_get_text(v->ta_cmd);
    v->bindings.on_send(v->bindings.ctx, cmd);
    lv_textarea_set_text(v->ta_cmd, "");
}

static void clear_clicked(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_clear) {
        return;
    }

    v->bindings.on_clear(v->bindings.ctx);
}

static void at_clicked(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_send) {
        return;
    }

    v->bindings.on_send(v->bindings.ctx, "AT");
}

static void at_rst_clicked(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->bindings.on_send) {
        return;
    }

    v->bindings.on_send(v->bindings.ctx, "AT+RST");
}

static void textarea_focus_cb(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->keyboard) {
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(v->keyboard, ta);
    lv_obj_clear_flag(v->keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void keyboard_event_cb(lv_event_t *e)
{
    serial_monitor_view_t *v = (serial_monitor_view_t *)lv_event_get_user_data(e);
    if (!v || !v->keyboard) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        /* Enter pressed — send command */
        if (v->bindings.on_send) {
            const char *cmd = lv_textarea_get_text(v->ta_cmd);
            v->bindings.on_send(v->bindings.ctx, cmd);
            lv_textarea_set_text(v->ta_cmd, "");
        }
        lv_keyboard_set_textarea(v->keyboard, NULL);
        lv_obj_add_flag(v->keyboard, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(v->keyboard, NULL);
        lv_obj_add_flag(v->keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}

void serial_monitor_view_create(serial_monitor_view_t *view,
                                lv_obj_t *parent,
                                const serial_monitor_view_bindings_t *bindings)
{
    if (!view || !parent) {
        return;
    }

    memset(view, 0, sizeof(*view));
    if (bindings) {
        view->bindings = *bindings;
    }

    /* Root container */
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
    lv_obj_set_size(view->content, left_menu_content_width(), LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, left_menu_content_x(), 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title — clarify this is direct ESP-01 UART connection */
    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, LV_SYMBOL_USB "  ESP-01 SERIAL MONITOR  (UART5 115200)");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 8);

    /* Log panel — scrollable dark background, label inside auto-wraps */
    view->log_panel = lv_obj_create(view->content);
    lv_obj_set_size(view->log_panel, left_menu_content_width() - 20, 252);
    lv_obj_align(view->log_panel, LV_ALIGN_TOP_MID, 0, 42);
    lv_obj_set_style_bg_color(view->log_panel, lv_color_hex(COL_LOG_BG), 0);
    lv_obj_set_style_pad_all(view->log_panel, 4, 0);
    lv_obj_set_style_border_color(view->log_panel, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(view->log_panel, 1, 0);
    lv_obj_set_style_radius(view->log_panel, 8, 0);

    view->label_log = lv_label_create(view->log_panel);
    lv_label_set_text(view->label_log, "");
    lv_label_set_long_mode(view->label_log, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(view->label_log, left_menu_content_width() - 28);
    lv_obj_set_style_text_color(view->label_log, lv_color_hex(COL_LOG_TEXT), 0);
    lv_obj_set_style_text_font(view->label_log, &lv_font_montserrat_14, 0);

    /* Quick-send buttons row */
    #define COL_BTN_AT   0x2e7d32
    #define COL_BTN_RST  0xb71c1c

    view->btn_at = lv_button_create(view->content);
    lv_obj_set_size(view->btn_at, 80, 40);
    lv_obj_align(view->btn_at, LV_ALIGN_TOP_LEFT, 10, 302);
    lv_obj_set_style_bg_color(view->btn_at, lv_color_hex(COL_BTN_AT), 0);
    lv_obj_set_style_radius(view->btn_at, 8, 0);
    lv_obj_add_event_cb(view->btn_at, at_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl_at = lv_label_create(view->btn_at);
    lv_label_set_text(lbl_at, "AT");
    lv_obj_set_style_text_color(lbl_at, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_at, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl_at);

    view->btn_at_rst = lv_button_create(view->content);
    lv_obj_set_size(view->btn_at_rst, 120, 40);
    lv_obj_align_to(view->btn_at_rst, view->btn_at, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_style_bg_color(view->btn_at_rst, lv_color_hex(COL_BTN_RST), 0);
    lv_obj_set_style_radius(view->btn_at_rst, 8, 0);
    lv_obj_add_event_cb(view->btn_at_rst, at_rst_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl_rst = lv_label_create(view->btn_at_rst);
    lv_label_set_text(lbl_rst, "AT+RST");
    lv_obj_set_style_text_color(lbl_rst, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_rst, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl_rst);

    /* Command input row */
    view->ta_cmd = lv_textarea_create(view->content);
    lv_obj_set_size(view->ta_cmd, left_menu_content_width() - 230, 48);
    lv_obj_align(view->ta_cmd, LV_ALIGN_TOP_LEFT, 10, 352);
    lv_textarea_set_one_line(view->ta_cmd, 1);
    lv_textarea_set_placeholder_text(view->ta_cmd, "AT command...");
    lv_obj_set_style_text_color(view->ta_cmd, lv_color_hex(COL_TEXT), 0);
    lv_obj_set_style_text_font(view->ta_cmd, &lv_font_montserrat_22, 0);
    lv_obj_set_style_border_color(view->ta_cmd, lv_color_hex(COL_BORDER), 0);
    lv_obj_set_style_border_width(view->ta_cmd, 1, 0);
    lv_obj_set_style_radius(view->ta_cmd, 8, 0);
    lv_obj_add_event_cb(view->ta_cmd, textarea_focus_cb, LV_EVENT_FOCUSED, view);

    /* Send button */
    view->btn_send = lv_button_create(view->content);
    lv_obj_set_size(view->btn_send, 100, 48);
    lv_obj_align_to(view->btn_send, view->ta_cmd, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    lv_obj_set_style_bg_color(view->btn_send, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_send, 8, 0);
    lv_obj_add_event_cb(view->btn_send, send_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl_send = lv_label_create(view->btn_send);
    lv_label_set_text(lbl_send, "SEND");
    lv_obj_set_style_text_color(lbl_send, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_send, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl_send);

    /* Clear button */
    view->btn_clear = lv_button_create(view->content);
    lv_obj_set_size(view->btn_clear, 100, 48);
    lv_obj_align_to(view->btn_clear, view->btn_send, LV_ALIGN_OUT_RIGHT_MID, 6, 0);
    lv_obj_set_style_bg_color(view->btn_clear, lv_color_hex(COL_BTN_CLR), 0);
    lv_obj_set_style_radius(view->btn_clear, 8, 0);
    lv_obj_add_event_cb(view->btn_clear, clear_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *lbl_clear = lv_label_create(view->btn_clear);
    lv_label_set_text(lbl_clear, "CLEAR");
    lv_obj_set_style_text_color(lbl_clear, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_clear, &lv_font_montserrat_22, 0);
    lv_obj_center(lbl_clear);

    /* Keyboard */
    view->keyboard = lv_keyboard_create(view->content);
    lv_obj_set_size(view->keyboard, left_menu_content_width() - 6, 230);
    lv_obj_align(view->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(view->keyboard, &lv_font_montserrat_22, 0);
    lv_obj_add_flag(view->keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(view->keyboard, keyboard_event_cb, LV_EVENT_ALL, view);
}

void serial_monitor_view_append_log(serial_monitor_view_t *view, const char *text)
{
    if (!view || !view->label_log || !text || text[0] == '\0') {
        return;
    }

    uint32_t add_len = (uint32_t)strlen(text);

    /* If adding would overflow, discard the front half to make room */
    if (view->log_len + add_len >= LOG_MAX_LEN) {
        uint32_t keep = LOG_MAX_LEN / 2u;
        uint32_t discard = view->log_len - keep;
        /* Advance discard pointer to the next newline for a clean cut */
        while (discard < view->log_len && view->log_buf[discard] != '\n') {
            discard++;
        }
        if (discard < view->log_len) {
            discard++; /* skip the newline itself */
        }
        memmove(view->log_buf, view->log_buf + discard, view->log_len - discard);
        view->log_len -= discard;
    }

    /* Append, clamping to remaining space */
    uint32_t space = LOG_MAX_LEN - view->log_len;
    if (add_len > space) {
        add_len = space;
    }
    memcpy(view->log_buf + view->log_len, text, add_len);
    view->log_len += add_len;
    view->log_buf[view->log_len] = '\0';

    lv_label_set_text(view->label_log, view->log_buf);
    lv_obj_scroll_to_y(view->log_panel, LV_COORD_MAX, LV_ANIM_OFF);
}

void serial_monitor_view_clear_log(serial_monitor_view_t *view)
{
    if (!view || !view->label_log) {
        return;
    }

    view->log_buf[0] = '\0';
    view->log_len = 0u;
    lv_label_set_text(view->label_log, "");
    lv_obj_scroll_to_y(view->log_panel, 0, LV_ANIM_OFF);
}
