#include "esp8266_uart.h"

#include "stm32f7xx_hal.h"

#ifndef ESP8266_UART_INSTANCE
#define ESP8266_UART_INSTANCE USART6
#endif

#ifndef ESP8266_UART_CLK_ENABLE
#define ESP8266_UART_CLK_ENABLE() __HAL_RCC_USART6_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_TX_GPIO_CLK_ENABLE
#define ESP8266_UART_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_RX_GPIO_CLK_ENABLE
#define ESP8266_UART_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_TX_PORT
#define ESP8266_UART_TX_PORT GPIOC
#endif

#ifndef ESP8266_UART_TX_PIN
#define ESP8266_UART_TX_PIN GPIO_PIN_6
#endif

#ifndef ESP8266_UART_RX_PORT
#define ESP8266_UART_RX_PORT GPIOC
#endif

#ifndef ESP8266_UART_RX_PIN
#define ESP8266_UART_RX_PIN GPIO_PIN_7
#endif

#ifndef ESP8266_UART_AF
#define ESP8266_UART_AF GPIO_AF8_USART6
#endif

#ifndef ESP8266_USE_EN_PIN
#define ESP8266_USE_EN_PIN 0
#endif

#if ESP8266_USE_EN_PIN
#ifndef ESP8266_EN_GPIO_CLK_ENABLE
#define ESP8266_EN_GPIO_CLK_ENABLE() __HAL_RCC_GPIOH_CLK_ENABLE()
#endif

#ifndef ESP8266_EN_PORT
#define ESP8266_EN_PORT GPIOH
#endif

#ifndef ESP8266_EN_PIN
#define ESP8266_EN_PIN GPIO_PIN_7
#endif
#endif

static UART_HandleTypeDef s_huart;
static uint8_t s_uart_ready = 0u;

static void esp8266_uart_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    ESP8266_UART_TX_GPIO_CLK_ENABLE();
    ESP8266_UART_RX_GPIO_CLK_ENABLE();
    ESP8266_UART_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = ESP8266_UART_AF;

    if (ESP8266_UART_RX_PORT == ESP8266_UART_TX_PORT) {
        gpio.Pin = ESP8266_UART_TX_PIN | ESP8266_UART_RX_PIN;
        HAL_GPIO_Init(ESP8266_UART_TX_PORT, &gpio);
    } else {
        gpio.Pin = ESP8266_UART_TX_PIN;
        HAL_GPIO_Init(ESP8266_UART_TX_PORT, &gpio);

        gpio.Pin = ESP8266_UART_RX_PIN;
        HAL_GPIO_Init(ESP8266_UART_RX_PORT, &gpio);
    }

#if ESP8266_USE_EN_PIN
    ESP8266_EN_GPIO_CLK_ENABLE();
    gpio.Pin = ESP8266_EN_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = 0;
    HAL_GPIO_Init(ESP8266_EN_PORT, &gpio);
    HAL_GPIO_WritePin(ESP8266_EN_PORT, ESP8266_EN_PIN, GPIO_PIN_SET);
#endif
}

int esp8266_uart_init(uint32_t baudrate)
{
    if (s_uart_ready) {
        return 0;
    }

    esp8266_uart_gpio_init();

    s_huart.Instance = ESP8266_UART_INSTANCE;
    s_huart.Init.BaudRate = baudrate;
    s_huart.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart.Init.StopBits = UART_STOPBITS_1;
    s_huart.Init.Parity = UART_PARITY_NONE;
    s_huart.Init.Mode = UART_MODE_TX_RX;
    s_huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&s_huart) != HAL_OK) {
        return -1;
    }

    s_uart_ready = 1u;
    return 0;
}

void esp8266_uart_deinit(void)
{
    if (!s_uart_ready) {
        return;
    }

    (void)HAL_UART_DeInit(&s_huart);
    s_uart_ready = 0u;
}

int esp8266_uart_is_ready(void)
{
    return s_uart_ready ? 1 : 0;
}

int esp8266_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!data || len == 0u || !s_uart_ready) {
        return -1;
    }

    if (HAL_UART_Transmit(&s_huart, (uint8_t *)data, len, timeout_ms) != HAL_OK) {
        return -1;
    }

    return 0;
}

int esp8266_uart_read_byte(uint8_t *out, uint32_t timeout_ms)
{
    if (!out || !s_uart_ready) {
        return -1;
    }

    HAL_StatusTypeDef st = HAL_UART_Receive(&s_huart, out, 1u, timeout_ms);
    if (st == HAL_OK) {
        return 1;
    }
    if (st == HAL_TIMEOUT) {
        return 0;
    }
    return -1;
}

void esp8266_uart_drain(uint32_t idle_timeout_ms)
{
    uint8_t byte;
    while (1) {
        int rc = esp8266_uart_read_byte(&byte, idle_timeout_ms);
        if (rc <= 0) {
            return;
        }
    }
}
