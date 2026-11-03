#ifndef PERIPHERAL_APPLICATION_H
#define PERIPHERAL_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#if defined(SALVUS_ESP32S3_MICROCONTROLLER_SELECTED)
#define COMMUNICATION_MODULE_POWER_ENABLE_PIN 40
#define COMMUNICATION_MODULE_STATUS_PIN 1

#if defined(SALVUS_Y70XX_SIMCOM_SELECTED)
#define COMMUNICATION_MODULE_RESET_PIN 2

#define COMMUNICATION_MODULE_USART_UNIT 1
#define COMMUNICATION_MODULE_USART_TX_PIN 42
#define COMMUNICATION_MODULE_USART_RX_PIN 41
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t peripheral_application_init();

salvus_error_type_t peripheral_application_start();

#ifdef __cplusplus
}
#endif

#endif // PERIPHERAL_APPLICATION_H
