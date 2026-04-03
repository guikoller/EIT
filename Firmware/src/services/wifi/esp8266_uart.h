#ifndef ESP8266_UART_H
#define ESP8266_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int esp8266_uart_init(uint32_t baudrate);
void esp8266_uart_deinit(void);
int esp8266_uart_is_ready(void);

int esp8266_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);
int esp8266_uart_read_byte(uint8_t *out, uint32_t timeout_ms);
void esp8266_uart_drain(uint32_t idle_timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* ESP8266_UART_H */
