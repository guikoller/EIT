#include "esp8266_at.h"

#include "esp8266_uart.h"

#include "stm32f7xx_hal.h"

#include <string.h>

#ifndef ESP8266_DEBUG_LOG_TX
#define ESP8266_DEBUG_LOG_TX 1
#endif

static void response_init(char *response, uint32_t response_size, uint32_t *used)
{
    if (used) {
        *used = 0u;
    }

    if (response && response_size > 0u) {
        response[0] = '\0';
    }
}

static void response_append(char *response,
                            uint32_t response_size,
                            uint32_t *used,
                            char ch)
{
    if (!response || !used || response_size < 2u) {
        return;
    }

    if (*used + 1u >= response_size) {
        return;
    }

    response[*used] = ch;
    (*used)++;
    response[*used] = '\0';
}

static esp8266_at_line_type_t classify_line(const char *line)
{
    if (!line || line[0] == '\0') {
        return ESP8266_AT_LINE_NONE;
    }

    if (strcmp(line, "OK") == 0) {
        return ESP8266_AT_LINE_OK;
    }

    if (strstr(line, "ERROR") != NULL) {
        return ESP8266_AT_LINE_ERROR;
    }

    if (strstr(line, "FAIL") != NULL) {
        return ESP8266_AT_LINE_FAIL;
    }

    if (strstr(line, "SEND OK") != NULL) {
        return ESP8266_AT_LINE_SEND_OK;
    }

    if (strstr(line, "WIFI CONNECTED") != NULL) {
        return ESP8266_AT_LINE_WIFI_CONNECTED;
    }

    if (strstr(line, "WIFI GOT IP") != NULL) {
        return ESP8266_AT_LINE_WIFI_GOT_IP;
    }

    return ESP8266_AT_LINE_OTHER;
}

static int expect_matches(const char *expect,
                          esp8266_at_line_type_t line_type,
                          const char *line,
                          const char *response)
{
    if (!expect || expect[0] == '\0') {
        return (line_type == ESP8266_AT_LINE_OK) ? 1 : 0;
    }

    if (expect[0] == '>' && expect[1] == '\0') {
        return (line_type == ESP8266_AT_LINE_PROMPT) ? 1 : 0;
    }

    if (response && strstr(response, expect) != NULL) {
        return 1;
    }

    if (line && strstr(line, expect) != NULL) {
        return 1;
    }

    if (strcmp(expect, "OK") == 0 && line_type == ESP8266_AT_LINE_OK) {
        return 1;
    }

    if (strcmp(expect, "SEND OK") == 0 && line_type == ESP8266_AT_LINE_SEND_OK) {
        return 1;
    }

    return 0;
}

void esp8266_at_parser_reset(esp8266_at_parser_t *parser)
{
    if (!parser) {
        return;
    }

    parser->len = 0u;
    parser->line[0] = '\0';
}

esp8266_at_line_type_t esp8266_at_parser_feed(esp8266_at_parser_t *parser,
                                               uint8_t byte,
                                               const char **out_line)
{
    if (out_line) {
        *out_line = NULL;
    }

    if (!parser) {
        return ESP8266_AT_LINE_NONE;
    }

    if (byte == '>') {
        parser->line[0] = '>';
        parser->line[1] = '\0';
        if (out_line) {
            *out_line = parser->line;
        }
        parser->len = 0u;
        parser->line[0] = '\0';
        return ESP8266_AT_LINE_PROMPT;
    }

    if (byte == '\r') {
        return ESP8266_AT_LINE_NONE;
    }

    if (byte == '\n') {
        if (parser->len == 0u) {
            return ESP8266_AT_LINE_NONE;
        }

        parser->line[parser->len] = '\0';
        if (out_line) {
            *out_line = parser->line;
        }

        esp8266_at_line_type_t t = classify_line(parser->line);
        parser->len = 0u;
        parser->line[0] = '\0';
        return t;
    }

    if (parser->len + 1u < ESP8266_AT_MAX_LINE) {
        parser->line[parser->len++] = (char)byte;
        parser->line[parser->len] = '\0';
    }

    return ESP8266_AT_LINE_NONE;
}

void esp8266_at_drain(uint32_t idle_timeout_ms)
{
    esp8266_uart_drain(idle_timeout_ms);
}

static int at_wait_impl(const char *expect,
                        uint32_t timeout_ms,
                        char *response,
                        uint32_t response_size)
{
    esp8266_at_parser_t parser;
    esp8266_at_parser_reset(&parser);

    uint32_t used = 0u;
    response_init(response, response_size, &used);

    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        uint8_t byte = 0u;
        int rd = esp8266_uart_read_byte(&byte, 20u);
        if (rd < 0) {
            return -1;
        }

        if (rd == 0) {
            continue;
        }

        response_append(response, response_size, &used, (char)byte);

        const char *line = NULL;
        esp8266_at_line_type_t t = esp8266_at_parser_feed(&parser, byte, &line);

        if (t == ESP8266_AT_LINE_ERROR || t == ESP8266_AT_LINE_FAIL) {
            return -2;
        }

        if (expect_matches(expect, t, line, response)) {
            return 0;
        }
    }

    return -1;
}

int esp8266_at_command(const char *cmd,
                       const char *expect,
                       uint32_t timeout_ms,
                       char *response,
                       uint32_t response_size)
{
    if (!cmd || cmd[0] == '\0') {
        return -1;
    }

    if (!esp8266_uart_is_ready()) {
        return -1;
    }

    esp8266_at_drain(10u);

#if ESP8266_DEBUG_LOG_TX
    esp8266_uart_debug_write_str("\r\n>> ");
    esp8266_uart_debug_write_str(cmd);
    esp8266_uart_debug_write_str("\r\n");
#endif

    uint32_t cmd_len = (uint32_t)strlen(cmd);
    if (esp8266_uart_write((const uint8_t *)cmd, (uint16_t)cmd_len, 1000u) != 0) {
        return -1;
    }

    if (cmd_len < 2u || cmd[cmd_len - 2u] != '\r' || cmd[cmd_len - 1u] != '\n') {
        static const uint8_t crlf[2] = {'\r', '\n'};
        if (esp8266_uart_write(crlf, 2u, 200u) != 0) {
            return -1;
        }
    }

    return at_wait_impl(expect, timeout_ms, response, response_size);
}

int esp8266_at_wait(const char *expect,
                    uint32_t timeout_ms,
                    char *response,
                    uint32_t response_size)
{
    if (!esp8266_uart_is_ready()) {
        return -1;
    }

    return at_wait_impl(expect, timeout_ms, response, response_size);
}

int esp8266_at_write_raw(const uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    if (!data || len == 0u || !esp8266_uart_is_ready()) {
        return -1;
    }

    if (len > 65535u) {
        return -1;
    }

    return esp8266_uart_write(data, (uint16_t)len, timeout_ms);
}
