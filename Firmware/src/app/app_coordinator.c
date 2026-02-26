#include "app_coordinator.h"

#include "app_fsm.h"
#include "screens/data_viewer/data_viewer.h"
#include "services/storage_service.h"
#include "hal_stm_lvgl/tft/tft.h"
#include "hal_stm_lvgl/touchpad/touchpad.h"
#include "lvgl/lvgl.h"
#include "screens/reconstruction/reconstruction_viewer.h"
#include "screens/browser/sd_file_browser.h"

static app_state_t s_app;

#define APP_EVENT_QUEUE_SIZE 8u
static app_event_t s_evt_q[APP_EVENT_QUEUE_SIZE];
static uint8_t s_evt_q_head = 0u;
static uint8_t s_evt_q_tail = 0u;
static uint8_t s_evt_q_count = 0u;

static int evt_q_push(const app_event_t *evt)
{
    if (!evt) return 0;
    if (s_evt_q_count >= APP_EVENT_QUEUE_SIZE) {
        return 0;
    }

    s_evt_q[s_evt_q_tail] = *evt;
    s_evt_q_tail = (uint8_t)((s_evt_q_tail + 1u) % APP_EVENT_QUEUE_SIZE);
    s_evt_q_count++;
    return 1;
}

static int evt_q_pop(app_event_t *out)
{
    if (!out) return 0;
    if (s_evt_q_count == 0u) return 0;

    *out = s_evt_q[s_evt_q_head];
    s_evt_q_head = (uint8_t)((s_evt_q_head + 1u) % APP_EVENT_QUEUE_SIZE);
    s_evt_q_count--;
    return 1;
}

static void app_apply_requested_screen(void)
{
    if (s_app.requested_screen == APP_SCREEN_NONE) return;
    if (s_app.requested_screen == s_app.active_screen) {
        s_app.requested_screen = APP_SCREEN_NONE;
        return;
    }

    lv_obj_clean(lv_screen_active());

    switch (s_app.requested_screen) {
        case APP_SCREEN_BROWSER:
            sd_file_browser_create();
            s_app.active_screen = APP_SCREEN_BROWSER;
            break;

        case APP_SCREEN_DATA_VIEWER:
            if (s_app.selected_file[0] == '\0') {
                s_app.current_state = APP_STATE_BROWSER;
                s_app.requested_screen = APP_SCREEN_BROWSER;
                return;
            }
            data_viewer_create(s_app.selected_file);
            s_app.active_screen = APP_SCREEN_DATA_VIEWER;
            break;

        case APP_SCREEN_RECON_VIEWER:
            if (s_app.selected_file[0] == '\0') {
                s_app.current_state = APP_STATE_BROWSER;
                s_app.requested_screen = APP_SCREEN_BROWSER;
                return;
            }
            reconstruction_viewer_create(s_app.selected_file);
            s_app.active_screen = APP_SCREEN_RECON_VIEWER;
            break;

        default:
            break;
    }

    s_app.requested_screen = APP_SCREEN_NONE;
}

void app_coordinator_init(void)
{
    app_fsm_init(&s_app);

    /* Storage and UI stack init */
    s_app.sd_ready = (storage_service_init() == 0) ? 1u : 0u;

    lv_init();
    tft_init();
    touchpad_init();

    app_event_t evt;
    evt.type = APP_EVENT_BOOT_COMPLETE;
    evt.data.boot.sd_ready = s_app.sd_ready;
    app_fsm_dispatch(&s_app, &evt);

    app_apply_requested_screen();
}

void app_coordinator_tick(void)
{
    lv_task_handler();

    app_event_t evt;
    while (evt_q_pop(&evt)) {
        app_fsm_dispatch(&s_app, &evt);
    }

    app_fsm_tick(&s_app);
    app_apply_requested_screen();

    /* Let LVGL render the (possibly) new screen without waiting a full tick. */
    lv_task_handler();
}

void app_coordinator_post_event(const app_event_t *event)
{
    (void)evt_q_push(event);
}

const app_state_t *app_coordinator_get_state(void)
{
    return &s_app;
}
