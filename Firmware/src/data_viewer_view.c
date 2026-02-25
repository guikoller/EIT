#include "data_viewer_view.h"

#include <stdio.h>
#include <string.h>

static void style_table(lv_obj_t *table)
{
    lv_obj_set_style_bg_color(table, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(table, 0, 0);

    lv_obj_set_style_bg_color(table, lv_color_hex(0x1a1a1a), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(table, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(table, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(table, &lv_font_montserrat_14, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(table, 4, LV_PART_ITEMS);
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(table, lv_color_hex(0x555555), LV_PART_ITEMS);
}

static void return_btn_clicked(lv_event_t *e)
{
    data_viewer_view_t *view = (data_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (view->bindings.on_return) {
        view->bindings.on_return(view->bindings.ctx);
    }
}

static void run_btn_clicked(lv_event_t *e)
{
    data_viewer_view_t *view = (data_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    if (view->bindings.on_run) {
        view->bindings.on_run(view->bindings.ctx);
    }
}

static void tab_changed_event_cb(lv_event_t *e)
{
    data_viewer_view_t *view = (data_viewer_view_t *)lv_event_get_user_data(e);
    if (!view) return;

    if (!view->tabview) return;

    uint32_t tab_id = lv_tabview_get_tab_active(view->tabview);
    if (view->bindings.on_tab_changed) {
        view->bindings.on_tab_changed(view->bindings.ctx, tab_id);
    }
}

void data_viewer_view_create(data_viewer_view_t *view, lv_obj_t *parent, const data_viewer_view_bindings_t *bindings)
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
    lv_obj_remove_flag(view->cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(view->cont, LV_ALIGN_TOP_LEFT, 0, 0);

    view->label_title = lv_label_create(view->cont);
    lv_label_set_text(view->label_title, "");
    lv_obj_set_style_text_color(view->label_title, lv_color_hex(0x4a9fd8), 0);
    lv_obj_set_style_text_font(view->label_title, &lv_font_montserrat_22, 0);
    lv_obj_align(view->label_title, LV_ALIGN_TOP_MID, 0, 10);

    view->tabview = lv_tabview_create(view->cont);
    lv_obj_set_size(view->tabview, LV_HOR_RES - 20, LV_VER_RES - 100);
    lv_obj_align(view->tabview, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(view->tabview, lv_color_hex(0x0a0a0a), 0);
    lv_tabview_set_tab_bar_position(view->tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(view->tabview, 50);

    lv_obj_t *tab_curr = lv_tabview_add_tab(view->tabview, "Current");
    view->table_curr = lv_table_create(tab_curr);
    lv_obj_set_size(view->table_curr, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(view->table_curr, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(view->table_curr, lv_color_white(), 0);
    style_table(view->table_curr);

    lv_obj_t *tab_uel = lv_tabview_add_tab(view->tabview, "Uel");
    view->table_uel = lv_table_create(tab_uel);
    lv_obj_set_size(view->table_uel, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(view->table_uel, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(view->table_uel, lv_color_white(), 0);
    style_table(view->table_uel);

    lv_obj_t *tab_meas = lv_tabview_add_tab(view->tabview, "Meas");
    view->table_meas = lv_table_create(tab_meas);
    lv_obj_set_size(view->table_meas, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(view->table_meas, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_text_color(view->table_meas, lv_color_white(), 0);
    style_table(view->table_meas);

    lv_obj_add_event_cb(view->tabview, tab_changed_event_cb, LV_EVENT_VALUE_CHANGED, view);

    lv_obj_t *btn_return = lv_button_create(view->cont);
    lv_obj_set_size(btn_return, 150, 50);
    lv_obj_align(btn_return, LV_ALIGN_BOTTOM_LEFT, 25, -10);
    lv_obj_set_style_bg_color(btn_return, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(btn_return, 5, 0);
    lv_obj_add_event_cb(btn_return, return_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_return = lv_label_create(btn_return);
    lv_label_set_text(label_return, LV_SYMBOL_LEFT " RETURN");
    lv_obj_set_style_text_color(label_return, lv_color_white(), 0);
    lv_obj_center(label_return);

    lv_obj_t *btn_run = lv_button_create(view->cont);
    lv_obj_set_size(btn_run, 150, 50);
    lv_obj_align(btn_run, LV_ALIGN_BOTTOM_RIGHT, -25, -10);
    lv_obj_set_style_bg_color(btn_run, lv_color_hex(0x2a7da8), 0);
    lv_obj_set_style_radius(btn_run, 5, 0);
    lv_obj_add_event_cb(btn_run, run_btn_clicked, LV_EVENT_CLICKED, view);

    lv_obj_t *label_run = lv_label_create(btn_run);
    lv_label_set_text(label_run, "RUN " LV_SYMBOL_PLAY);
    lv_obj_set_style_text_color(label_run, lv_color_white(), 0);
    lv_obj_center(label_run);
}

void data_viewer_view_set_title(data_viewer_view_t *view, const char *filename)
{
    if (!view || !view->label_title) return;

    char title_text[256];
    const int uel_vals = (int)view->n_meas * (int)view->n_inj;
    snprintf(title_text, sizeof(title_text), "%s | Uel: %d vals", filename ? filename : "", uel_vals);
    lv_label_set_text(view->label_title, title_text);
}

void data_viewer_view_set_dataset_meta(data_viewer_view_t *view,
                                      uint16_t n_meas,
                                      uint16_t n_inj,
                                      uint16_t curr_pattern_rows,
                                      uint16_t meas_pattern_rows)
{
    if (!view) return;

    view->n_meas = n_meas;
    view->n_inj = n_inj;
    view->curr_pattern_rows = curr_pattern_rows;
    view->meas_pattern_rows = meas_pattern_rows;

    view->curr_populated = 0u;
    view->uel_populated = 0u;
    view->meas_populated = 0u;
}

void data_viewer_view_populate_current(data_viewer_view_t *view, const int8_t *curr_pattern)
{
    if (!view || !view->table_curr) return;
    if (!curr_pattern || view->curr_pattern_rows == 0u) return;
    if (view->curr_populated) return;

    const uint16_t rows = view->curr_pattern_rows;
    const uint16_t cols = view->n_inj;

    lv_table_set_row_count(view->table_curr, rows + 1u);
    lv_table_set_column_count(view->table_curr, cols + 1u);

    lv_table_set_column_width(view->table_curr, 0, 60);
    for (uint16_t i = 1; i <= cols; i++) {
        lv_table_set_column_width(view->table_curr, i, 60);
    }

    lv_table_set_cell_value(view->table_curr, 0, 0, "E");
    for (uint16_t j = 0; j < cols; j++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "I%d", (int)j);
        lv_table_set_cell_value(view->table_curr, 0, j + 1u, buf);
    }

    for (uint16_t i = 0; i < rows; i++) {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "E%d", (int)i);
        lv_table_set_cell_value(view->table_curr, i + 1u, 0, row_label);

        for (uint16_t j = 0; j < cols; j++) {
            const int8_t value = curr_pattern[i * cols + j];
            char buf[16];
            if (value == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if (value > 0) {
                snprintf(buf, sizeof(buf), "+%d", (int)value);
            } else {
                snprintf(buf, sizeof(buf), "%d", (int)value);
            }
            lv_table_set_cell_value(view->table_curr, i + 1u, j + 1u, buf);
        }
    }

    lv_obj_invalidate(view->table_curr);
    view->curr_populated = 1u;
}

void data_viewer_view_populate_uel(data_viewer_view_t *view, const float *uel_data)
{
    if (!view || !view->table_uel) return;
    if (!uel_data) return;
    if (view->uel_populated) return;

    const uint16_t rows = view->n_meas;
    const uint16_t cols = (view->n_inj > 15u) ? 15u : view->n_inj;

    lv_table_set_row_count(view->table_uel, rows + 1u);
    lv_table_set_column_count(view->table_uel, cols + 1u);

    lv_table_set_column_width(view->table_uel, 0, 60);
    for (uint16_t i = 1; i <= cols; i++) {
        lv_table_set_column_width(view->table_uel, i, 70);
    }

    lv_table_set_cell_value(view->table_uel, 0, 0, "M");
    for (uint16_t j = 0; j < cols; j++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "I%d", (int)j);
        lv_table_set_cell_value(view->table_uel, 0, j + 1u, buf);
    }

    for (uint16_t i = 0; i < rows; i++) {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "M%d", (int)i);
        lv_table_set_cell_value(view->table_uel, i + 1u, 0, row_label);

        for (uint16_t j = 0; j < cols; j++) {
            char buf[32];
            const float value = uel_data[i * view->n_inj + j];
            const int32_t value_mv = (int32_t)(value * 1000.0f);

            if (value_mv == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if (value_mv >= 1000 || value_mv <= -1000) {
                int32_t volts = value_mv / 1000;
                int32_t millis = value_mv % 1000;
                if (millis < 0) millis = -millis;
                snprintf(buf, sizeof(buf), "%ld.%03ld", (long)volts, (long)millis);
            } else {
                snprintf(buf, sizeof(buf), "%ldmV", (long)value_mv);
            }

            lv_table_set_cell_value(view->table_uel, i + 1u, j + 1u, buf);
        }
    }

    lv_obj_set_style_text_color(view->table_uel, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(view->table_uel, lv_color_hex(0x1a1a1a), LV_PART_ITEMS);

    lv_obj_invalidate(view->table_uel);
    view->uel_populated = 1u;
}

void data_viewer_view_populate_meas(data_viewer_view_t *view, const int8_t *meas_pattern)
{
    if (!view || !view->table_meas) return;
    if (!meas_pattern || view->meas_pattern_rows == 0u) return;
    if (view->meas_populated) return;

    const uint16_t rows = view->meas_pattern_rows;
    const uint16_t cols = view->n_meas;

    lv_table_set_row_count(view->table_meas, rows + 1u);
    lv_table_set_column_count(view->table_meas, cols + 1u);

    lv_table_set_column_width(view->table_meas, 0, 60);
    for (uint16_t i = 1; i <= cols; i++) {
        lv_table_set_column_width(view->table_meas, i, 60);
    }

    lv_table_set_cell_value(view->table_meas, 0, 0, "E");
    for (uint16_t j = 0; j < cols; j++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "M%d", (int)j);
        lv_table_set_cell_value(view->table_meas, 0, j + 1u, buf);
    }

    for (uint16_t i = 0; i < rows; i++) {
        char row_label[16];
        snprintf(row_label, sizeof(row_label), "E%d", (int)i);
        lv_table_set_cell_value(view->table_meas, i + 1u, 0, row_label);

        for (uint16_t j = 0; j < cols; j++) {
            const int8_t value = meas_pattern[i * cols + j];
            char buf[16];
            if (value == 0) {
                snprintf(buf, sizeof(buf), "0");
            } else if (value > 0) {
                snprintf(buf, sizeof(buf), "+%d", (int)value);
            } else {
                snprintf(buf, sizeof(buf), "%d", (int)value);
            }
            lv_table_set_cell_value(view->table_meas, i + 1u, j + 1u, buf);
        }
    }

    lv_obj_invalidate(view->table_meas);
    view->meas_populated = 1u;
}
