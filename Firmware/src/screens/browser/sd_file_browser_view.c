#include "sd_file_browser_view.h"

#include <string.h>
#include <stdio.h>

static void file_item_clicked(lv_event_t *e)
{
    sd_file_browser_view_t *view = (sd_file_browser_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;

    lv_obj_t *btn = lv_event_get_target(e);

    if(view->selected_file_obj != NULL)
    {
        lv_obj_set_style_bg_color(view->selected_file_obj, lv_color_hex(0x2a2a2a), 0);
    }

    view->selected_file_obj = btn;
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a5f7a), 0);

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

    view->cont = lv_obj_create(parent);
    lv_obj_set_size(view->cont, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(view->cont, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_border_width(view->cont, 0, 0);
    lv_obj_set_style_pad_all(view->cont, 0, 0);
    lv_obj_center(view->cont);

    view->label_title = lv_label_create(view->cont);
    lv_label_set_text(view->label_title, "SD STORAGE");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_LEFT, 20, 15);

    view->file_list = lv_list_create(view->cont);
    lv_obj_set_size(view->file_list, LV_HOR_RES - 60, LV_VER_RES - 150);
    lv_obj_align(view->file_list, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(view->file_list, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_color(view->file_list, lv_color_hex(0x3a3a3a), 0);
    lv_obj_set_style_border_width(view->file_list, 2, 0);
    lv_obj_set_style_radius(view->file_list, 5, 0);

    view->label_status = lv_label_create(view->cont);
    lv_label_set_text(view->label_status, "READY");
    lv_obj_set_style_text_color(view->label_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(view->label_status, &lv_font_montserrat_22, 0);
    /* Vertically align with the bottom buttons (y=-20, h=50 => center is -45). */
    lv_obj_align(view->label_status, LV_ALIGN_BOTTOM_MID, 0, -45);

    view->btn_load = lv_button_create(view->cont);
    lv_obj_set_size(view->btn_load, 220, 50);
    lv_obj_align(view->btn_load, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(view->btn_load, lv_color_hex(0x2a7da8), 0);
    lv_obj_set_style_radius(view->btn_load, 5, 0);
    lv_obj_add_event_cb(view->btn_load, load_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *btn_label = lv_label_create(view->btn_load);
    lv_label_set_text(btn_label, "LOAD DATASET");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
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
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a2a), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 18, 0);
        lv_obj_set_style_min_height(btn, 60, 0);
        lv_obj_add_event_cb(btn, file_item_clicked, LV_EVENT_CLICKED, view);

        uint32_t ccount = lv_obj_get_child_count(btn);
        if(ccount > 0)
        {
            lv_obj_t *icon = lv_obj_get_child(btn, 0);
            lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        }
        if(ccount > 1)
        {
            lv_obj_t *label = lv_obj_get_child(btn, 1);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
        }
    }
}
