#include "esp32s3_usart_application.h"

#include "salvus_logging.h"

#include "salvus_hal_esp_usart_types.h"
#include "salvus_hal_usart.h"

#include "peripheral_application.h"

static const char *TAG = "ESP32S3_USART_APPLICATION";

#if defined(SALVUS_ENABLE_SIMCOM_MODULE_USART)
static salvus_error_type_t _init_communication_module_usart()
{
    salvus_hal_esp_usart_config_t usart_config = {
        .base_config = {.type = SALVUS_HAL_ESP_USART,
                        .unit = COMMUNICATION_MODULE_USART_UNIT},
        .uart_config = {.baud_rate = 115200,
                        .data_bits = UART_DATA_8_BITS,
                        .parity = UART_PARITY_DISABLE,
                        .stop_bits = UART_STOP_BITS_1,
                        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE},
        .pins = {.tx = COMMUNICATION_MODULE_USART_TX_PIN,
                 .rx = COMMUNICATION_MODULE_USART_RX_PIN,
                 .rts = -1,
                 .cts = -1},
        .buffer_size = {.tx = 512, .rx = 512}};

    salvus_hal_usart_handle_t usart_handle = NULL;
    return salvus_hal_usart_init(&usart_config, &usart_handle);
}
#endif

salvus_error_type_t esp32s3_usart_application_get_usart_handle(
    usart_application_handle_type_t type,
    salvus_hal_usart_handle_t *usart_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    switch (type) {
#if defined(SALVUS_ENABLE_SIMCOM_MODULE_USART)
    case USART_APPLICATION_COMMUNICATION_MODULE_USART_HANDLE :
        err = salvus_hal_usart_get_handle(SALVUS_HAL_ESP_USART,
                                          COMMUNICATION_MODULE_USART_UNIT,
                                          usart_handle);
        break;
#endif

    default :
        break;
    }

    return err;
}

salvus_error_type_t esp32s3_usart_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

#if defined(SALVUS_ENABLE_SIMCOM_MODULE_USART)
    err = _init_communication_module_usart();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize communication module USART");
    }
#endif

    return err;
}

salvus_error_type_t esp32s3_usart_application_start()
{
    return SALVUS_ERROR_OK;
}
