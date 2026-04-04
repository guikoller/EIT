#include "esp8266_uart.h"

#include "stm32f7xx_hal.h"

#include <stdio.h>

#ifndef ESP8266_UART_INSTANCE
#define ESP8266_UART_INSTANCE UART5
#endif

#ifndef ESP8266_UART_CLK_ENABLE
#define ESP8266_UART_CLK_ENABLE() __HAL_RCC_UART5_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_TX_GPIO_CLK_ENABLE
#define ESP8266_UART_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_RX_GPIO_CLK_ENABLE
#define ESP8266_UART_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_TX_PORT
#define ESP8266_UART_TX_PORT GPIOC
#endif

#ifndef ESP8266_UART_TX_PIN
#define ESP8266_UART_TX_PIN GPIO_PIN_12
#endif

#ifndef ESP8266_UART_RX_PORT
#define ESP8266_UART_RX_PORT GPIOD
#endif

#ifndef ESP8266_UART_RX_PIN
#define ESP8266_UART_RX_PIN GPIO_PIN_2
#endif

#ifndef ESP8266_UART_AF
#define ESP8266_UART_AF GPIO_AF8_UART5
#endif

#ifndef ESP8266_UART_ENABLE_ALT_PINMAP
#define ESP8266_UART_ENABLE_ALT_PINMAP 0
#endif

#if ESP8266_UART_ENABLE_ALT_PINMAP
#ifndef ESP8266_UART_ALT_TX_GPIO_CLK_ENABLE
#define ESP8266_UART_ALT_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_ALT_RX_GPIO_CLK_ENABLE
#define ESP8266_UART_ALT_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#endif

#ifndef ESP8266_UART_ALT_TX_PORT
#define ESP8266_UART_ALT_TX_PORT GPIOG
#endif

#ifndef ESP8266_UART_ALT_TX_PIN
#define ESP8266_UART_ALT_TX_PIN GPIO_PIN_14
#endif

#ifndef ESP8266_UART_ALT_RX_PORT
#define ESP8266_UART_ALT_RX_PORT GPIOG
#endif

#ifndef ESP8266_UART_ALT_RX_PIN
#define ESP8266_UART_ALT_RX_PIN GPIO_PIN_9
#endif

#ifndef ESP8266_UART_ALT_AF
#define ESP8266_UART_ALT_AF GPIO_AF8_USART6
#endif
#endif

#ifndef ESP8266_UART_RX_BUFFER_SIZE
#define ESP8266_UART_RX_BUFFER_SIZE 2048u
#endif

#ifndef ESP8266_USE_CTRL_PINS
#define ESP8266_USE_CTRL_PINS 0
#endif

#ifndef ESP8266_USE_RST_PIN
#define ESP8266_USE_RST_PIN 1
#endif

#ifndef ESP8266_RST_GPIO_CLK_ENABLE
#define ESP8266_RST_GPIO_CLK_ENABLE() __HAL_RCC_GPIOJ_CLK_ENABLE()
#endif

#ifndef ESP8266_RST_PORT
#define ESP8266_RST_PORT GPIOJ
#endif

#ifndef ESP8266_RST_PIN
#define ESP8266_RST_PIN GPIO_PIN_14
#endif

#ifndef ESP8266_RST_STABILIZE_MS
#define ESP8266_RST_STABILIZE_MS 500u
#endif

#ifndef ESP8266_USE_RST2_PIN
#define ESP8266_USE_RST2_PIN 0
#endif

#ifndef ESP8266_RST2_GPIO_CLK_ENABLE
#define ESP8266_RST2_GPIO_CLK_ENABLE() __HAL_RCC_GPIOI_CLK_ENABLE()
#endif

#ifndef ESP8266_RST2_PORT
#define ESP8266_RST2_PORT GPIOI
#endif

#ifndef ESP8266_RST2_PIN
#define ESP8266_RST2_PIN GPIO_PIN_14
#endif

#if ESP8266_USE_CTRL_PINS
#ifndef ESP8266_CH_PD_PORT
#error "ESP8266_CH_PD_PORT must be defined when ESP8266_USE_CTRL_PINS=1"
#endif

#ifndef ESP8266_CH_PD_PIN
#error "ESP8266_CH_PD_PIN must be defined when ESP8266_USE_CTRL_PINS=1"
#endif

#ifndef ESP8266_GPIO0_PORT
#error "ESP8266_GPIO0_PORT must be defined when ESP8266_USE_CTRL_PINS=1"
#endif

#ifndef ESP8266_GPIO0_PIN
#error "ESP8266_GPIO0_PIN must be defined when ESP8266_USE_CTRL_PINS=1"
#endif

#ifndef ESP8266_GPIO2_PORT
#error "ESP8266_GPIO2_PORT must be defined when ESP8266_USE_CTRL_PINS=1"
#endif

#ifndef ESP8266_GPIO2_PIN
#error "ESP8266_GPIO2_PIN must be defined when ESP8266_USE_CTRL_PINS=1"
#endif
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

#ifndef ESP8266_DEBUG_UART_ENABLE
#define ESP8266_DEBUG_UART_ENABLE 1
#endif

#ifndef ESP8266_DEBUG_UART_HEX_RX
#define ESP8266_DEBUG_UART_HEX_RX 1
#endif

#if ESP8266_DEBUG_UART_ENABLE
#ifndef ESP8266_DEBUG_UART_INSTANCE
#define ESP8266_DEBUG_UART_INSTANCE USART1
#endif

#ifndef ESP8266_DEBUG_UART_CLK_ENABLE
#define ESP8266_DEBUG_UART_CLK_ENABLE() __HAL_RCC_USART1_CLK_ENABLE()
#endif

#ifndef ESP8266_DEBUG_UART_TX_GPIO_CLK_ENABLE
#define ESP8266_DEBUG_UART_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#endif

#ifndef ESP8266_DEBUG_UART_RX_GPIO_CLK_ENABLE
#define ESP8266_DEBUG_UART_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#endif

#ifndef ESP8266_DEBUG_UART_TX_PORT
#define ESP8266_DEBUG_UART_TX_PORT GPIOA
#endif

#ifndef ESP8266_DEBUG_UART_TX_PIN
#define ESP8266_DEBUG_UART_TX_PIN GPIO_PIN_9
#endif

#ifndef ESP8266_DEBUG_UART_RX_PORT
#define ESP8266_DEBUG_UART_RX_PORT GPIOA
#endif

#ifndef ESP8266_DEBUG_UART_RX_PIN
#define ESP8266_DEBUG_UART_RX_PIN GPIO_PIN_10
#endif

#ifndef ESP8266_DEBUG_UART_AF
#define ESP8266_DEBUG_UART_AF GPIO_AF7_USART1
#endif

#ifndef ESP8266_DEBUG_UART_BAUD
#define ESP8266_DEBUG_UART_BAUD 115200u
#endif
#endif

typedef struct {
    uint8_t data[ESP8266_UART_RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} esp8266_uart_rx_ring_t;

static UART_HandleTypeDef s_huart;
static uint8_t s_uart_ready = 0u;
static uint8_t s_rx_it_ready = 0u;
static esp8266_uart_pinmap_t s_pinmap = ESP8266_UART_PINMAP_PRIMARY;

static esp8266_uart_rx_ring_t s_rx_ring;
static uint8_t s_reset_ready = 0u;

#if ESP8266_USE_CTRL_PINS
static uint8_t s_ctrl_ready = 0u;
#endif

#if ESP8266_DEBUG_UART_ENABLE
static UART_HandleTypeDef s_dbg_uart;
static uint8_t s_dbg_ready = 0u;
#if ESP8266_DEBUG_UART_HEX_RX
static uint8_t s_dbg_rx_line_open = 0u;
#endif
#endif

#if ESP8266_DEBUG_UART_ENABLE
static void esp8266_uart_debug_init(void)
{
    if (s_dbg_ready) {
        return;
    }

    GPIO_InitTypeDef gpio;

    ESP8266_DEBUG_UART_TX_GPIO_CLK_ENABLE();
    ESP8266_DEBUG_UART_RX_GPIO_CLK_ENABLE();
    ESP8266_DEBUG_UART_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = ESP8266_DEBUG_UART_AF;

    if (ESP8266_DEBUG_UART_TX_PORT == ESP8266_DEBUG_UART_RX_PORT) {
        gpio.Pin = ESP8266_DEBUG_UART_TX_PIN | ESP8266_DEBUG_UART_RX_PIN;
        HAL_GPIO_Init(ESP8266_DEBUG_UART_TX_PORT, &gpio);
    } else {
        gpio.Pin = ESP8266_DEBUG_UART_TX_PIN;
        HAL_GPIO_Init(ESP8266_DEBUG_UART_TX_PORT, &gpio);

        gpio.Pin = ESP8266_DEBUG_UART_RX_PIN;
        HAL_GPIO_Init(ESP8266_DEBUG_UART_RX_PORT, &gpio);
    }

    s_dbg_uart.Instance = ESP8266_DEBUG_UART_INSTANCE;
    s_dbg_uart.Init.BaudRate = ESP8266_DEBUG_UART_BAUD;
    s_dbg_uart.Init.WordLength = UART_WORDLENGTH_8B;
    s_dbg_uart.Init.StopBits = UART_STOPBITS_1;
    s_dbg_uart.Init.Parity = UART_PARITY_NONE;
    s_dbg_uart.Init.Mode = UART_MODE_TX_RX;
    s_dbg_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_dbg_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&s_dbg_uart) == HAL_OK) {
        s_dbg_ready = 1u;
    }
}

static void esp8266_uart_debug_write_byte(uint8_t byte)
{
    if (!s_dbg_ready) {
        esp8266_uart_debug_init();
    }

    if (!s_dbg_ready) {
        return;
    }

    (void)HAL_UART_Transmit(&s_dbg_uart, &byte, 1u, 5u);
}

#if ESP8266_DEBUG_UART_HEX_RX
static void esp8266_uart_debug_write_hex(uint8_t byte)
{
    static const char hex[] = "0123456789ABCDEF";
    char out[4];

    out[0] = hex[(byte >> 4) & 0x0F];
    out[1] = hex[byte & 0x0F];
    out[2] = ' ';
    out[3] = '\0';
    esp8266_uart_debug_write_str(out);
}
#endif
#endif

static int esp8266_uart_get_irqn(IRQn_Type *out_irq)
{
    if (!out_irq) {
        return 0;
    }

    /* NOTE: IRQn values are enum members, not #defines,
       so #if defined() cannot be used to guard them. */
    USART_TypeDef *inst = ESP8266_UART_INSTANCE;
    if (inst == UART5) {
        *out_irq = UART5_IRQn;
        return 1;
    }
    if (inst == USART6) {
        *out_irq = USART6_IRQn;
        return 1;
    }
    if (inst == USART3) {
        *out_irq = USART3_IRQn;
        return 1;
    }

    return 0;
}

static void esp8266_uart_irq_enable(void)
{
    IRQn_Type irq = (IRQn_Type)0;
    if (!esp8266_uart_get_irqn(&irq)) {
        return;
    }

    HAL_NVIC_SetPriority(irq, 0, 1);
    HAL_NVIC_EnableIRQ(irq);
}

static void esp8266_uart_irq_disable(void)
{
    IRQn_Type irq = (IRQn_Type)0;
    if (!esp8266_uart_get_irqn(&irq)) {
        return;
    }

    HAL_NVIC_DisableIRQ(irq);
}

static uint16_t esp8266_uart_rx_next(uint16_t index)
{
    return (uint16_t)((index + 1u) % ESP8266_UART_RX_BUFFER_SIZE);
}

static void esp8266_uart_rx_reset(void)
{
    s_rx_ring.head = 0u;
    s_rx_ring.tail = 0u;
}

static void esp8266_uart_start_rx_it(void)
{
    esp8266_uart_rx_reset();
    s_rx_it_ready = 0u;

    /* Retry arming RX interrupt a few times to avoid a stuck BUSY state. */
    for (uint32_t i = 0u; i < 3u; i++) {
        if (HAL_UART_Receive_IT(&s_huart, (uint8_t *)&s_rx_ring.data[s_rx_ring.tail], 1u) == HAL_OK) {
            s_rx_it_ready = 1u;
            break;
        }
        HAL_Delay(1u);
    }
}

static void esp8266_uart_rearm_rx_it_if_needed(void)
{
    if (!s_uart_ready || s_rx_it_ready) {
        return;
    }

    if (HAL_UART_Receive_IT(&s_huart, (uint8_t *)&s_rx_ring.data[s_rx_ring.tail], 1u) == HAL_OK) {
        s_rx_it_ready = 1u;
    }
}

static int esp8266_uart_rx_pop(uint8_t *out)
{
    if (s_rx_ring.head == s_rx_ring.tail) {
        return 0;
    }

    *out = s_rx_ring.data[s_rx_ring.head];
    s_rx_ring.head = esp8266_uart_rx_next(s_rx_ring.head);
    return 1;
}

int esp8266_uart_read_bytes_raw(uint8_t *buf, uint32_t max)
{
    if (!buf || !s_uart_ready || max == 0u) {
        return 0;
    }

    esp8266_uart_rearm_rx_it_if_needed();

    uint32_t count = 0u;
    while (count < max) {
        if (esp8266_uart_rx_pop(&buf[count]) == 0) {
            break;
        }
        count++;
    }
    return (int)count;
}

static void esp8266_uart_reset_init(void)
{
#if ESP8266_USE_RST_PIN
    if (s_reset_ready) {
        return;
    }

    GPIO_InitTypeDef gpio;

    ESP8266_RST_GPIO_CLK_ENABLE();
    gpio.Pin = ESP8266_RST_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = 0;
    HAL_GPIO_Init(ESP8266_RST_PORT, &gpio);

#if ESP8266_USE_RST2_PIN
    ESP8266_RST2_GPIO_CLK_ENABLE();
    gpio.Pin = ESP8266_RST2_PIN;
    HAL_GPIO_Init(ESP8266_RST2_PORT, &gpio);
#endif

    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, GPIO_PIN_SET);
#if ESP8266_USE_RST2_PIN
    HAL_GPIO_WritePin(ESP8266_RST2_PORT, ESP8266_RST2_PIN, GPIO_PIN_SET);
#endif

    HAL_Delay(ESP8266_RST_STABILIZE_MS);

    s_reset_ready = 1u;
#endif
}

static void esp8266_uart_reset_write(GPIO_PinState state)
{
#if ESP8266_USE_RST_PIN
    HAL_GPIO_WritePin(ESP8266_RST_PORT, ESP8266_RST_PIN, state);
#if ESP8266_USE_RST2_PIN
    HAL_GPIO_WritePin(ESP8266_RST2_PORT, ESP8266_RST2_PIN, state);
#endif
#else
    (void)state;
#endif
}

#if ESP8266_USE_CTRL_PINS
static void esp8266_uart_enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if (port == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    } else if (port == GPIOG) {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    } else if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    } else if (port == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    } else if (port == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    }
}

static void esp8266_uart_ctrl_init(void)
{
    if (s_ctrl_ready) {
        return;
    }

    esp8266_uart_enable_gpio_clock(ESP8266_CH_PD_PORT);
    esp8266_uart_enable_gpio_clock(ESP8266_GPIO0_PORT);
    esp8266_uart_enable_gpio_clock(ESP8266_GPIO2_PORT);
    esp8266_uart_reset_init();

    GPIO_InitTypeDef gpio;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = 0;

    gpio.Pin = ESP8266_CH_PD_PIN;
    HAL_GPIO_Init(ESP8266_CH_PD_PORT, &gpio);

    gpio.Pin = ESP8266_GPIO0_PIN;
    HAL_GPIO_Init(ESP8266_GPIO0_PORT, &gpio);

    gpio.Pin = ESP8266_GPIO2_PIN;
    HAL_GPIO_Init(ESP8266_GPIO2_PORT, &gpio);

    HAL_GPIO_WritePin(ESP8266_CH_PD_PORT, ESP8266_CH_PD_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ESP8266_GPIO0_PORT, ESP8266_GPIO0_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ESP8266_GPIO2_PORT, ESP8266_GPIO2_PIN, GPIO_PIN_SET);
    esp8266_uart_reset_write(GPIO_PIN_SET);

    s_ctrl_ready = 1u;
}
#endif

static void esp8266_uart_gpio_init(void)
{
    GPIO_TypeDef *tx_port = ESP8266_UART_TX_PORT;
    GPIO_TypeDef *rx_port = ESP8266_UART_RX_PORT;
    uint16_t tx_pin = ESP8266_UART_TX_PIN;
    uint16_t rx_pin = ESP8266_UART_RX_PIN;
    uint32_t uart_af = ESP8266_UART_AF;

    GPIO_InitTypeDef gpio;

#if ESP8266_UART_ENABLE_ALT_PINMAP
    if (s_pinmap == ESP8266_UART_PINMAP_ALT1) {
        ESP8266_UART_ALT_TX_GPIO_CLK_ENABLE();
        ESP8266_UART_ALT_RX_GPIO_CLK_ENABLE();
        tx_port = ESP8266_UART_ALT_TX_PORT;
        rx_port = ESP8266_UART_ALT_RX_PORT;
        tx_pin = ESP8266_UART_ALT_TX_PIN;
        rx_pin = ESP8266_UART_ALT_RX_PIN;
        uart_af = ESP8266_UART_ALT_AF;
    } else
#endif
    {
        ESP8266_UART_TX_GPIO_CLK_ENABLE();
        ESP8266_UART_RX_GPIO_CLK_ENABLE();
    }

    ESP8266_UART_CLK_ENABLE();

    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = uart_af;

    if (rx_port == tx_port) {
        gpio.Pin = (uint32_t)tx_pin | (uint32_t)rx_pin;
        HAL_GPIO_Init(tx_port, &gpio);
    } else {
        gpio.Pin = tx_pin;
        HAL_GPIO_Init(tx_port, &gpio);

        gpio.Pin = rx_pin;
        HAL_GPIO_Init(rx_port, &gpio);
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

void esp8266_uart_set_pinmap(esp8266_uart_pinmap_t pinmap)
{
    if (pinmap == ESP8266_UART_PINMAP_ALT1) {
#if ESP8266_UART_ENABLE_ALT_PINMAP
        s_pinmap = ESP8266_UART_PINMAP_ALT1;
#else
        s_pinmap = ESP8266_UART_PINMAP_PRIMARY;
#endif
        return;
    }

    s_pinmap = ESP8266_UART_PINMAP_PRIMARY;
}

esp8266_uart_pinmap_t esp8266_uart_get_pinmap(void)
{
    return s_pinmap;
}

void esp8266_uart_ctrl_prepare(uint32_t reset_low_ms)
{
#if ESP8266_USE_CTRL_PINS
    esp8266_uart_ctrl_init();
#elif ESP8266_USE_RST_PIN
    esp8266_uart_reset_init();
#endif

#if ESP8266_USE_RST_PIN
    if (reset_low_ms > 0u) {
        esp8266_uart_reset_write(GPIO_PIN_RESET);
        HAL_Delay(reset_low_ms);
        esp8266_uart_reset_write(GPIO_PIN_SET);
        HAL_Delay(ESP8266_RST_STABILIZE_MS);
    }
#else
    (void)reset_low_ms;
#endif
}

int esp8266_uart_ctrl_is_configured(void)
{
#if ESP8266_USE_CTRL_PINS || ESP8266_USE_RST_PIN
    return 1;
#else
    return 0;
#endif
}

void esp8266_uart_debug_write_str(const char *text)
{
#if ESP8266_DEBUG_UART_ENABLE
    if (!text) {
        return;
    }

    while (*text != '\0') {
        esp8266_uart_debug_write_byte((uint8_t)*text++);
    }
#else
    (void)text;
#endif
}

int esp8266_uart_init(uint32_t baudrate)
{
    if (s_uart_ready) {
        return 0;
    }

#if ESP8266_USE_CTRL_PINS
    esp8266_uart_ctrl_init();
#else
    esp8266_uart_reset_init();
#endif

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
    esp8266_uart_irq_enable();
    esp8266_uart_start_rx_it();

#if ESP8266_DEBUG_UART_ENABLE
    char msg[64];
    (void)snprintf(msg, sizeof(msg), "\r\n[ESP UART] baud=%lu\r\n", (unsigned long)baudrate);
    esp8266_uart_debug_write_str(msg);
#endif

    return 0;
}

void esp8266_uart_deinit(void)
{
    if (!s_uart_ready) {
        return;
    }

    s_rx_it_ready = 0u;
    esp8266_uart_irq_disable();
    esp8266_uart_rx_reset();

    (void)HAL_UART_DeInit(&s_huart);
    s_uart_ready = 0u;
}

int esp8266_uart_is_ready(void)
{
    return s_uart_ready ? 1 : 0;
}

void esp8266_uart_irq_handler(void)
{
    if (!s_uart_ready) {
        return;
    }

    HAL_UART_IRQHandler(&s_huart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (!huart || !s_uart_ready || !s_rx_it_ready || huart->Instance != ESP8266_UART_INSTANCE) {
        return;
    }

    uint16_t next = esp8266_uart_rx_next(s_rx_ring.tail);
    if (next == s_rx_ring.head) {
        s_rx_ring.head = esp8266_uart_rx_next(s_rx_ring.head);
    }

    s_rx_ring.tail = next;
    if (HAL_UART_Receive_IT(huart, (uint8_t *)&s_rx_ring.data[s_rx_ring.tail], 1u) != HAL_OK) {
        s_rx_it_ready = 0u;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (!huart || !s_uart_ready || huart->Instance != ESP8266_UART_INSTANCE) {
        return;
    }

    /* Defensively clear all error flags before restarting RX.
       ORE causes UART_EndRxTransfer() which disables RXNE/PE/ERR interrupts
       and sets RxState = READY. Without clearing, the restart may fail. */
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;

    if (HAL_UART_Receive_IT(huart, (uint8_t *)&s_rx_ring.data[s_rx_ring.tail], 1u) != HAL_OK) {
        s_rx_it_ready = 0u;
    } else {
        s_rx_it_ready = 1u;
    }
}

int esp8266_uart_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    if (!data || len == 0u || !s_uart_ready) {
        return -1;
    }

    esp8266_uart_rearm_rx_it_if_needed();

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

    uint32_t start = HAL_GetTick();
    while (1) {
        esp8266_uart_rearm_rx_it_if_needed();

        if (esp8266_uart_rx_pop(out) != 0) {
#if ESP8266_DEBUG_UART_ENABLE
#if ESP8266_DEBUG_UART_HEX_RX
            if (!s_dbg_rx_line_open) {
                esp8266_uart_debug_write_str("\r\n<< ");
                s_dbg_rx_line_open = 1u;
            }
            esp8266_uart_debug_write_hex(*out);
            if (*out == '\n') {
                s_dbg_rx_line_open = 0u;
            }
#else
            esp8266_uart_debug_write_byte(*out);
#endif
#endif
            return 1;
        }

        if (timeout_ms == 0u) {
            return 0;
        }

        if ((HAL_GetTick() - start) >= timeout_ms) {
            return 0;
        }
    }
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
