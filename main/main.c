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
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "esp_sleep.h"
#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "nvs.h"

// TODO add button event handling instead of the simple gpio interrupt
#include "iot_button.h"

static const char *TAG = "esp32_c6";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

#define BUTTON_GPIO CONFIG_BUTTON_GPIO

typedef enum
{
    RAINBOW,
    BLINKING,
    ORDINARY
} led_state;

#define NUM_STATES 3

static led_state STATE = ORDINARY;

static led_strip_handle_t led_strip;
// Predefined RGB values
const uint32_t NONE = 0x000000;
const uint32_t RED = 0xFF0000;
const uint32_t GREEN = 0x00FF00;
const uint32_t BLUE = 0x0000FF;
const uint32_t YELLOW = 0xFFFF00;
const uint32_t CYAN = 0x00FFFF;
const uint32_t MAGENTA = 0xFF00FF;
const uint32_t WHITE = 0xFFFFFF;

// Array to store the colors
const uint32_t colors[] = {
    0x00FF00, // Green
    0xFF0000, // Red
    0xFF7F00, // Orange
    0xFFFF00, // Yellow
    0x9400D3, // Violet
    0x0000FF, // Blue
    0x4B0082, // Indigo
    0xFFFFFF  // White
};

// Function to store a value in persistent memory
void store_value(int value)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Flash Init Error");
        return;
    }

    // Open NVS namespace
    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Open Error");
        return;
    }

    // Store the value
    err = nvs_set_i32(nvs_handle, "value", value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Set Error");
        return;
    }

    // Commit the changes
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Commit Error");
        return;
    }

    // Close NVS handle
    nvs_close(nvs_handle);
}

// Function to retrieve the stored value from persistent memory
int retrieve_value(void)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Flash Init Error");
        return 0;
    }

    // Open NVS namespace
    nvs_handle_t nvs_handle;
    err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Open Error");
        return 0;
    }

    // Retrieve the value
    int value = 0;
    err = nvs_get_i32(nvs_handle, "value", &value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS Get Error");
        return 0;
    }

    // Close NVS handle
    nvs_close(nvs_handle);

    return value;
}

static void set_color(uint32_t color)
{
    /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    led_strip_set_pixel(led_strip, 0, r, g, b);
    /* Refresh the strip to send data */
    led_strip_refresh(led_strip);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(50)); // Add a delay to debounce the button
    // ESP_LOGI(TAG, "Interrupt handler called!");
    ESP_DRAM_LOGI(TAG, "Interrupt handler called!");

    // xSemaphoreTake(state_mutex, portMAX_DELAY);
    STATE = (STATE + 1) % NUM_STATES;
    ESP_DRAM_LOGI(TAG, "State changed to %d", STATE);
    // xSemaphoreGiveFromISR(state_mutex, NULL);
}

static void configure_gpio_interrupt(void)
{
    gpio_config_t io_conf;

    // Configure the button GPIO pin as input
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Install the ISR service
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);
}

void toggle_led()
{
    /* Retrieve the LED state from persistent memory */
    uint8_t color_index = retrieve_value();
    
    set_color(colors[!color_index]);
    store_value(!color_index);
}

uint32_t lerpColor(uint32_t color1, uint32_t color2, float t) {
    int r1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int b1 = color1 & 0xFF;

    int r2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int b2 = color2 & 0xFF;

    int r = (int)(r1 + t * (r2 - r1));
    int g = (int)(g1 + t * (g2 - g1));
    int b = (int)(b1 + t * (b2 - b1));

    return (r << 16) | (g << 8) | b;
}

void app_main(void)
{
    configure_led();
    toggle_led();
    configure_gpio_interrupt();
    uint8_t color_index = retrieve_value();
    int numColors = sizeof(colors) / sizeof(colors[0]);

    uint8_t currentIndex = 0;
    uint8_t nextIndex = (currentIndex + 1) % numColors; // numColors for all the colors, 2 for only red and green
    double delay = 10;
    uint32_t lerpedColor = colors[currentIndex];
    while (true)
    {   
        switch (STATE)
        {
        case RAINBOW:
            for (float t = 0.0; t < 1.0; t += 0.01) {
                uint32_t lerpedColor = lerpColor(colors[currentIndex], colors[nextIndex], t);
                set_color(lerpedColor);
                vTaskDelay(pdMS_TO_TICKS(delay));
            }
            ESP_LOGI(TAG, "Color: %d", (int)lerpedColor);
            ESP_LOGI(TAG, "Next Color: %d", (int)colors[nextIndex]);
            currentIndex = nextIndex;
            nextIndex = (currentIndex + 1) % numColors;
            break;
        case BLINKING:
            set_color(NONE);
            vTaskDelay(pdMS_TO_TICKS(200));
            set_color(colors[color_index]);
            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        default:
            vTaskDelay(pdMS_TO_TICKS(1000));
            break;
        }
    }
}
