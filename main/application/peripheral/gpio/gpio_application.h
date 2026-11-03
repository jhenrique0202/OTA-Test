#ifndef GPIO_APPLICATION_H
#define GPIO_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gpio_application_pin_handle_type_t
{
    GPIO_APPLICATION_COMMUNICATION_MODULE_POWER_ENABLE_PIN_HANDLE,
    GPIO_APPLICATION_COMMUNICATION_MODULE_STATUS_PIN_HANDLE,
#if defined(SALVUS_ENABLE_SIMCOM_MODULE_RESET_PIN)
    GPIO_APPLICATION_COMMUNICATION_MODULE_RESET_PIN_HANDLE,
#endif
    GPIO_APPLICATION_PIN_HANDLE_MAX_TYPE
} gpio_application_pin_handle_type_t;

salvus_error_type_t
gpio_application_get_pin_handle(gpio_application_pin_handle_type_t type,
                                salvus_hal_pin_handle_t *pin_handle);

salvus_error_type_t gpio_application_init();

salvus_error_type_t gpio_application_start();

#ifdef __cplusplus
}
#endif

#endif // GPIO_APPLICATION_H
