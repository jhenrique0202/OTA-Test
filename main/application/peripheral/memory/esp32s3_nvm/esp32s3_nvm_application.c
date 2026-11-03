#include "esp32s3_nvm_application.h"

#include "salvus_logging.h"

#include "salvus_hal_esp_nvs_types.h"
#include "salvus_hal_nvm.h"

static const char *TAG = "ESP32S3_NVM_APPLICATION";

salvus_error_type_t
esp32s3_nvm_application_get_handle(salvus_hal_nvm_handle_t *handle)
{
    return salvus_hal_nvm_get_handle(SALVUS_HAL_ESP_NVS, 0, handle);
}

salvus_error_type_t esp32s3_nvm_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_nvm_handle_t nvm_handle = NULL;

    salvus_hal_esp_nvs_config_t config = {
        .base_config = {.type = SALVUS_HAL_ESP_NVS, .unit = 0},
        .partition = {.name = NULL, .erase_when_partition_is_corrupted = 1}};

    err = salvus_hal_nvm_init(&config, &nvm_handle);
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize ESP NVS");
    }

    return err;
}
