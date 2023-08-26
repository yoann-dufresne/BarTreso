#include "strips.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

static const char *TAG = "strips";


#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      21
// #define EXAMPLE_CHASE_SPEED_MS      500

#define LED_NUMBERS 50



struct leds_datastruct_s
{
    TaskHandle_t task;

    rmt_channel_handle_t chan;
    rmt_tx_channel_config_t tx_chan_config;

    rmt_encoder_handle_t encoder;
    led_strip_encoder_config_t encoder_config;

    uint8_t pixels[LED_NUMBERS * 3];
};
typedef struct leds_datastruct_s leds_datastruct;

void leds_update_task(void *pvParameters);



// Static leds datastructure. This is the structure that will be constantly used.
static leds_datastruct leds_ds;



void leds_init()
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    leds_ds.chan = NULL;
    leds_ds.tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT; // select source clock
    leds_ds.tx_chan_config.gpio_num = RMT_LED_STRIP_GPIO_NUM;
    leds_ds.tx_chan_config.mem_block_symbols = 64; // increase the block size can make the LED less flickering
    leds_ds.tx_chan_config.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ;
    leds_ds.tx_chan_config.trans_queue_depth = 4; // set the number of transactions that can be pending in the background

    ESP_ERROR_CHECK(rmt_new_tx_channel(&leds_ds.tx_chan_config, &leds_ds.chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    leds_ds.encoder = NULL;
    leds_ds.encoder_config.resolution = RMT_LED_STRIP_RESOLUTION_HZ;
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&leds_ds.encoder_config, &leds_ds.encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(leds_ds.chan));

    ESP_LOGI(TAG, "LED init complete");

    ESP_LOGI(TAG, "Init led refresh task");

    // Test led values
    uint32_t r, g, b;
    for (int l=0 ; l<LED_NUMBERS ; l++)
    {
        uint32_t hue = l * 360 / LED_NUMBERS;
        leds_hsv2rgb(hue, 100, 100, &r, &g, &b);

        leds_ds.pixels[3*l] = r;
        leds_ds.pixels[3*l+1] = g;
        leds_ds.pixels[3*l+2] = b;
    }

    xTaskCreate(leds_update_task, "leds_refresh", 2048, NULL, 1, &(leds_ds.task));
}


void leds_setled(uint32_t led_idx, uint8_t r, uint8_t g, uint8_t b)
{
    leds_ds.pixels[led_idx * 3] = r;
    leds_ds.pixels[led_idx * 3 + 1] = g;
    leds_ds.pixels[led_idx * 3 + 2] = b;
}


void leds_update_task(void *pvParameters)
{
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };

    while (1) {
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(leds_ds.chan, leds_ds.encoder, leds_ds.pixels, sizeof(leds_ds.pixels), &tx_config));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(leds_ds.chan, portMAX_DELAY));
        vTaskSuspend(leds_ds.task);
    }
}


void leds_show()
{
    vTaskResume(leds_ds.task);
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

