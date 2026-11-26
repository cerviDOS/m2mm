#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"

#include "esp_log.h"
#include "esp_wifi.h"

#include "sdkconfig.h"

#define MAX_RETRY_ATTEMPTS 3

#define WIFI_LOG_TAG "WIFI"

#define WIFI_SUCCESS_BIT 0x1
#define WIFI_FAIL_BIT 0x2
EventGroupHandle_t wifi_init_event_group;

void event_handler(void* args, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    static uint8_t retry_count = 0;
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(WIFI_LOG_TAG, "Attempting to connect...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(WIFI_LOG_TAG, "Connection successful");
                retry_count = 0;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (retry_count++ < MAX_RETRY_ATTEMPTS) {
                    ESP_LOGW(WIFI_LOG_TAG, "Failed to connect, trying again (attempt %d of %d)", retry_count, MAX_RETRY_ATTEMPTS);
                    esp_wifi_connect();
                } else {
                    ESP_LOGE(WIFI_LOG_TAG, "Unable to connect after %d attempts.", MAX_RETRY_ATTEMPTS);
                    xEventGroupSetBits(wifi_init_event_group, WIFI_FAIL_BIT);
                }
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(WIFI_LOG_TAG, "IP retrieval successful");
                xEventGroupSetBits(wifi_init_event_group, WIFI_SUCCESS_BIT);
                break;
        }
    }
}

void wifi_init(void)
{
    wifi_init_event_group = xEventGroupCreate();

    esp_netif_init();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_config);

    esp_event_handler_instance_t wifi_event_instance;
    esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &wifi_event_instance
    );

    esp_event_handler_instance_t ip_event_instance;
    esp_event_handler_instance_register(
        IP_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        &ip_event_instance
    );

    // Use details from Kconfig
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD
            // Anything else?
        }
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    ESP_LOGI(WIFI_LOG_TAG, "Wi-Fi initialization complete");

    esp_wifi_start();

    EventBits_t wifi_result_bits = xEventGroupWaitBits(
        wifi_init_event_group,
        WIFI_SUCCESS_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (wifi_result_bits & WIFI_SUCCESS_BIT) {
        ESP_LOGI(WIFI_LOG_TAG, "Wi-Fi Ready");
    }
}

void app_main(void)
{
    nvs_flash_init();
    esp_event_loop_create_default();

    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("wifi_init", ESP_LOG_WARN);
    wifi_init();
}
