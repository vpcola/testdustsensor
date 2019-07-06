#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "dustsensor_parser.h"

#define DUSTSENSOR_PARSER_RING_BUFFER_SIZE   CONFIG_DUSTSENSOR_PARSER_RING_BUFFER_SIZE
#define DUSTSENSOR_PARSER_RUNTIME_BUFFER_SIZE  (DUSTSENSOR_PARSER_RING_BUFFER_SIZE / 2)
#define DUSTSENSOR_EVENT_LOOP_QUEUE_SIZE (16)
#define DUSTSENSOR_PARSER_TASK_STACK_SIZE      CONFIG_DUSTSENSOR_PARSER_TASK_STACK_SIZE
#define DUSTSENSOR_PARSER_TASK_PRIORITY  CONFIG_DUSTSENSOR_PARSER_TASK_PRIORITY


ESP_EVENT_DEFINE_BASE(ESP_DUSTSENSOR_EVENT);

static const char *DUSTSENSOR_TAG = "dustsensor_parser";

typedef struct {
    dustsensor_t parent;                           /*!< Parent class */
    uart_port_t uart_port;                         /*!< Uart port number */
    uint8_t *buffer;                               /*!< Runtime buffer */
    esp_event_loop_handle_t event_loop_hdl;        /*!< Event loop handle */
    TaskHandle_t tsk_hdl;                          /*!< NMEA Parser task handle */
    QueueHandle_t event_queue;                     /*!< UART event queue handle */
} esp_dustsensor_t;

static esp_err_t dustsensor_decode(esp_dustsensor_t *esp_dustsensor, size_t len)
{
    const uint8_t *d = esp_dustsensor->buffer;
	int i = 0;

	// Check we have 'BM' as the header
	if ((d[0] == 0x42) && (d[1] == 0x4D))
	{
		uint16_t checksum = 0;
		// Check the frame length
		if (len != 32) return ESP_ERR_INVALID_SIZE; // Data should be 32 bytes in length

		// Frame size should be 28 bytes!
		// uint16_t framesiz = (((uint16_t) d[2]) << 8 ) | (d[3] & 0xFF);

		// total all 30 bytes for checksum
		for(i = 0; i < 30; i++)
			checksum += d[i];

		uint16_t chkdata = (d[30] << 8) | (d[31] & 0xFF);

		if (checksum == chkdata)
		{
			esp_dustsensor->parent.pm1 = (d[4] << 8) | (d[5] & 0xFF);
			esp_dustsensor->parent.pm25 = (d[6] << 8) | (d[7] & 0xFF);
			esp_dustsensor->parent.pm10 = (d[8] << 8) | (d[9] & 0xFF);
		    esp_dustsensor->parent.pm1_atmospheric = (d[10] << 8) | (d[11] & 0xFF);
			esp_dustsensor->parent.pm25_atmospheric = (d[12] << 8) | (d[13] & 0xFF);
			esp_dustsensor->parent.pm10_atmospheric = (d[14] << 8) | (d[15] & 0xFF);	

			// Post sensor update event 
			esp_event_post_to(esp_dustsensor->event_loop_hdl, ESP_DUSTSENSOR_EVENT, SENSOR_UPDATE,
					&(esp_dustsensor->parent), sizeof(dustsensor_t), 100 / portTICK_PERIOD_MS);

			return ESP_OK;
		}else
			return ESP_ERR_INVALID_CRC;

	}

    return ESP_ERR_INVALID_SIZE;
} 

static void esp_handle_uart_pattern(esp_dustsensor_t *esp_dustsensor)
{
    size_t buffered_size = 0;

    uart_get_buffered_data_len(esp_dustsensor->uart_port, &buffered_size);
    int pos = uart_pattern_pop_pos(esp_dustsensor->uart_port);
    ESP_LOGI(DUSTSENSOR_TAG, "[PATTERN DETECTED] pos: %d, buffered size: %d", pos, buffered_size);

    if (pos != -1) {
        /* read one line(include '\n') */
        int read_len = uart_read_bytes(esp_dustsensor->uart_port, esp_dustsensor->buffer, pos, 100 / portTICK_PERIOD_MS);
        /* Send new line to handle */
        if (dustsensor_decode(esp_dustsensor, read_len) != ESP_OK) {
            ESP_LOGW(DUSTSENSOR_TAG, "DUSTSENSOR decode line failed");
        }
    } else {
        ESP_LOGW(DUSTSENSOR_TAG, "Pattern Queue Size too small");
        uart_flush_input(esp_dustsensor->uart_port);
    }
}


static void dustsensor_parser_task_entry(void *arg)
{
    esp_dustsensor_t *esp_dustsensor = (esp_dustsensor_t *)arg;
    uart_event_t event;
    while (1) {
        if (xQueueReceive(esp_dustsensor->event_queue, &event, pdMS_TO_TICKS(200))) {
            switch (event.type) {
            case UART_DATA:
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(DUSTSENSOR_TAG, "HW FIFO Overflow");
                uart_flush(esp_dustsensor->uart_port);
                xQueueReset(esp_dustsensor->event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(DUSTSENSOR_TAG, "Ring Buffer Full");
                uart_flush(esp_dustsensor->uart_port);
                xQueueReset(esp_dustsensor->event_queue);
                break;
            case UART_BREAK:
                ESP_LOGW(DUSTSENSOR_TAG, "Rx Break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(DUSTSENSOR_TAG, "Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(DUSTSENSOR_TAG, "Frame Error");
                break;
            case UART_PATTERN_DET:
                esp_handle_uart_pattern(esp_dustsensor);
                break;
            default:
                ESP_LOGW(DUSTSENSOR_TAG, "unknown uart event type: %d", event.type);
                break;
            }
        }
        /* Drive the event loop */
        esp_event_loop_run(esp_dustsensor->event_loop_hdl, pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}


dustsensor_parser_handle_t dustsensor_parser_init(const dustsensor_parser_config_t *config)
{
    esp_dustsensor_t *esp_dustsensor = calloc(1, sizeof(esp_dustsensor_t));
    if (!esp_dustsensor) {
        ESP_LOGE(DUSTSENSOR_TAG, "calloc memory for esp_fps failed");
        goto err_dustsensor;
    }
    esp_dustsensor->buffer = calloc(1, DUSTSENSOR_PARSER_RUNTIME_BUFFER_SIZE);
    if (!esp_dustsensor->buffer) {
        ESP_LOGE(DUSTSENSOR_TAG, "calloc memory for runtime buffer failed");
        goto err_buffer;
    }

    esp_dustsensor->uart_port = config->uart.uart_port;

    /* Install UART friver */
    uart_config_t uart_config = {
        .baud_rate = config->uart.baud_rate,
        .data_bits = config->uart.data_bits,
        .parity = config->uart.parity,
        .stop_bits = config->uart.stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    if (uart_param_config(esp_dustsensor->uart_port, &uart_config) != ESP_OK) {
        ESP_LOGE(DUSTSENSOR_TAG, "config uart parameter failed");
        goto err_uart_config;
    }
    if (uart_set_pin(esp_dustsensor->uart_port, UART_PIN_NO_CHANGE, config->uart.rx_pin,
                UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(DUSTSENSOR_TAG, "config uart gpio failed");
        goto err_uart_config;
    }
    if (uart_driver_install(esp_dustsensor->uart_port, DUSTSENSOR_PARSER_RING_BUFFER_SIZE, 0,
                config->uart.event_queue_size, &esp_dustsensor->event_queue, 0) != ESP_OK) {
        ESP_LOGE(DUSTSENSOR_TAG, "install uart driver failed");
        goto err_uart_install;
    }

    /* Set pattern interrupt, used to detect the end of a line */
    uart_enable_pattern_det_intr(esp_dustsensor->uart_port, 'B', 1, 10000, 10, 10);
    /* Set pattern queue size */
    uart_pattern_queue_reset(esp_dustsensor->uart_port, config->uart.event_queue_size);
    uart_flush(esp_dustsensor->uart_port);
    /* Create Event loop */
    esp_event_loop_args_t loop_args = {
        .queue_size = DUSTSENSOR_EVENT_LOOP_QUEUE_SIZE,
        .task_name = NULL
    };
    if (esp_event_loop_create(&loop_args, &esp_dustsensor->event_loop_hdl) != ESP_OK) {
        ESP_LOGE(DUSTSENSOR_TAG, "create event loop faild");
        goto err_eloop;
    }
    /* Create NMEA Parser task */
    BaseType_t err = xTaskCreate(
            dustsensor_parser_task_entry,
            "dustsensor_parser",
            DUSTSENSOR_PARSER_TASK_STACK_SIZE,
            esp_dustsensor,
            DUSTSENSOR_PARSER_TASK_PRIORITY,
            &esp_dustsensor->tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(DUSTSENSOR_TAG, "create dustsensor parser task failed");
        goto err_task_create;
    }

    ESP_LOGI(DUSTSENSOR_TAG, "DUSTSENSOR Parser init OK");
    return esp_dustsensor;

err_task_create:
    esp_event_loop_delete(esp_dustsensor->event_loop_hdl);
err_eloop:
err_uart_install:
    uart_driver_delete(esp_dustsensor->uart_port);
err_uart_config:
err_buffer:
    free(esp_dustsensor->buffer);
err_dustsensor:
    free(esp_dustsensor);

    return NULL;
}



esp_err_t dustsensor_parser_deinit(dustsensor_parser_handle_t dustsensor_hdl)
{
    esp_dustsensor_t *esp_dustsensor = (esp_dustsensor_t *)dustsensor_hdl;
    vTaskDelete(esp_dustsensor->tsk_hdl);
    esp_event_loop_delete(esp_dustsensor->event_loop_hdl);
    esp_err_t err = uart_driver_delete(esp_dustsensor->uart_port);
    free(esp_dustsensor->buffer);
    free(esp_dustsensor);
    return err;
}


esp_err_t dustsensor_parser_add_handler(dustsensor_parser_handle_t dustsensor_hdl, esp_event_handler_t event_handler, void *handler_args)
{
    esp_dustsensor_t *esp_dustsensor = (esp_dustsensor_t *)dustsensor_hdl;
    return esp_event_handler_register_with(esp_dustsensor->event_loop_hdl, ESP_DUSTSENSOR_EVENT, ESP_EVENT_ANY_ID,
            event_handler, handler_args);
}


esp_err_t dustsensor_parser_remove_handler(dustsensor_parser_handle_t dustsensor_hdl, esp_event_handler_t event_handler)
{
    esp_dustsensor_t *esp_dustsensor = (esp_dustsensor_t *)dustsensor_hdl;
    return esp_event_handler_unregister_with(esp_dustsensor->event_loop_hdl, ESP_DUSTSENSOR_EVENT, ESP_EVENT_ANY_ID, event_handler);
}
