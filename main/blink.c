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
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "string.h"
#include "ctype.h"

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define LED_PIN 2
#define UART_PORT UART_NUM_0
#define TIMEOUT 100

#define UART_BUFFER_SIZE 2048
#define UART_TX 1
#define UART_RX 3

static const char *TAG = "UART TEST";

static int8_t led_state = 0;
static int8_t led_save_state = 0;
int8_t blink_rate = 1; // [blink_rate] blink(s) / second
nvs_handle data_handle;

void set_LED(void) {
    gpio_set_level(LED_PIN, led_state);
}

static void toggle_LED(void) {
    led_state = !led_state; // toggle [led_state]
    if (led_save_state != 2) {
        led_save_state = led_state;
    }
    set_LED();
}

void enable_LED(void) {
    led_state = 1;
    led_save_state = led_state;
    set_LED();
}

void disable_LED(void) {
    led_state = 0;
    led_save_state = led_state;
    set_LED();
}

void configure_led(void) {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
}

void configure_UART(void) {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .source_clk = UART_SCLK_APB,
    };
    uart_param_config(UART_PORT, &uart_config);
    uart_set_pin(UART_PORT, UART_TX, UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    
    QueueHandle_t uart_queue;
    uart_driver_install(UART_PORT, 2048, 2048, 20, &uart_queue, 0);
}

void configure_NVS() {
    nvs_flash_init();
    nvs_open("led_state", NVS_READWRITE, &data_handle);
    nvs_get_i8(data_handle, "led_state", &led_save_state); // initialize led_state
    nvs_get_i8(data_handle, "blink_rate", &blink_rate);
    led_state = led_save_state;
}

void save_state() {
    nvs_set_i8(data_handle, "led_state", led_save_state);
    nvs_set_i8(data_handle, "blink_rate", blink_rate);
}

void interpret_command(char* UART_command) {
    ESP_LOGI(TAG, "data: %s", UART_command);
    if (strcmp(UART_command, "help") == 0) {
        ESP_LOGI(TAG, "Commands:\nledOn\nledOff\nledToggle\nledBlink\nblinkRate [0-10]");
    }
    else if (strcmp(UART_command, "ledon") == 0) {
        enable_LED();
        save_state();
    }
    else if (strcmp(UART_command, "ledoff") == 0) {
        disable_LED();
        save_state();
    }
    else if (strcmp(UART_command, "ledtoggle") == 0) {
        toggle_LED();
        save_state();
    }
    else if (strcmp(UART_command, "ledblink") == 0) {
        toggle_LED(); // provide immediate feedback to the command
        led_save_state = 2;
        save_state();
    }
    else {
        // format of "blinkrate 0123"
        char blink_rate_command[10] = ""; // length of "blinkrate"
        strncat(blink_rate_command, UART_command, 9);
        if (strcmp(blink_rate_command, "blinkrate") == 0) {
            char blink_rate_setting[5] = ""; // should never be greater than 5-1 = 4
            strncat(blink_rate_setting, &UART_command[10], strlen(UART_command)-10);

            blink_rate = (int8_t) atoi((char*) blink_rate_setting);
            ESP_LOGI(TAG, "Blink rate set to %d/s", blink_rate);
            save_state();
        }
    }
}

void app_main(void) {
    configure_led();
    configure_UART();
    configure_NVS();
    set_LED();

    uint8_t* data = (uint8_t*) malloc(UART_BUFFER_SIZE);
    uint8_t cycles = 0; // keep track of time without starting another process or pausing execution
    while (true) {
        int len = uart_read_bytes(UART_PORT, data, UART_BUFFER_SIZE-1, TIMEOUT / portTICK_PERIOD_MS); // each cycle of this loop will last 100ms
        if (len) {
            char* old_data_char = (char*) data;
            char cutoff_data[50] = ""; // [len] should never be greater than 50
            strncat(cutoff_data, old_data_char, len);

            for (int i = 0; i < len; i++) {
                cutoff_data[i] = (char) tolower((int) cutoff_data[i]);
            }

            char* new_data = (char*) cutoff_data;

            interpret_command(new_data);
        }
        if (led_save_state == 2) { // only increment [cycles] when in the looping state
            cycles++;
            if (cycles*blink_rate*10 > TIMEOUT) { // toggle every 100ms*10 = 1000ms = 1s
                cycles = 0; // reset cycles
                toggle_LED();
            }
        }
    }
}
