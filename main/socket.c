#include "strips.h"
#include "socket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char *TAG = "socket server";

// /* BSD Socket API Example

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */
// #include <string.h>
// #include <sys/param.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_system.h"
// #include "esp_netif.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "protocol_examples_common.h"


#define PORT                        CONFIG_PORT
#define KEEPALIVE_IDLE              CONFIG_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL          CONFIG_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT             CONFIG_KEEPALIVE_COUNT




void parse_command(const int sock);

void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = AF_INET; // IP v4 Addresses
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {

        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        parse_command(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}


#define PARSER_READY 0
#define PARSER_COLORS 1
#define PARSER_MODE 2
#define PARSER_MATRIX 3
#define PARSER_NUM_PIXELS 4
#define PARSER_PIXELS 5
#define PARSER_EXIT 6


/** Parse a command with the format:
 * 1 byte : (n) Number of colors into the palette.
 * 3 x n bytes : (r, g, b) Triplets to represent the colors
 * 1 byte : (mode) 0 is pixel by pixel mode ; 1 is addressed pixels mode
 * If mode == 0:
 * 350 bytes : (pixels) 1 byte [palette] per pixel.
 * If mode == 1:
 * 1 byte : (i) Number of pixels to change
 * 3 x i bytes : (l, p, c) p-th led on the l ledstrip to set with the color c from the color palette
 **/
void parse_command(const int sock)
{
    int len;
    uint8_t rx_buffer[128];

    uint32_t palette_size = 0;
    uint8_t * palette = NULL;
    uint32_t palette_idx = 0;

    uint32_t matrix_idx = 0;

    uint32_t num_pixels = 0;
    uint32_t parserpixel_idx = 0;
    uint32_t ledstrip_idx, pixel_idx, color_idx;

    int status = PARSER_READY;

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            ESP_LOGI(TAG, "Received %d bytes", len);

            for (uint32_t buff_idx=0 ; status != PARSER_EXIT && buff_idx<len ; buff_idx++)
            {   
                switch (status)
                {
                case PARSER_READY:
                    palette_size = rx_buffer[buff_idx];
                    palette = (uint8_t *)malloc(palette_size * 3);
                    memset(palette, 0, palette_size * 3);
                    
                    status = PARSER_COLORS;
                    palette_idx = 0;
                    break;

                case PARSER_COLORS:
                    palette[palette_idx++] = rx_buffer[buff_idx];

                    if (palette_idx / 3 == palette_size)
                    {
                        status = PARSER_MODE;
                    }
                    break;

                case PARSER_MODE:
                    if (rx_buffer[buff_idx] == 0) {
                        status = PARSER_MATRIX;
                        matrix_idx = 0;
                    }
                    else
                        status = PARSER_NUM_PIXELS;
                    break;

                case PARSER_MATRIX:
                    color_idx = rx_buffer[buff_idx];
                    if (color_idx >= palette_size)
                    {
                        ESP_LOGE(TAG, "Wrong color idx %lu. Only %lu colors declared.", color_idx, palette_size);
                        matrix_idx += 1;
                        continue;
                    }

                    // Set the pixel color
                    uint8_t * rgb = palette + 3 * color_idx;
                    leds_setled((matrix_idx % 50), rgb[0], rgb[1], rgb[2]);
                    matrix_idx += 1;

                    // End of the matrix ?
                    if (matrix_idx == 350)
                        status = PARSER_EXIT;
                    break;

                case PARSER_NUM_PIXELS:
                    num_pixels = rx_buffer[buff_idx];
                    status = PARSER_PIXELS;
                    break;

                case PARSER_PIXELS:
                    switch (parserpixel_idx % 3)
                    {
                    case 0:
                        ledstrip_idx = rx_buffer[buff_idx];
                        break;
                    case 1:
                        pixel_idx = rx_buffer[buff_idx];
                        break;
                    case 2:
                        color_idx = rx_buffer[buff_idx];

                        // Error detection
                        if (ledstrip_idx >= 7) {
                            ESP_LOGE(TAG, "Strip %lu does not exists.", ledstrip_idx);
                            parserpixel_idx += 1; break;
                        }
                        if (pixel_idx >= 50) {
                            ESP_LOGE(TAG, "Prixel %lu does not exists.", pixel_idx);
                            break;
                        }
                        if (color_idx >= palette_size) {
                            ESP_LOGE(TAG, "Wrong color idx %lu. Only %lu colors declared.", color_idx, palette_size);
                            parserpixel_idx += 1;
                            if (parserpixel_idx == 3 * num_pixels)
                                status = PARSER_EXIT;
                            break;
                        }

                        // Setting the pixel
                        uint8_t * rgb = palette + 3 * color_idx;
                        leds_setled(pixel_idx, rgb[0], rgb[1], rgb[2]);

                        parserpixel_idx += 1;

                        // Verify end
                        if (parserpixel_idx == 3 * num_pixels)
                            status = PARSER_EXIT;

                        break;
                    }
                }
            }

        }
    } while (status != PARSER_EXIT && len > 0);

    // setup a led update
    leds_show();
}


void sock_init()
{
	xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}


