#include "esp_http_server.h"

#include "http.h"


httpd_handle_t * server = NULL;

void http_init()
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    server = (httpd_handle_t *)malloc(sizeof(httpd_handle_t));

    /* Start the httpd server */
    if (httpd_start(server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(*server, &uri_get);
    }
    /* If server failed to start, handle will be NULL */
}


/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /* Send a simple response */
    const char resp[] = "Salut les copains !";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/uri",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};

/* Function for stopping the webserver */
void stop_webserver()
{
    if (*server) {
        /* Stop the httpd server */
        httpd_stop(*server);
    }
}