#ifndef ESP8266_UART_H
#define ESP8266_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	ESP8266_UART_PINMAP_PRIMARY = 0,
	ESP8266_UART_PINMAP_ALT1,
} esp8266_uart_pinmap_t;

void esp8266_uart_set_pinmap(esp8266_uart_pinmap_t pinmap);
esp8266_uart_pinmap_t esp8266_uart_get_pinmap(void);

void esp8266_uart_ctrl_prepare(uint32_t reset_low_ms);
int esp8266_uart_ctrl_is_configured(void);

void esp8266_uart_debug_write_str(const char *text);

int esp8266_uart_init(uint32_t baudrate);
void esp8266_uart_deinit(void);
int esp8266_uart_is_ready(void);

void esp8266_uart_irq_handler(void);

int esp8266_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int esp8266_uart_read_byte(uint8_t *out, uint32_t timeout_ms);
int esp8266_uart_read_bytes_raw(uint8_t *buf, uint32_t max);
void esp8266_uart_drain(uint32_t idle_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_UART_H */
