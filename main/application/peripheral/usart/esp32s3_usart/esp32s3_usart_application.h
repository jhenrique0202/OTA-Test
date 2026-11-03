#ifndef ESP32S3_USART_APPLICATION_H
#define ESP32S3_USART_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_usart.h"

#include "usart_application.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t esp32s3_usart_application_get_usart_handle(
    usart_application_handle_type_t type,
    salvus_hal_usart_handle_t *usart_handle);

salvus_error_type_t esp32s3_usart_application_init();

salvus_error_type_t esp32s3_usart_application_start();

#ifdef __cplusplus
}
#endif

#endif // ESP32S3_USART_APPLICATION_H
