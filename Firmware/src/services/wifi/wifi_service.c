#include "wifi_service.h"

#include "esp8266_at.h"
#include "esp8266_uart.h"

#include "eit_config.h"
#include "services/json_encoder.h"
#include "services/storage_service.h"

#include "ff.h"
#include "stm32f7xx_hal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIFI_CONFIG_PATH "0:/wifi_settings.json"

#define WIFI_ESP_BOOT_SETTLE_MS        500u
#define WIFI_ESP_REINIT_SETTLE_MS      0u
#define WIFI_ESP_AT_TIMEOUT_MS         900u
#define WIFI_ESP_AT_RETRIES            3u
#define WIFI_ESP_RESYNC_TIMEOUT_MS     600u
#define WIFI_ESP_RESYNC_RETRIES        2u
#define WIFI_ESP_RESET_LOW_MS          120u
#define WIFI_ESP_JOIN_TIMEOUT_MS       30000u

static const uint32_t s_uart_probe_bauds[] = {
    115200u,
    9600u,
    57600u,
    38400u,
    19200u,
    74880u,
};

static wifi_service_config_t s_cfg;
static uint8_t s_inited = 0u;
static wifi_service_state_t s_state = WIFI_STATE_OFFLINE;
static char s_last_error[96] = "";
static uint32_t s_uart_baud = 0u;
static esp8266_uart_pinmap_t s_uart_pinmap = ESP8266_UART_PINMAP_PRIMARY;

static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
}

static void set_error_with_response(const char *message, const char *response)
{
    if (response && response[0] != '\0') {
        set_error("%s (rx: %.48s)", message, response);
    } else {
        set_error("%s", message);
    }
}

static void clear_error(void)
{
    s_last_error[0] = '\0';
}

static uint32_t bounded_strlen(const char *text, uint32_t max_len)
{
    uint32_t len = 0u;

    if (!text) {
        return 0u;
    }

    while (len < max_len && text[len] != '\0') {
        len++;
    }

    return len;
}

static void safe_copy(char *dst, uint32_t dst_size, const char *src)
{
    if (!dst || dst_size == 0u) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    uint32_t copy_len = bounded_strlen(src, dst_size - 1u);
    if (copy_len > 0u) {
        memcpy(dst, src, copy_len);
    }
    dst[copy_len] = '\0';
}

static int extract_json_string(const char *json,
                               const char *key,
                               char *out,
                               uint32_t out_size)
{
    if (!json || !key || !out || out_size == 0u) {
        return 0;
    }

    char pattern[32];
    (void)snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *pos = strstr(json, pattern);
    if (!pos) {
        return 0;
    }

    pos = strchr(pos, ':');
    if (!pos) {
        return 0;
    }
    pos++;

    while (*pos == ' ' || *pos == '\t') {
        pos++;
    }

    if (*pos != '"') {
        return 0;
    }
    pos++;

    uint32_t n = 0u;
    while (*pos && *pos != '"' && n + 1u < out_size) {
        out[n++] = *pos++;
    }
    out[n] = '\0';

    return (n > 0u) ? 1 : 0;
}

static int escape_at_field(const char *in, char *out, uint32_t out_size)
{
    if (!in || !out || out_size == 0u) {
        return 0;
    }

    uint32_t w = 0u;
    while (*in != '\0') {
        char ch = *in++;
        if (ch == '"' || ch == '\\') {
            if (w + 2u >= out_size) {
                return 0;
            }
            out[w++] = '\\';
            out[w++] = ch;
        } else {
            if (w + 1u >= out_size) {
                return 0;
            }
            out[w++] = ch;
        }
    }

    out[w] = '\0';
    return 1;
}

static int build_join_command(char *out, uint32_t out_size)
{
    char ssid_escaped[(WIFI_SSID_MAX * 2u) + 1u];
    char pass_escaped[(WIFI_PASSWORD_MAX * 2u) + 1u];

    if (!escape_at_field(s_cfg.ssid, ssid_escaped, sizeof(ssid_escaped))) {
        return 0;
    }
    if (!escape_at_field(s_cfg.password, pass_escaped, sizeof(pass_escaped))) {
        return 0;
    }

    int n = snprintf(out,
                     out_size,
                     "AT+CWJAP=\"%s\",\"%s\"",
                     ssid_escaped,
                     pass_escaped);

    if (n <= 0 || (uint32_t)n >= out_size) {
        return 0;
    }

    return 1;
}

static int parse_server_url(const char *server,
                            char *host,
                            uint32_t host_size,
                            uint16_t *port,
                            char *path,
                            uint32_t path_size)
{
    if (!server || !host || !port || !path || host_size == 0u || path_size == 0u) {
        return 0;
    }

    const char *p = server;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        set_error("HTTPS is not supported in the current flow");
        return 0;
    }

    const char *slash = strchr(p, '/');
    const char *host_end = slash ? slash : (p + strlen(p));

    if (host_end == p) {
        set_error("Invalid server host");
        return 0;
    }

    const char *colon = NULL;
    for (const char *it = p; it < host_end; it++) {
        if (*it == ':') {
            colon = it;
            break;
        }
    }

    uint32_t host_len = 0u;
    if (colon) {
        host_len = (uint32_t)(colon - p);
        uint32_t port_len = (uint32_t)(host_end - (colon + 1));
        if (port_len == 0u || port_len >= 8u) {
            set_error("Invalid server port");
            return 0;
        }

        char port_text[8];
        memcpy(port_text, colon + 1, port_len);
        port_text[port_len] = '\0';

        char *endptr = NULL;
        unsigned long port_val = strtoul(port_text, &endptr, 10);
        if (!endptr || *endptr != '\0' || port_val == 0u || port_val > 65535u) {
            set_error("Invalid server port");
            return 0;
        }

        *port = (uint16_t)port_val;
    } else {
        host_len = (uint32_t)(host_end - p);
        *port = 80u;
    }

    if (host_len == 0u || host_len + 1u > host_size) {
        set_error("Invalid server host");
        return 0;
    }

    memcpy(host, p, host_len);
    host[host_len] = '\0';

    if (slash && slash[0] != '\0') {
        safe_copy(path, path_size, slash);
    } else {
        safe_copy(path, path_size, "/");
    }

    return 1;
}

static int load_file_to_buffer(const char *path,
                               char *buf,
                               uint32_t buf_size,
                               uint32_t *out_len)
{
    if (!path || !buf || buf_size < 2u) {
        return 0;
    }

    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        set_error("Failed to open %s (r%u)", path, (unsigned)res);
        return 0;
    }

    FSIZE_t sz = f_size(&fil);
    if (sz == 0u || sz + 1u > buf_size) {
        (void)f_close(&fil);
        set_error("JSON file is empty or too large");
        return 0;
    }

    UINT br = 0u;
    res = f_read(&fil, buf, (UINT)sz, &br);
    (void)f_close(&fil);

    if (res != FR_OK || br != (UINT)sz) {
        set_error("Failed to read JSON (r%u)", (unsigned)res);
        return 0;
    }

    buf[br] = '\0';
    if (out_len) {
        *out_len = (uint32_t)br;
    }

    return 1;
}

static int wifi_wait_for_at(uint32_t timeout_ms,
                            uint32_t attempts,
                            char *response,
                            uint32_t response_size)
{
    for (uint32_t i = 0u; i < attempts; i++) {
        if (esp8266_at_command("AT", "OK", timeout_ms, response, response_size) == 0) {
            return 0;
        }
        HAL_Delay(120u);
    }

    return -1;
}

static const char *wifi_pinmap_name(esp8266_uart_pinmap_t pinmap)
{
    (void)pinmap;
    return "PC12/PD2";
}

static int wifi_uart_open(uint32_t baud)
{
    esp8266_uart_deinit();
    if (esp8266_uart_init(baud) != 0) {
        return -1;
    }

    s_uart_pinmap = esp8266_uart_get_pinmap();
    s_uart_baud = baud;
    HAL_Delay(WIFI_ESP_REINIT_SETTLE_MS);
    esp8266_at_drain(15u);

    return 0;
}

static int wifi_bootstrap_module(char *response, uint32_t response_size)
{
    esp8266_uart_set_pinmap(ESP8266_UART_PINMAP_PRIMARY);
    s_uart_pinmap = esp8266_uart_get_pinmap();

    esp8266_uart_ctrl_prepare(WIFI_ESP_RESET_LOW_MS);
    HAL_Delay(WIFI_ESP_BOOT_SETTLE_MS);

    for (uint32_t i = 0u; i < (uint32_t)(sizeof(s_uart_probe_bauds) / sizeof(s_uart_probe_bauds[0])); i++) {
        uint32_t baud = s_uart_probe_bauds[i];

        if (wifi_uart_open(baud) != 0) {
            continue;
        }

        if (wifi_wait_for_at(WIFI_ESP_AT_TIMEOUT_MS,
                             WIFI_ESP_AT_RETRIES,
                             response,
                             response_size) == 0) {
            return 0;
        }
    }

    return -1;
}

static int wifi_resync_module(char *response, uint32_t response_size)
{
    if (wifi_wait_for_at(WIFI_ESP_RESYNC_TIMEOUT_MS,
                         WIFI_ESP_RESYNC_RETRIES,
                         response,
                         response_size) == 0) {
        return 0;
    }

    return wifi_bootstrap_module(response, response_size);
}

static int wifi_run_command(const char *cmd,
                            const char *expect,
                            uint32_t timeout_ms,
                            char *response,
                            uint32_t response_size,
                            const char *error_text)
{
    if (esp8266_at_command(cmd, expect, timeout_ms, response, response_size) == 0) {
        return 0;
    }

    set_error_with_response(error_text, response);
    return -1;
}

static int wifi_configure_station(char *response, uint32_t response_size)
{
    if (wifi_run_command("ATE0",
                         "OK",
                         1000u,
                         response,
                         response_size,
                         "ATE0 command failed") != 0) {
        return -1;
    }

    if (wifi_run_command("AT+CWMODE=1",
                         "OK",
                         3000u,
                         response,
                         response_size,
                         "CWMODE command failed") != 0) {
        return -1;
    }

    if (wifi_run_command("AT+CIPMUX=0",
                         "OK",
                         2000u,
                         response,
                         response_size,
                         "CIPMUX command failed") != 0) {
        return -1;
    }

    return 0;
}

int wifi_service_init(void)
{
    if (s_inited) {
        return 0;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    clear_error();
    s_uart_baud = 0u;
    s_uart_pinmap = ESP8266_UART_PINMAP_PRIMARY;

    (void)wifi_service_load_config();

    char resp[256];
    if (wifi_bootstrap_module(resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        if (resp[0] != '\0') {
            set_error("ESP8266 no AT response (last rx: %.40s)", resp);
        } else if (esp8266_uart_ctrl_is_configured() && s_uart_baud != 0u) {
            set_error("ESP8266 no AT response (%s, %lu baud; CN2 SB5/SB6 OFF, SB8 ON)",
                      wifi_pinmap_name(s_uart_pinmap),
                      (unsigned long)s_uart_baud);
        } else if (esp8266_uart_ctrl_is_configured()) {
            set_error("ESP8266 no AT response (PC12/PD2; CN2 SB5/SB6 OFF, SB8 ON)");
        } else {
            set_error("ESP8266 no AT response (ctrl pins off; CN2 SB5/SB6 OFF, SB8 ON)");
        }
        return -1;
    }

    if (wifi_configure_station(resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        return -1;
    }

    s_inited = 1u;
    s_state = WIFI_STATE_DISCONNECTED;
    clear_error();

    return 0;
}

void wifi_service_get_config(wifi_service_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    *cfg = s_cfg;
}

void wifi_service_set_config(const wifi_service_config_t *cfg)
{
    if (!cfg) {
        return;
    }

    safe_copy(s_cfg.ssid, WIFI_SSID_MAX, cfg->ssid);
    safe_copy(s_cfg.password, WIFI_PASSWORD_MAX, cfg->password);
    safe_copy(s_cfg.server, WIFI_SERVER_MAX, cfg->server);
}

int wifi_service_load_config(void)
{
    FIL fil;
    FRESULT res = f_open(&fil, WIFI_CONFIG_PATH, FA_READ);
    if (res != FR_OK) {
        return -1;
    }

    char json[512];
    UINT br = 0u;
    res = f_read(&fil, json, sizeof(json) - 1u, &br);
    (void)f_close(&fil);

    if (res != FR_OK || br == 0u) {
        return -1;
    }

    json[br] = '\0';

    (void)extract_json_string(json, "ssid", s_cfg.ssid, WIFI_SSID_MAX);
    (void)extract_json_string(json, "password", s_cfg.password, WIFI_PASSWORD_MAX);
    (void)extract_json_string(json, "server", s_cfg.server, WIFI_SERVER_MAX);

    return 0;
}

int wifi_service_save_config(void)
{
    char *json_buf = (char *)EIT_SDRAM_JSON_BUF_ADDR;
    json_encoder_t enc;

    json_init(&enc, json_buf, EIT_SDRAM_JSON_BUF_SIZE);
    json_object_start(&enc);
    json_key_string(&enc, "ssid", s_cfg.ssid);
    json_key_string(&enc, "password", s_cfg.password);
    json_key_string(&enc, "server", s_cfg.server);
    json_object_end(&enc);

    if (json_has_error(&enc)) {
        set_error("Insufficient buffer to save Wi-Fi config");
        return -1;
    }

    FIL fil;
    FRESULT res = f_open(&fil, WIFI_CONFIG_PATH, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        set_error("Failed to open wifi_settings.json (r%u)", (unsigned)res);
        return -1;
    }

    UINT bw = 0u;
    size_t len = json_get_length(&enc);
    res = f_write(&fil, json_buf, (UINT)len, &bw);
    (void)f_close(&fil);

    if (res != FR_OK || bw != (UINT)len) {
        set_error("Failed to write wifi_settings.json (r%u)", (unsigned)res);
        return -1;
    }

    return 0;
}

wifi_service_state_t wifi_service_get_state(void)
{
    return s_state;
}

const char *wifi_service_last_error(void)
{
    return s_last_error;
}

int wifi_service_connect(void)
{
    if (!s_inited && wifi_service_init() != 0) {
        return -1;
    }

    if (s_state == WIFI_STATE_CONNECTED) {
        return 0;
    }

    if (s_cfg.ssid[0] == '\0') {
        s_state = WIFI_STATE_ERROR;
        set_error("SSID is not configured");
        return -1;
    }

    s_state = WIFI_STATE_CONNECTING;
    clear_error();

    char resp[384];
    if (wifi_resync_module(resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        if (s_uart_baud != 0u) {
            set_error("ESP8266 not responding (%s, %lu baud)",
                      wifi_pinmap_name(s_uart_pinmap),
                      (unsigned long)s_uart_baud);
        } else {
            set_error("ESP8266 not responding");
        }
        return -1;
    }

    if (wifi_configure_station(resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        return -1;
    }

    char cmd[256];
    if (!build_join_command(cmd, sizeof(cmd))) {
        s_state = WIFI_STATE_ERROR;
        set_error("Invalid Wi-Fi parameters");
        return -1;
    }

    if (wifi_run_command(cmd,
                         "OK",
                         WIFI_ESP_JOIN_TIMEOUT_MS,
                         resp,
                         sizeof(resp),
                         "Failed to connect to AP") != 0) {
        s_state = WIFI_STATE_ERROR;
        return -1;
    }

    if (wifi_run_command("AT+CIPMUX=0",
                         "OK",
                         2000u,
                         resp,
                         sizeof(resp),
                         "CIPMUX command failed") != 0) {
        s_state = WIFI_STATE_ERROR;
        return -1;
    }

    s_state = WIFI_STATE_CONNECTED;
    clear_error();
    return 0;
}

int wifi_service_send_json_file(const char *json_path,
                                char *server_reply,
                                uint32_t server_reply_size)
{
    if (!json_path || json_path[0] == '\0') {
        set_error("Invalid JSON file");
        return -1;
    }

    if (s_cfg.server[0] == '\0') {
        set_error("Server is not configured");
        return -1;
    }

    if (s_state != WIFI_STATE_CONNECTED) {
        if (wifi_service_connect() != 0) {
            return -1;
        }
    }

    char host[96];
    char path[96];
    uint16_t port = 80u;
    if (!parse_server_url(s_cfg.server, host, sizeof(host), &port, path, sizeof(path))) {
        return -1;
    }

    char *payload = (char *)EIT_SDRAM_JSON_BUF_ADDR;
    uint32_t payload_len = 0u;
    if (!load_file_to_buffer(json_path, payload, EIT_SDRAM_JSON_BUF_SIZE, &payload_len)) {
        return -1;
    }

    char request_hdr[256];
    int hdr_len = snprintf(request_hdr,
                           sizeof(request_hdr),
                           "POST %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Connection: close\r\n"
                           "Content-Length: %lu\r\n\r\n",
                           path,
                           host,
                           (unsigned long)payload_len);

    if (hdr_len <= 0 || hdr_len >= (int)sizeof(request_hdr)) {
        set_error("HTTP header exceeds limit");
        return -1;
    }

    char resp[640];
    (void)esp8266_at_command("AT+CIPCLOSE", "OK", 1000u, resp, sizeof(resp));

    char cmd[196];
    int cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, (unsigned)port);
    if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) {
        set_error("CIPSTART command is too long");
        return -1;
    }

    if (wifi_run_command(cmd,
                         "OK",
                         10000u,
                         resp,
                         sizeof(resp),
                         "CIPSTART command failed") != 0) {
        return -1;
    }

    uint32_t total_len = (uint32_t)hdr_len + payload_len;
    cmd_len = snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%lu", (unsigned long)total_len);
    if (cmd_len <= 0 || cmd_len >= (int)sizeof(cmd)) {
        set_error("CIPSEND command is too long");
        return -1;
    }

    if (wifi_run_command(cmd,
                         ">",
                         4000u,
                         resp,
                         sizeof(resp),
                         "CIPSEND command failed") != 0) {
        return -1;
    }

    if (esp8266_at_write_raw((const uint8_t *)request_hdr, (uint32_t)hdr_len, 3000u) != 0) {
        set_error("Failed sending HTTP header");
        return -1;
    }

    if (esp8266_at_write_raw((const uint8_t *)payload, payload_len, 15000u) != 0) {
        set_error("Failed sending JSON payload");
        return -1;
    }

    if (esp8266_at_wait("SEND OK", 12000u, resp, sizeof(resp)) != 0) {
        set_error_with_response("No SEND OK confirmation", resp);
        return -1;
    }

    if (server_reply && server_reply_size > 0u) {
        server_reply[0] = '\0';
        (void)esp8266_at_wait("+IPD", 3000u, server_reply, server_reply_size);
    }

    (void)esp8266_at_command("AT+CIPCLOSE", "OK", 1000u, resp, sizeof(resp));

    s_state = WIFI_STATE_CONNECTED;
    clear_error();
    return 0;
}
