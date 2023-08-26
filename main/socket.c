#include "strips.h"
#include "socket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "nvs_flash.h"
// #include "esp_netif.h"
// #include "protocol_examples_common.h"


#define PORT                        CONFIG_EXAMPLE_PORT
#define KEEPALIVE_IDLE              CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL          CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT             CONFIG_EXAMPLE_KEEPALIVE_COUNT



// static void tcp_server_task(void *pvParameters)
// {
// 	uint32_t r, g, b;

// 	while(1)
// 	{
// 		for (uint32_t i=0 ; i<50 ; i++)
// 		{
// 			uint32_t hue = (49-i) * 360 / 50;
// 	        leds_hsv2rgb(hue, 100, 100, &r, &g, &b);
// 	        leds_setled(i, r, g, b);
// 	        leds_show();
// 			vTaskDelay(pdMS_TO_TICKS(200));
// 		}

// 		for (uint32_t i=0 ; i<50 ; i++)
// 		{
// 			uint32_t hue = i * 360 / 50;
// 	        leds_hsv2rgb(hue, 100, 100, &r, &g, &b);
// 	        leds_setled(i, r, g, b);
// 	        leds_show();
// 			vTaskDelay(pdMS_TO_TICKS(200));
// 		}
// 	}
// }

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

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

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
    uint8_t rx_buffer[2048];

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0;
            ESP_LOGI(TAG, "Received %d bytes", len);

            // --- color map ---
            uint32_t num_colors = rx_buffer[0];
            uint8_t * color_map = rx_buffer + 1;
            uint32_t buffer_idx = 1 + 3 * (uint32_t)rx_buffer[0];

            uint32_t mode = rx_buffer[buffer_idx]:
            buffer_idx += 1;

            // --- Full matrix coloration mode ---
            if (mode == 0)
            {
            	ESP_LOGI(TAG, "Full coloration mode detected");
            	if (len - buffer_idx < 350)
            	{
            		ESP_LOGE(TAG, "Too few bytes. Cannot perform a full coloration mode with only %u bytes while 350 are expected.", (len - buffer_idx));
         			return;
            	}

            	for (uint32_t ledstrip=0 ; ledstrip<7 ; ledstrip++)
            	{
            		for (uint32_t pixel=0 ; pixel<50 ; pixel++)
            		{
            			// Get the color palette idx
            			uint32_t color_idx = rx_buffer[buffer_idx++];
            			if (color_idx >= num_colors)
            			{
            				ESP_LOGE(TAG, "Wrong color idx %u. Only %u colors declared.", color_idx, num_colors);
            				continue;
            			}

            			// Set the pixel color
            			uint8_t * rgb = color_map + 3 * color_idx;
            			leds_setled(pixel, rgb[0], rgb[1], rgb[2]);
            		}
            	}

            	leds_show();
            }

            // --- Pixel by pixel coloration mode ---
            else if (mode == 1)
            {
            	ESP_LOGI(TAG, "Pixel by pixel coloration mode detected.")

            	// Getting the number of pixels to set
            	uint32_t num_pixel = rx_buffer[buffer_idx];

            	if ((len - buffer_idx) < (num_pixel * 3))
            	{
            		ESP_LOGE(TAG, "Too few bytes. Cannot set %u pixels with only %u bytes while 3 bytes per pixel are requiered.", num_pixel, (len - buffer_idx));
            		return;
            	}

            	for (uint32_t i=0 ; i<num_pixel ; i++)
            	{
            		// Parsing the next pixel
            		uint32_t strip_idx = rx_buffer[buffer_idx++];
            		uint32_t pixel = rx_buffer[buffer_idx++];
					uint32_t color_idx = rx_buffer[buffer_idx++];
					
					if (strip_idx >= 7)
        			{
        				ESP_LOGE(TAG, "Strip %u does not exists.", strip_idx);
        				continue;
        			}

        			if (pixel >= 50)
        			{
        				ESP_LOGE(TAG, "Strip %u does not exists.", strip_idx);
        				continue;
        			}

            		if (color_idx >= num_colors)
        			{
        				ESP_LOGE(TAG, "Wrong color idx %u. Only %u colors declared.", color_idx, num_colors);
        				continue;
        			}

        			// Setting the pixel
        			uint8_t * rgb = color_map + 3 * color_idx;
            		leds_setled(pixel, rgb[0], rgb[1], rgb[2]);
            	}

            	leds_show();
            }

            // Wrong mode
            else
            {
            	ESP_LOGE(TAG, "Wrong coloration mode %u at byte %u. Skipping", mode, buffer_idx);
            	return;
            }
        }
    } while (len > 0);
}


void sock_init()
{
	xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}


