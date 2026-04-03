#ifndef ESP8266_AT_H
#define ESP8266_AT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESP8266_AT_MAX_LINE 192

typedef enum {
    ESP8266_AT_LINE_NONE = 0,
    ESP8266_AT_LINE_OK,
    ESP8266_AT_LINE_ERROR,
    ESP8266_AT_LINE_FAIL,
    ESP8266_AT_LINE_PROMPT,
    ESP8266_AT_LINE_SEND_OK,
    ESP8266_AT_LINE_WIFI_CONNECTED,
    ESP8266_AT_LINE_WIFI_GOT_IP,
    ESP8266_AT_LINE_OTHER,
} esp8266_at_line_type_t;

typedef struct {
    char line[ESP8266_AT_MAX_LINE];
    uint16_t len;
} esp8266_at_parser_t;

void esp8266_at_parser_reset(esp8266_at_parser_t *parser);
esp8266_at_line_type_t esp8266_at_parser_feed(esp8266_at_parser_t *parser, uint8_t byte, const char **out_line);

void esp8266_at_drain(uint32_t idle_timeout_ms);

int esp8266_at_command(const char *cmd,
                       const char *expect,
                       uint32_t timeout_ms,
                       char *response,
                       uint32_t response_size);

int esp8266_at_wait(const char *expect,
                    uint32_t timeout_ms,
                    char *response,
                    uint32_t response_size);

int esp8266_at_write_raw(const uint8_t *data, uint32_t len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_AT_H */
