#ifndef ESP_LOG_APPLICATION_H
#define ESP_LOG_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t esp_log_application_init();

#ifdef __cplusplus
}
#endif

#endif // ESP_LOG_APPLICATION_H
