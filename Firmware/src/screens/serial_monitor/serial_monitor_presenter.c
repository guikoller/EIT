#include "serial_monitor_presenter.h"

#include "app/app_coordinator.h"
#include "services/wifi/esp8266_uart.h"
#include "services/wifi/esp8266_at.h"

#include <string.h>

void serial_monitor_presenter_init(serial_monitor_presenter_t *p,
                                   serial_monitor_view_t *v)
{
    if (!p) {
        return;
    }

    memset(p, 0, sizeof(*p));
    p->view = v;
}

void serial_monitor_presenter_on_create(serial_monitor_presenter_t *p)
{
    if (!p || !p->view) {
        return;
    }

    if (!esp8266_uart_is_ready()) {
        if (esp8266_uart_init(115200u) != 0) {
            serial_monitor_view_append_log(p->view,
                                           "[SYS] UART init failed\n");
            return;
        }
    }

    serial_monitor_view_append_log(p->view, "[SYS] Serial monitor ready\n");
    p->polling = 1u;
}

void serial_monitor_presenter_poll(serial_monitor_presenter_t *p)
{
    if (!p || !p->view || !p->polling) {
        return;
    }

    /* Read raw bytes without triggering the blocking debug UART hex output */
    uint8_t raw[128];
    int n = esp8266_uart_read_bytes_raw(raw, sizeof(raw));
    if (n <= 0) {
        return;
    }

    /* Translate raw bytes to displayable characters:
     *   \r (0x0D)       -> drop (paired with \n)
     *   \n (0x0A)       -> keep
     *   printable ASCII -> keep
     *   everything else -> '.' so garbage is visible and 0x00 cannot
     *                       truncate the string before LVGL sees it */
    char filtered[129];
    int out = 0;
    for (int i = 0; i < n; i++) {
        uint8_t b = raw[i];
        if (b == '\r') {
            continue;
        } else if (b == '\n' || (b >= 0x20u && b <= 0x7Eu)) {
            filtered[out++] = (char)b;
        } else {
            filtered[out++] = '.';
        }
    }
    if (out == 0) {
        return;
    }
    filtered[out] = '\0';

    serial_monitor_view_append_log(p->view, filtered);
}

void serial_monitor_presenter_on_back(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_BACK;
    app_coordinator_post_event(&evt);
}

void serial_monitor_presenter_on_send(void *ctx, const char *cmd)
{
    serial_monitor_presenter_t *p = (serial_monitor_presenter_t *)ctx;
    if (!p || !p->view || !cmd || cmd[0] == '\0') {
        return;
    }

    /* Echo the command to the log */
    serial_monitor_view_append_log(p->view, ">> ");
    serial_monitor_view_append_log(p->view, cmd);
    serial_monitor_view_append_log(p->view, "\n");

    /* Write directly to UART — non-blocking, no wait for response.
       The poll() loop displays all incoming bytes in real time, which
       is what a serial monitor should do. This also means AT+RST boot
       messages appear live as they arrive. */
    uint32_t len = (uint32_t)strlen(cmd);
    (void)esp8266_at_write_raw((const uint8_t *)cmd, len, 1000u);
    static const uint8_t crlf[2] = {'\r', '\n'};
    (void)esp8266_at_write_raw(crlf, 2u, 200u);
}

void serial_monitor_presenter_on_clear(void *ctx)
{
    serial_monitor_presenter_t *p = (serial_monitor_presenter_t *)ctx;
    if (!p || !p->view) {
        return;
    }

    serial_monitor_view_clear_log(p->view);
}

void serial_monitor_presenter_on_nav_home(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_HOME;
    app_coordinator_post_event(&evt);
}

void serial_monitor_presenter_on_nav_eit(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_BROWSER;
    app_coordinator_post_event(&evt);
}

void serial_monitor_presenter_on_nav_settings(void *ctx)
{
    (void)ctx;

    app_event_t evt;
    evt.type = APP_EVENT_OPEN_SETTINGS;
    app_coordinator_post_event(&evt);
}
