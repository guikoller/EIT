#include "app_fsm.h"

#include <string.h>

static void set_selected_file(app_state_t *state, const char *filename)
{
    if (!state) return;
    if (!filename) {
        state->selected_file[0] = '\0';
        return;
    }

    strncpy(state->selected_file, filename, APP_FILENAME_MAX - 1);
    state->selected_file[APP_FILENAME_MAX - 1] = '\0';
}

void app_fsm_init(app_state_t *state)
{
    if (!state) return;

    memset(state, 0, sizeof(*state));
    state->current_state = APP_STATE_BOOT;
    state->active_screen = APP_SCREEN_NONE;
    state->requested_screen = APP_SCREEN_NONE;

    /* Default settings (from eit_config.h) */
    state->settings.algorithm       = EIT_ALGO_DEFAULT;
    state->settings.image_size      = EIT_IMAGE_SIZE;
    state->settings.show_data_table = EIT_SHOW_DATA_TABLE_DEFAULT;
}

void app_fsm_dispatch(app_state_t *state, const app_event_t *event)
{
    if (!state || !event) return;

    switch (event->type) {
        case APP_EVENT_OPEN_HOME:
            state->current_state = APP_STATE_HOME;
            state->requested_screen = APP_SCREEN_HOME;
            return;

        case APP_EVENT_OPEN_SETTINGS:
            state->current_state = APP_STATE_SETTINGS;
            state->requested_screen = APP_SCREEN_SETTINGS;
            return;

        case APP_EVENT_OPEN_WIFI_SETTINGS:
            state->current_state = APP_STATE_WIFI_SETTINGS;
            state->requested_screen = APP_SCREEN_WIFI_SETTINGS;
            return;

        case APP_EVENT_OPEN_BROWSER:
            state->current_state = APP_STATE_BROWSER;
            state->requested_screen = APP_SCREEN_BROWSER;
            return;

        case APP_EVENT_OPEN_DATA_VIEWER:
            set_selected_file(state, event->data.nav.filename);
            state->current_state = APP_STATE_DATA_VIEWER;
            state->requested_screen = APP_SCREEN_DATA_VIEWER;
            return;

        case APP_EVENT_OPEN_RECON_VIEWER:
            set_selected_file(state, event->data.nav.filename);
            state->current_state = APP_STATE_RECON_VIEWER;
            state->requested_screen = APP_SCREEN_RECON_VIEWER;
            return;

        default:
            break;
    }

    switch (state->current_state) {
        case APP_STATE_BOOT:
            if (event->type == APP_EVENT_BOOT_COMPLETE) {
                state->sd_ready = event->data.boot.sd_ready;
                state->current_state = APP_STATE_HOME;
                state->requested_screen = APP_SCREEN_HOME;
            }
            break;

        case APP_STATE_HOME:
            /* No back from home */
            break;

        case APP_STATE_SETTINGS:
            if (event->type == APP_EVENT_BACK || event->type == APP_EVENT_OPEN_HOME) {
                state->current_state = APP_STATE_HOME;
                state->requested_screen = APP_SCREEN_HOME;
            } else if (event->type == APP_EVENT_OPEN_WIFI_SETTINGS) {
                state->current_state = APP_STATE_WIFI_SETTINGS;
                state->requested_screen = APP_SCREEN_WIFI_SETTINGS;
            }
            break;

        case APP_STATE_WIFI_SETTINGS:
            if (event->type == APP_EVENT_BACK) {
                state->current_state = APP_STATE_SETTINGS;
                state->requested_screen = APP_SCREEN_SETTINGS;
            } else if (event->type == APP_EVENT_OPEN_HOME) {
                state->current_state = APP_STATE_HOME;
                state->requested_screen = APP_SCREEN_HOME;
            }
            break;

        case APP_STATE_BROWSER:
        case APP_STATE_DATA_VIEWER:
        case APP_STATE_RECON_VIEWER:
            if (event->type == APP_EVENT_BACK) {
                state->current_state = APP_STATE_HOME;
                state->requested_screen = APP_SCREEN_HOME;
                break;
            }
            if (event->type == APP_EVENT_ERROR) {
                state->last_error = event->data.error.code;
                state->current_state = APP_STATE_ERROR;
            }
            break;

        case APP_STATE_ERROR:
            if (event->type == APP_EVENT_BACK) {
                state->current_state = APP_STATE_HOME;
                state->requested_screen = APP_SCREEN_HOME;
            }
            break;

        default:
            break;
    }
}

void app_fsm_tick(app_state_t *state)
{
    (void)state;
}
