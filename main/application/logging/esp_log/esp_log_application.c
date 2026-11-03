#include "esp_log_application.h"

#include "salvus_esp_log_types.h"
#include "salvus_logging.h"

salvus_error_type_t esp_log_application_init()
{
    salvus_esp_log_config_t logging_config = {
        .base_config = {.type = SALVUS_LOGGING_ESP_LOG,
                        .level = SALVUS_LOGGING_LEVEL_INFO},
        .buffer_size = 256};

    return salvus_logging_init(&logging_config);
}