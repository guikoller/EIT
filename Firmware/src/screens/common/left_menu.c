#include "left_menu.h"

#include <string.h>

#define COL_MENU_BG       0xe9eef2
#define COL_MENU_BORDER   0xd5dde3
#define COL_MENU_BTN      0xffffff
#define COL_MENU_BTN_ACT  0x2a7da8
#define COL_MENU_TEXT     0x3a4a55
#define COL_MENU_BTN_RET  0x7b8a96

#define MENU_BTN_SIZE     78
#define MENU_BTN_GAP      16
#define MENU_TOP_MARGIN   20
#define MENU_BOTTOM_MARGIN 18

static lv_obj_t *create_menu_button(lv_obj_t *parent,
                                    const char *text,
                                    lv_event_cb_t cb,
                                    left_menu_t *menu)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, MENU_BTN_SIZE, MENU_BTN_SIZE);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xc8d2db), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_MENU_BTN), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, menu);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_MENU_TEXT), 0);
    lv_obj_center(lbl);

    return btn;
}

static lv_obj_t *create_return_button(lv_obj_t *parent,
                                      lv_event_cb_t cb,
                                      left_menu_t *menu)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, MENU_BTN_SIZE, MENU_BTN_SIZE);
    lv_obj_set_style_radius(btn, 14, 0);
    lv_obj_set_style_shadow_width(btn, 4, 0);
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xc8d2db), 0);
    lv_obj_set_style_shadow_ofs_y(btn, 2, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_MENU_BTN_RET), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, menu);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_center(lbl);

    return btn;
}

static void style_menu_btn(lv_obj_t *btn, int active)
{
    if (!btn) return;

    lv_obj_set_style_bg_color(btn,
                              active ? lv_color_hex(COL_MENU_BTN_ACT)
                                     : lv_color_hex(COL_MENU_BTN),
                              0);

    lv_obj_t *lbl = lv_obj_get_child(btn, 0);
    if (lbl) {
        lv_obj_set_style_text_color(lbl,
                                    active ? lv_color_white()
                                           : lv_color_hex(COL_MENU_TEXT),
                                    0);
    }
}

static void menu_home_clicked(lv_event_t *e)
{
    left_menu_t *menu = (left_menu_t *)lv_event_get_user_data(e);
    if (!menu) return;

    left_menu_set_active(menu, LEFT_MENU_ITEM_HOME);
    if (menu->bindings.on_home) {
        menu->bindings.on_home(menu->bindings.ctx);
    }
}

static void menu_eit_clicked(lv_event_t *e)
{
    left_menu_t *menu = (left_menu_t *)lv_event_get_user_data(e);
    if (!menu) return;

    left_menu_set_active(menu, LEFT_MENU_ITEM_EIT);
    if (menu->bindings.on_eit) {
        menu->bindings.on_eit(menu->bindings.ctx);
    }
}

static void menu_settings_clicked(lv_event_t *e)
{
    left_menu_t *menu = (left_menu_t *)lv_event_get_user_data(e);
    if (!menu) return;

    left_menu_set_active(menu, LEFT_MENU_ITEM_SETTINGS);
    if (menu->bindings.on_settings) {
        menu->bindings.on_settings(menu->bindings.ctx);
    }
}

static void menu_return_clicked(lv_event_t *e)
{
    left_menu_t *menu = (left_menu_t *)lv_event_get_user_data(e);
    if (!menu) return;

    if (menu->bindings.on_return) {
        menu->bindings.on_return(menu->bindings.ctx);
    }
}

void left_menu_create(left_menu_t *menu,
                      lv_obj_t *parent,
                      left_menu_item_t active_item,
                      const left_menu_bindings_t *bindings)
{
    if (!menu || !parent) return;

    memset(menu, 0, sizeof(*menu));
    if (bindings) {
        menu->bindings = *bindings;
    }

    menu->cont = lv_obj_create(parent);
    lv_obj_set_size(menu->cont, LEFT_MENU_WIDTH, LV_VER_RES);
    lv_obj_align(menu->cont, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(menu->cont, lv_color_hex(COL_MENU_BG), 0);
    lv_obj_set_style_border_width(menu->cont, 0, 0);
    lv_obj_set_style_border_side(menu->cont, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_border_color(menu->cont, lv_color_hex(COL_MENU_BORDER), 0);
    lv_obj_set_style_border_width(menu->cont, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu->cont, 6, 0);
    lv_obj_remove_flag(menu->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_layout(menu->cont, LV_LAYOUT_NONE);

    menu->btn_home = create_menu_button(menu->cont,
                                        LV_SYMBOL_HOME,
                                        menu_home_clicked,
                                        menu);
    menu->btn_eit = create_menu_button(menu->cont,
                                       LV_SYMBOL_FILE,
                                       menu_eit_clicked,
                                       menu);
    menu->btn_settings = create_menu_button(menu->cont,
                                            LV_SYMBOL_SETTINGS,
                                            menu_settings_clicked,
                                            menu);

    lv_obj_align(menu->btn_home, LV_ALIGN_TOP_MID, 0, MENU_TOP_MARGIN);
    lv_obj_align(menu->btn_eit, LV_ALIGN_TOP_MID, 0, MENU_TOP_MARGIN + MENU_BTN_SIZE + MENU_BTN_GAP);
    lv_obj_align(menu->btn_settings,
                 LV_ALIGN_TOP_MID,
                 0,
                 MENU_TOP_MARGIN + 2 * (MENU_BTN_SIZE + MENU_BTN_GAP));

    if (menu->bindings.on_return) {
        menu->btn_return = create_return_button(menu->cont, menu_return_clicked, menu);
        lv_obj_align(menu->btn_return, LV_ALIGN_BOTTOM_MID, 0, -MENU_BOTTOM_MARGIN);
    }

    left_menu_set_active(menu, active_item);
}

void left_menu_set_active(left_menu_t *menu, left_menu_item_t active_item)
{
    if (!menu) return;

    style_menu_btn(menu->btn_home, active_item == LEFT_MENU_ITEM_HOME);
    style_menu_btn(menu->btn_eit, active_item == LEFT_MENU_ITEM_EIT);
    style_menu_btn(menu->btn_settings, active_item == LEFT_MENU_ITEM_SETTINGS);
}

int16_t left_menu_content_x(void)
{
    return LEFT_MENU_WIDTH;
}

int16_t left_menu_content_width(void)
{
    return (int16_t)(LV_HOR_RES - LEFT_MENU_WIDTH);
}
