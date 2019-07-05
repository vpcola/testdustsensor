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

static void dustsensor_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    dustsensor_t *sensor = NULL;
    switch (event_id) {
    case SENSOR_UPDATE:
        sensor = (dustsensor_t *)event_data;
        // TODO: 
        // Handle the data from the sensor here
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
    /* Configure the IOMUX register for pad BLINK_GPIO (some pads are
       muxed to GPIO on reset already, but some default to other
       functions and need to be switched to GPIO. Consult the
       Technical Reference for a list of pads and their default
       functions.)
    */
    gpio_pad_select_gpio(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);


    /* NMEA parser configuration */
    dustsensor_parser_config_t config = DUSTSENSOR_PARSER_CONFIG_DEFAULT();
    config.uart.uart_port = UART_NUM_1;
    config.uart.rx_pin = 21; // Change GPI pin
    /* init NMEA parser library */
    dustsensor_parser_handle_t dustsensor_hdl = dustsensor_parser_init(&config);
    /* register event handler for NMEA parser library */
    dustsensor_parser_add_handler(dustsensor_hdl, dustsensor_event_handler, NULL);


    while(1) {
        /* Blink off (output low) */
	printf("Turning off the LED\n");
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        /* Blink on (output high) */
	printf("Turning on the LED\n");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

        /* unregister event handler */
    dustsensor_parser_remove_handler(dustsensor_hdl, dustsensor_event_handler);
    /* deinit NMEA parser library */
    dustsensor_parser_deinit(dustsensor_hdl);
}
