/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "dustsensor_parser.h"

/* Can run 'make menuconfig' to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

#if defined(CONFIG_DUSTSENSOR_UART_PORT_1)
#define DUSTSENSOR_UART_PORT UART_NUM_1
#elif defined(CONFIG_DUSTSENSOR_UART_PORT_2)
#define DUSTSENSOR_UART_PORT UART_NUM_2
#else
#error Please select the UART Port used by the dust sensor!
#endif

static void dustsensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    dustsensor_t *sensor = NULL;
    switch (event_id) {
    case SENSOR_UPDATE:
        sensor = (dustsensor_t *)event_data;
        // Handle the data from the sensor here
		printf("Concentration Unit (Standard):\nPM1.0 = %d ug/cum, PM2.5 = %d ug/cum, PM10 = %d ug/cum\n", 
				sensor->pm1,
				sensor->pm25,
				sensor->pm10);
		printf("Concentration Unit (Environmental):\nPM1.0 = %d ug/cum, PM2.5 = %d ug/cum, PM10 = %d ug/cum\n", 
				sensor->pm1_atmospheric,
				sensor->pm25_atmospheric,
				sensor->pm10_atmospheric);

        break;
    case SENSOR_UNKNOWN:
        /* print unknown statements */
        printf("Unknown statement:%s", (char *)event_data);
        break;
    default:
        break;
    }
}

void app_main()
{
    /* NMEA parser configuration */
    dustsensor_parser_config_t config = DUSTSENSOR_PARSER_CONFIG_DEFAULT();
	config.uart.uart_port = DUSTSENSOR_UART_PORT;
	config.uart.rx_pin = CONFIG_DUSTSENSOR_UART_RX_PIN;
    /* init NMEA parser library */
    dustsensor_parser_handle_t dustsensor_hdl = dustsensor_parser_init(&config);
    /* register event handler for NMEA parser library */
    dustsensor_parser_add_handler(dustsensor_hdl, dustsensor_event_handler, NULL);


    while(1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

        /* unregister event handler */
    dustsensor_parser_remove_handler(dustsensor_hdl, dustsensor_event_handler);
    /* deinit NMEA parser library */
    dustsensor_parser_deinit(dustsensor_hdl);
}
