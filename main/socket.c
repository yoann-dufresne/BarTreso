#include "socket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

static const char *TAG = "socket client";


#define PORT                        CONFIG_PORT
#define KEEPALIVE_IDLE              CONFIG_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL          CONFIG_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT             CONFIG_KEEPALIVE_COUNT


static uint8_t mailbox[256];



void tcp_client_task(void *pvParameters)
{
    memset(mailbox, 0, 256);

    // char rx_buffer[256];
    char host_ip[] = "192.168.1.76";
    int addr_family = 0;
    int ip_protocol = 0;


    while (1)
    {
        // Config socket structure
        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, host_ip, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;

        // Socket creation
        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            shutdown(sock, 0);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", host_ip, PORT);

        // Set options
        int keepAlive = 1;
        // int keepIdle = KEEPALIVE_IDLE;
        // int keepInterval = KEEPALIVE_INTERVAL;
        // int keepCount = KEEPALIVE_COUNT;
        int nodelay = 1;

        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        // setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(int));

        // Socket connection
        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            shutdown(sock, 0);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");

        // first message: mac address
        uint8_t connec_msg[13] = {12, 'E', 'S', 'P', ' ', 0, 0, 0, 0, 0, 0, 0, 0};
        esp_efuse_mac_get_default(connec_msg+5);
        err = send(sock, connec_msg, 13, 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            shutdown(sock, 0);
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        while (1) {
            if (mailbox[0] > 0)
            {
                err = send(sock, mailbox, mailbox[0]+1, 0);
                mailbox[0] = 0;
                if (err < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    break;
                }
            }
            else {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
}


void sock_send(uint8_t * msg, uint8_t size)
{
    memcpy(mailbox+1, msg, size);
    mailbox[0] = size;
}



void sock_init()
{
	xTaskCreate(tcp_client_task, "tcp_server", 4096, NULL, 5, NULL);
}


