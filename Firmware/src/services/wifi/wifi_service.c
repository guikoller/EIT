#include "wifi_service.h"

#include "esp8266_uart.h"
#include "esp8266_at.h"

#include "services/json_encoder.h"
#include "services/storage_service.h"
#include "eit_config.h"

#include "ff.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define WIFI_CONFIG_PATH "0:/wifi_settings.json"

static wifi_service_config_t s_cfg;
static uint8_t s_inited = 0u;
static wifi_service_state_t s_state = WIFI_STATE_OFFLINE;
static char s_last_error[96] = "";

static void set_error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(s_last_error, sizeof(s_last_error), fmt, args);
    va_end(args);
}

static void clear_error(void)
{
    s_last_error[0] = '\0';
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

    strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
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
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

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

static int build_wifi_cmd(char *out, uint32_t out_size,
                          const char *prefix,
                          const char *a,
                          const char *b)
{
    if (!out || out_size == 0u || !prefix || !a || !b) {
        return 0;
    }

    int n = snprintf(out, out_size, "%s\"%s\",\"%s\"", prefix, a, b);
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
        int pnum = atoi(colon + 1);
        if (pnum <= 0 || pnum > 65535) {
            set_error("Invalid server port");
            return 0;
        }
        *port = (uint16_t)pnum;
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

static int load_file_to_buffer(const char *path, char *buf, uint32_t buf_size, uint32_t *out_len)
{
    if (!path || !buf || buf_size == 0u) {
        return 0;
    }

    FIL fil;
    FRESULT res = f_open(&fil, path, FA_READ);
    if (res != FR_OK) {
        set_error("Failed to open %s (r%u)", path, (unsigned)res);
        return 0;
    }

    FSIZE_t sz = f_size(&fil);
    if (sz == 0 || sz + 1u > buf_size) {
        f_close(&fil);
        set_error("JSON file is empty or too large");
        return 0;
    }

    UINT br = 0;
    res = f_read(&fil, buf, (UINT)sz, &br);
    f_close(&fil);

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

int wifi_service_init(void)
{
    if (s_inited) {
        return 0;
    }

    memset(&s_cfg, 0, sizeof(s_cfg));
    clear_error();

    if (esp8266_uart_init(115200u) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("Failed to initialize ESP8266 UART");
        return -1;
    }

    s_inited = 1u;
    s_state = WIFI_STATE_DISCONNECTED;

    (void)wifi_service_load_config();

    char resp[256];
    if (esp8266_at_command("AT", "OK", 1500u, resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("ESP8266 not responding to AT");
        return -1;
    }

    (void)esp8266_at_command("ATE0", "OK", 1000u, resp, sizeof(resp));
    (void)esp8266_at_command("AT+CIPMUX=0", "OK", 1000u, resp, sizeof(resp));

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
    UINT br = 0;
    res = f_read(&fil, json, sizeof(json) - 1u, &br);
    f_close(&fil);

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

    UINT bw = 0;
    size_t len = json_get_length(&enc);
    res = f_write(&fil, json_buf, (UINT)len, &bw);
    f_close(&fil);

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

    if (s_cfg.ssid[0] == '\0') {
        s_state = WIFI_STATE_ERROR;
        set_error("SSID is not configured");
        return -1;
    }

    s_state = WIFI_STATE_CONNECTING;
    clear_error();

    char resp[384];
    if (esp8266_at_command("AT", "OK", 1500u, resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("ESP8266 not responding");
        return -1;
    }

    if (esp8266_at_command("ATE0", "OK", 1000u, resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("ATE0 command failed");
        return -1;
    }

    if (esp8266_at_command("AT+CWMODE=1", "OK", 3000u, resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("CWMODE command failed");
        return -1;
    }

    char cmd[256];
    if (!build_wifi_cmd(cmd, sizeof(cmd), "AT+CWJAP=", s_cfg.ssid, s_cfg.password)) {
        s_state = WIFI_STATE_ERROR;
        set_error("Invalid Wi-Fi parameters");
        return -1;
    }

    int rc = esp8266_at_command(cmd, "OK", 30000u, resp, sizeof(resp));
    if (rc != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("Failed to connect to AP");
        return -1;
    }

    if (esp8266_at_command("AT+CIPMUX=0", "OK", 2000u, resp, sizeof(resp)) != 0) {
        s_state = WIFI_STATE_ERROR;
        set_error("CIPMUX command failed");
        return -1;
    }

    s_state = WIFI_STATE_CONNECTED;
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
    int hdr_len = snprintf(request_hdr, sizeof(request_hdr),
                           "POST %s HTTP/1.1\r\n"
                           "Host: %s\r\n"
                           "Content-Type: application/json\r\n"
                           "Connection: close\r\n"
                           "Content-Length: %lu\r\n\r\n",
                           path, host, (unsigned long)payload_len);
    if (hdr_len <= 0 || hdr_len >= (int)sizeof(request_hdr)) {
        set_error("HTTP header exceeds limit");
        return -1;
    }

    char resp[512];
    (void)esp8266_at_command("AT+CIPCLOSE", "OK", 1000u, resp, sizeof(resp));

    char cmd[196];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, (unsigned)port);
    if (esp8266_at_command(cmd, "OK", 8000u, resp, sizeof(resp)) != 0) {
        set_error("CIPSTART command failed");
        return -1;
    }

    uint32_t total_len = (uint32_t)hdr_len + payload_len;
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%lu", (unsigned long)total_len);
    if (esp8266_at_command(cmd, ">", 3000u, resp, sizeof(resp)) != 0) {
        set_error("CIPSEND command failed");
        return -1;
    }

    if (esp8266_at_write_raw((const uint8_t *)request_hdr, (uint32_t)hdr_len, 2000u) != 0) {
        set_error("Failed sending HTTP header");
        return -1;
    }

    if (esp8266_at_write_raw((const uint8_t *)payload, payload_len, 10000u) != 0) {
        set_error("Failed sending JSON payload");
        return -1;
    }

    if (esp8266_at_wait("SEND OK", 10000u, resp, sizeof(resp)) != 0) {
        set_error("No SEND OK confirmation");
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
