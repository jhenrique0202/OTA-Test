#ifndef USART_APPLICATION_H
#define USART_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_usart.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum usart_application_handle_type_t
{
#if defined(SALVUS_ENABLE_SIMCOM_MODULE_USART)
    USART_APPLICATION_COMMUNICATION_MODULE_USART_HANDLE,
#endif
    USART_APPLICATION_PIN_HANDLE_MAX_TYPE
} usart_application_handle_type_t;

salvus_error_type_t
usart_application_get_usart_handle(usart_application_handle_type_t type,
                                   salvus_hal_usart_handle_t *usart_handle);

salvus_error_type_t usart_application_init();

salvus_error_type_t usart_application_start();

#ifdef __cplusplus
}
#endif

#endif // USART_APPLICATION_H
