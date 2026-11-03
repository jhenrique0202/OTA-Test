#ifndef ESP32S3_GPIO_APPLICATION_H
#define ESP32S3_GPIO_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_gpio.h"

#include "gpio_application.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t
esp32s3_gpio_application_get_pin_handle(gpio_application_pin_handle_type_t type,
                                        salvus_hal_pin_handle_t *pin_handle);

salvus_error_type_t esp32s3_gpio_application_init();

salvus_error_type_t esp32s3_gpio_application_start();

#ifdef __cplusplus
}
#endif

#endif // ESP32S3_GPIO_APPLICATION_H
