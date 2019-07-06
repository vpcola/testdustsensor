// Copyright 2015-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_types.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/uart.h"

ESP_EVENT_DECLARE_BASE(ESP_DUSTSENSOR_EVENT);

typedef struct {
    uint16_t pm1;
    uint16_t pm25;
	uint16_t pm10;
	uint16_t pm1_atmospheric;
	uint16_t pm25_atmospheric;
	uint16_t pm10_atmospheric;
} dustsensor_t;

typedef struct {
    struct {
        uart_port_t uart_port;        /*!< UART port number */
        uint32_t rx_pin;              /*!< UART Rx Pin number */
        uint32_t baud_rate;           /*!< UART baud rate */
        uart_word_length_t data_bits; /*!< UART data bits length */
        uart_parity_t parity;         /*!< UART parity */
        uart_stop_bits_t stop_bits;   /*!< UART stop bits length */
        uint32_t event_queue_size;    /*!< UART event queue size */
    } uart;                           /*!< UART specific configuration */
} dustsensor_parser_config_t;

typedef void *dustsensor_parser_handle_t;

#define DUSTSENSOR_PARSER_CONFIG_DEFAULT()       \
    {                                      \
        .uart = {                          \
            .uart_port = UART_NUM_1,       \
            .rx_pin = 2,                   \
            .baud_rate = 9600,             \
            .data_bits = UART_DATA_8_BITS, \
            .parity = UART_PARITY_DISABLE, \
            .stop_bits = UART_STOP_BITS_1, \
            .event_queue_size = 16         \
        }                                  \
    }

typedef enum {
    SENSOR_UPDATE, 
    SENSOR_UNKNOWN 
} dustsensor_event_id_t;

dustsensor_parser_handle_t dustsensor_parser_init(const dustsensor_parser_config_t *config);

esp_err_t dustsensor_parser_deinit(dustsensor_parser_handle_t dustsensor_hdl);
esp_err_t dustsensor_parser_add_handler(dustsensor_parser_handle_t dustsensor_hdl, esp_event_handler_t event_handler, void *handler_args);
esp_err_t dustsensor_parser_remove_handler(dustsensor_parser_handle_t dustsensor_hdl, esp_event_handler_t event_handler);


#ifdef __cplusplus
}
#endif
