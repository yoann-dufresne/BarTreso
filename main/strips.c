#include "strips.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "strips";



// struct leds_datastruct_s
// {
//     TaskHandle_t task;

//     rmt_channel_handle_t chan;
//     rmt_tx_channel_config_t tx_chan_config;

//     rmt_encoder_handle_t encoder;
//     led_strip_encoder_config_t encoder_config;

//     uint8_t pixels[LED_NUMBERS * 3];
// };
// typedef struct leds_datastruct_s leds_datastruct;


#define LED_NUMBERS 50


void leds_update_task(void *pvParameters);


// Static leds datastructure. This is the structure that will be constantly used.
// static leds_datastruct leds_ds;
static TaskHandle_t leds_task;

static led_strip_config_t strip_config;
static led_strip_spi_config_t spi_config;
static led_strip_handle_t leds_pixels;


void leds_init()
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    strip_config.strip_gpio_num = 2;   // The GPIO that connected to the LED strip's data line
    strip_config.max_leds = LED_NUMBERS;        // The number of LEDs in the strip,
    strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB; // Pixel format of your LED strip
    strip_config.led_model = LED_MODEL_WS2812;            // LED strip model
    strip_config.flags.invert_out = false;                // whether to invert the output signal

    // LED strip backend configuration: SPI
    spi_config.clk_src = SPI_CLK_SRC_DEFAULT; // different clock source can lead to different power consumption
    spi_config.flags.with_dma = true;         // Using DMA can improve performance and help drive more LEDs
    spi_config.spi_bus = SPI2_HOST;           // SPI bus ID

    // LED Strip object handle
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &leds_pixels));
    ESP_LOGI(TAG, "Created LED strip object with SPI backend");

    // Test led values
    uint32_t r, g, b;
    for (int l=0 ; l<LED_NUMBERS ; l++)
    {
        uint32_t hue = l * 360 / LED_NUMBERS;
        leds_hsv2rgb(hue, 100, 100, &r, &g, &b);
        leds_setled(l, r, g, b);
    }

    xTaskCreate(leds_update_task, "leds_refresh", 2048, NULL, 1, &leds_task);
}


void leds_setled(uint32_t led_idx, uint8_t r, uint8_t g, uint8_t b)
{
    // ESP_LOGI(TAG, "Set pixel %lu (%lu %lu %lu)", led_idx, (uint32_t)r, (uint32_t)g, (uint32_t)b);
    ESP_ERROR_CHECK(led_strip_set_pixel(leds_pixels, led_idx, g, r, b));
}


void leds_update_task(void *pvParameters)
{
    while (1) {
        // Flush RGB values to LEDs
        // ESP_LOGI(TAG, "Leds update");
        ESP_ERROR_CHECK(led_strip_refresh(leds_pixels));
        vTaskSuspend(leds_task);
    }
}


void leds_show()
{
    // ESP_LOGI(TAG, "Leds show");
    vTaskResume(leds_task);
}



/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void leds_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

