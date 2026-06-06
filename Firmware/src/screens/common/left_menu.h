#ifndef LEFT_MENU_H
#define LEFT_MENU_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*left_menu_nav_cb_t)(void *ctx);

typedef enum {
    LEFT_MENU_ITEM_HOME = 0,
    LEFT_MENU_ITEM_EIT,
    LEFT_MENU_ITEM_SETTINGS,
} left_menu_item_t;

typedef struct {
    void *ctx;
    left_menu_nav_cb_t on_home;
    left_menu_nav_cb_t on_eit;
    left_menu_nav_cb_t on_settings;
    left_menu_nav_cb_t on_return;
} left_menu_bindings_t;

typedef struct {
    lv_obj_t *cont;
    lv_obj_t *btn_home;
    lv_obj_t *btn_eit;
    lv_obj_t *btn_settings;
    lv_obj_t *btn_return;

    left_menu_bindings_t bindings;
} left_menu_t;

#define LEFT_MENU_WIDTH 120

void left_menu_create(left_menu_t *menu,
                      lv_obj_t *parent,
                      left_menu_item_t active_item,
                      const left_menu_bindings_t *bindings);

void left_menu_set_active(left_menu_t *menu, left_menu_item_t active_item);

int16_t left_menu_content_x(void);
int16_t left_menu_content_width(void);

#ifdef __cplusplus
}
#endif

#endif /* LEFT_MENU_H */
