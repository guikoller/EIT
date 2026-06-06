#include "sd_file_browser_view.h"

#include <string.h>
#include <stdio.h>

/* ---- Light theme color palette ---- */
#define COL_BG          0xf5f5f5
#define COL_LIST_BG     0xffffff
#define COL_LIST_BORDER 0xe0e0e0
#define COL_ITEM_BG     0xffffff
#define COL_ITEM_SEL    0xe3f2fd
#define COL_TEXT        0x333333
#define COL_TEXT_SEC    0x666666
#define COL_ACCENT      0x4a9fd8
#define COL_BTN         0x2a7da8

static void nav_home_clicked(void *ctx)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)ctx;
    if (view && view->bindings.on_nav_home) {
        view->bindings.on_nav_home(view->bindings.ctx);
    }
}

static void nav_eit_clicked(void *ctx)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)ctx;
    if (view && view->bindings.on_nav_eit) {
        view->bindings.on_nav_eit(view->bindings.ctx);
    }
}

static void nav_settings_clicked(void *ctx)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)ctx;
    if (view && view->bindings.on_nav_settings) {
        view->bindings.on_nav_settings(view->bindings.ctx);
    }
}

static void nav_return_clicked(void *ctx)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)ctx;
    if (!view) return;

    if (view->bindings.on_back) {
        view->bindings.on_back(view->bindings.ctx);
    }
}

static void file_item_clicked(lv_event_t *e)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);

    if(view->selected_file_obj != NULL)
    {
        lv_obj_set_style_bg_color(view->selected_file_obj, lv_color_hex(COL_ITEM_BG), 0);
    }

    view->selected_file_obj = btn;
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_ITEM_SEL), 0);

    lv_obj_t *label = lv_obj_get_child(btn, 1);
    if(label == NULL) return;
    const char *text = lv_label_get_text(label);

    char *pipe = strchr(text, '|');
    if(pipe)
    {
        size_t len = (size_t)(pipe - text);
        while (len > 0 && (text[len - 1] == ' ')) len--;
        if(len < sizeof(view->selected_filename))
        {
            strncpy(view->selected_filename, text, len);
            view->selected_filename[len] = '\0';
        }
    }
}

static void load_btn_clicked(lv_event_t *e)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;

    if(view->selected_filename[0] == '\0')
    {
        sd_file_browser_view_set_status(view, "SELECT FILE");
        return;
    }

    if (view->bindings.on_load) {
        view->bindings.on_load(view->bindings.ctx, view->selected_filename);
    }
}

static void format_size(uint32_t bytes, char *buf, size_t buf_size)
{
    if(bytes >= 1024u * 1024u)
    {
        snprintf(buf, buf_size, "%luMB", (unsigned long)(bytes / (1024u * 1024u)));
    }
    else if(bytes >= 1024u)
    {
        snprintf(buf, buf_size, "%luKB", (unsigned long)(bytes / 1024u));
    }
    else
    {
        snprintf(buf, buf_size, "%lub", (unsigned long)bytes);
    }
}

void sd_file_browser_view_create(sd_file_browser_view_t *view, lv_obj_t *parent, const sd_file_browser_view_bindings_t *bindings)
{
    if (!view || !parent) return;
    memset(view, 0, sizeof(*view));

    if (bindings) {
        view->bindings = *bindings;
    } else {
        memset(&view->bindings, 0, sizeof(view->bindings));
    }

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
    menu_bindings.on_return = nav_return_clicked;
    left_menu_create(&view->menu, view->cont, LEFT_MENU_ITEM_EIT, &menu_bindings);

    view->content = lv_obj_create(view->cont);
    lv_obj_set_size(view->content, left_menu_content_width(), LV_VER_RES);
    lv_obj_align(view->content, LV_ALIGN_TOP_LEFT, left_menu_content_x(), 0);
    lv_obj_set_style_bg_color(view->content, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_border_width(view->content, 0, 0);
    lv_obj_set_style_pad_all(view->content, 0, 0);
    lv_obj_remove_flag(view->content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title label */
    view->label_title = lv_label_create(view->content);
    lv_label_set_text(view->label_title, LV_SYMBOL_FILE "  SD STORAGE");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(COL_ACCENT), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 16);

    /* File list - white background with subtle border */
    view->file_list = lv_list_create(view->content);
    lv_obj_set_size(view->file_list, left_menu_content_width() - 34, LV_VER_RES - 170);
    lv_obj_align(view->file_list, LV_ALIGN_TOP_MID, 0, 58);
    lv_obj_set_style_bg_color(view->file_list, lv_color_hex(COL_LIST_BG), 0);
    lv_obj_set_style_border_color(view->file_list, lv_color_hex(COL_LIST_BORDER), 0);
    lv_obj_set_style_border_width(view->file_list, 1, 0);
    lv_obj_set_style_radius(view->file_list, 8, 0);
    lv_obj_set_style_shadow_width(view->file_list, 4, 0);
    lv_obj_set_style_shadow_color(view->file_list, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_shadow_ofs_y(view->file_list, 2, 0);

    view->label_status = NULL;
    view->btn_back = NULL;

    /* Load button */
    view->btn_load = lv_button_create(view->content);
    lv_obj_set_size(view->btn_load, 300, 62);
    lv_obj_align(view->btn_load, LV_ALIGN_BOTTOM_RIGHT, -18, -12);
    lv_obj_set_style_bg_color(view->btn_load, lv_color_hex(COL_BTN), 0);
    lv_obj_set_style_radius(view->btn_load, 8, 0);
    lv_obj_set_style_shadow_width(view->btn_load, 6, 0);
    lv_obj_set_style_shadow_color(view->btn_load, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_shadow_ofs_y(view->btn_load, 3, 0);
    lv_obj_add_event_cb(view->btn_load, load_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *btn_label = lv_label_create(view->btn_load);
    lv_label_set_text(btn_label, LV_SYMBOL_UPLOAD "  LOAD DATASET");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_22, 0);
    lv_obj_center(btn_label);

    view->selected_file_obj = NULL;
    view->selected_filename[0] = '\0';
}

void sd_file_browser_view_set_enabled(sd_file_browser_view_t *view, int enabled)
{
    if (!view) return;

    if (view->btn_load) {
        if (enabled) lv_obj_clear_state(view->btn_load, LV_STATE_DISABLED);
        else lv_obj_add_state(view->btn_load, LV_STATE_DISABLED);
    }
    if (view->file_list) {
        if (enabled) lv_obj_clear_state(view->file_list, LV_STATE_DISABLED);
        else lv_obj_add_state(view->file_list, LV_STATE_DISABLED);
    }
}

void sd_file_browser_view_set_status(sd_file_browser_view_t *view, const char *text)
{
    if (!view || !view->label_status) return;
    lv_label_set_text(view->label_status, text ? text : "");
}

void sd_file_browser_view_set_files(sd_file_browser_view_t *view, const sd_browser_file_entry_t *entries, int count)
{
    if (!view || !view->file_list) return;

    view->selected_file_obj = NULL;
    view->selected_filename[0] = '\0';

    uint32_t child_cnt = lv_obj_get_child_count(view->file_list);
    for(uint32_t i = child_cnt; i > 0; i--)
    {
        lv_obj_t *child = lv_obj_get_child(view->file_list, i - 1);
        lv_obj_delete(child);
    }

    if (!entries || count <= 0) {
        return;
    }

    for(int i = 0; i < count; i++)
    {
        if(!entries[i].is_valid) continue;

        char size_str[16];
        format_size(entries[i].size, size_str, sizeof(size_str));

        char label_text[128];
        snprintf(label_text, sizeof(label_text), "%s | %s", entries[i].filename, size_str);

        lv_obj_t *btn = lv_list_add_button(view->file_list, LV_SYMBOL_FILE, label_text);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COL_ITEM_BG), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(COL_LIST_BORDER), 0);
        lv_obj_set_style_pad_all(btn, 18, 0);
        lv_obj_set_style_min_height(btn, 66, 0);
        lv_obj_add_event_cb(btn, file_item_clicked, LV_EVENT_CLICKED, view);

        uint32_t ccount = lv_obj_get_child_count(btn);
        if(ccount > 0)
        {
            lv_obj_t *icon = lv_obj_get_child(btn, 0);
            lv_obj_set_style_text_color(icon, lv_color_hex(COL_ACCENT), 0);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_22, 0);
        }
        if(ccount > 1)
        {
            lv_obj_t *label = lv_obj_get_child(btn, 1);
            lv_obj_set_style_text_color(label, lv_color_hex(COL_TEXT), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
        }
    }
}
