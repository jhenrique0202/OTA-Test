#ifndef ESP32S3_NVM_APPLICATION_H
#define ESP32S3_NVM_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_nvm.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t
esp32s3_nvm_application_get_handle(salvus_hal_nvm_handle_t *handle);

salvus_error_type_t esp32s3_nvm_application_init();

#ifdef __cplusplus
}
#endif

#endif // ESP32S3_NVM_APPLICATION_H
