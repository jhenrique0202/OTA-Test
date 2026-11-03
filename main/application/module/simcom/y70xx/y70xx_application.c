#include "y70xx_application.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gpio_application.h"
#include "usart_application.h"

#include "salvus_logging.h"

#include "salvus_simcom.h"
#include "salvus_y70xx_types.h"

#define PDP_CID 0
#define SSL_CID 0

#define TURN_ON_TIMEOUT_MS 5000
#define TURN_OFF_TIMEOUT_MS 5000
#define RESPONSIVENESS_QUERY_TIMEOUT_MS 1000
#define BAUDRATE_CONFIGURATION_TIMEOUT_MS 1000
#define PSM_DISABLING_TIMEOUT_MS 10000
#define IMEI_QUERY_TIMEOUT_MS 5000
#define NETWORK_CONNECTION_CONFIGURATION_TIMEOUT_MS 60000
#define NETWORK_REGISTERING_POLL_INTERVAL_MS 500
#define NETWORK_ATTACHMENT_POLL_INTERVAL_MS 1000
#define ESTABLISH_NETWORK_CONNECTION_TIMEOUT_MS 120000

static const char *_access_point_name = "salvus.tim.br";

static const char *TAG = "Y70XX_APPLICATION";

static int32_t _get_operation_elapsed_time(TickType_t start_tick);

static salvus_error_type_t _check_operation_timeout(TickType_t start_tick,
                                                    int32_t timeout_ms,
                                                    int32_t *elapsed_time_ms);

static salvus_error_type_t _turn_on(salvus_simcom_handle_t simcom_handle);

static salvus_error_type_t _turn_off(salvus_simcom_handle_t simcom_handle);

static salvus_error_type_t
_query_responsiveness(salvus_simcom_handle_t simcom_handle);

static salvus_error_type_t
_configure_baudrate(salvus_simcom_handle_t simcom_handle);

static salvus_error_type_t _init_y70xx();

static salvus_error_type_t _start_y70xx();

static int32_t _get_operation_elapsed_time(TickType_t start_tick)
{
    return (int32_t) ((xTaskGetTickCount() - start_tick) * portTICK_PERIOD_MS);
}

static salvus_error_type_t _check_operation_timeout(TickType_t start_tick,
                                                    int32_t timeout_ms,
                                                    int32_t *elapsed_time_ms)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (timeout_ms >= 0) {
        *elapsed_time_ms = _get_operation_elapsed_time(start_tick);

        if (*elapsed_time_ms >= timeout_ms) {
            err = SALVUS_ERROR_TIMEOUT;
        }
    }

    return err;
}

static salvus_error_type_t _turn_on(salvus_simcom_handle_t simcom_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_pin_handle_t power_enable_pin_handle = NULL;

    err = gpio_application_get_pin_handle(
        GPIO_APPLICATION_COMMUNICATION_MODULE_POWER_ENABLE_PIN_HANDLE,
        &power_enable_pin_handle);

    if (err == SALVUS_ERROR_OK) {
        salvus_hal_pin_write(power_enable_pin_handle, SALVUS_HAL_PIN_HIGH);

        err = salvus_simcom_turn_on(simcom_handle, TURN_ON_TIMEOUT_MS);
    }

    if (err != SALVUS_ERROR_OK) {
        salvus_hal_pin_write(power_enable_pin_handle, SALVUS_HAL_PIN_LOW);
    }

    return err;
}

static salvus_error_type_t _turn_off(salvus_simcom_handle_t simcom_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_pin_handle_t power_enable_pin_handle = NULL;

    err = gpio_application_get_pin_handle(
        GPIO_APPLICATION_COMMUNICATION_MODULE_POWER_ENABLE_PIN_HANDLE,
        &power_enable_pin_handle);

    if (err == SALVUS_ERROR_OK) {
        salvus_simcom_turn_off(simcom_handle, TURN_OFF_TIMEOUT_MS);

        salvus_hal_pin_write(power_enable_pin_handle, SALVUS_HAL_PIN_LOW);
    }

    return err;
}

static salvus_error_type_t
_query_responsiveness(salvus_simcom_handle_t simcom_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    uint8_t responsiveness_status = 0;

    err = salvus_simcom_query_responsiveness(
        simcom_handle, &responsiveness_status, RESPONSIVENESS_QUERY_TIMEOUT_MS);

    if (err == SALVUS_ERROR_OK) {
        if (! responsiveness_status) {
            err = SALVUS_ERROR_FAIL;
        }
    }

    return err;
}

static salvus_error_type_t
_configure_baudrate(salvus_simcom_handle_t simcom_handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_usart_handle_t usart_handle = NULL;
    err = usart_application_get_usart_handle(
        USART_APPLICATION_COMMUNICATION_MODULE_USART_HANDLE, &usart_handle);

    if (err == SALVUS_ERROR_OK) {
        err = salvus_hal_usart_set_baudrate(usart_handle, 9600);

        if (err == SALVUS_ERROR_OK) {
            err = _query_responsiveness(simcom_handle);

            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_set_baudrate(
                    simcom_handle, 115200, BAUDRATE_CONFIGURATION_TIMEOUT_MS);
            }

            salvus_hal_usart_set_baudrate(usart_handle, 115200);

            if (err == SALVUS_ERROR_OK) {
                err = _query_responsiveness(simcom_handle);
            }
        }
    }

    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Error configuring Y70XX baudrate");
    }

    return err;
}

static salvus_error_type_t _init_y70xx()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_hal_pin_handle_t status_pin_handle = NULL;
    salvus_hal_pin_handle_t reset_pin_handle = NULL;

    salvus_hal_usart_handle_t usart_handle = NULL;

    salvus_simcom_handle_t simcom_handle = NULL;

    err = gpio_application_get_pin_handle(
        GPIO_APPLICATION_COMMUNICATION_MODULE_STATUS_PIN_HANDLE,
        &status_pin_handle);

    if (err == SALVUS_ERROR_OK) {
        err = gpio_application_get_pin_handle(
            GPIO_APPLICATION_COMMUNICATION_MODULE_RESET_PIN_HANDLE,
            &reset_pin_handle);
    }

    if (err == SALVUS_ERROR_OK) {
        err = usart_application_get_usart_handle(
            USART_APPLICATION_COMMUNICATION_MODULE_USART_HANDLE, &usart_handle);
    }

    if (err == SALVUS_ERROR_OK) {
        salvus_y70xx_config_t simcom_config = {
            .base_config = {.type = SALVUS_Y70XX,
                            .unit = 0,
                            .status_pin_handle = status_pin_handle},
            .usart_handle = usart_handle,
            .reset_pin_handle = reset_pin_handle};

        err = salvus_simcom_init(&simcom_config, &simcom_handle);
    }

    return err;
}

static salvus_error_type_t _start_y70xx()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;

    if (err == SALVUS_ERROR_OK) {
        err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);
    }

    if (err == SALVUS_ERROR_OK) {
        err = _turn_on(simcom_handle);
    }

    if (err == SALVUS_ERROR_OK) {
        for (uint8_t i = 0; i < 3; i++) {
            err = _query_responsiveness(simcom_handle);

            if (err == SALVUS_ERROR_OK) {
                break;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (err != SALVUS_ERROR_OK) {
            err = _configure_baudrate(simcom_handle);
        }

        if (err != SALVUS_ERROR_OK) {
            _turn_off(simcom_handle);
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err =
            salvus_simcom_disable_psm(simcom_handle, PSM_DISABLING_TIMEOUT_MS);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Error disabling PSM");
        }

        const char *imei = NULL;

        if (err == SALVUS_ERROR_OK) {
            err = salvus_simcom_query_imei(simcom_handle, &imei,
                                           IMEI_QUERY_TIMEOUT_MS);
            if (err == SALVUS_ERROR_OK) {
                SALVUS_LOGI(TAG, "Communication module IMEI: %s", imei);
            } else {
                SALVUS_LOGE(TAG, "Error getting IMEI");
            }
        }

        _turn_off(simcom_handle);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }

    return err;
}

salvus_error_type_t
y70xx_application_get_simcom_handle(salvus_simcom_handle_t *simcom_handle)
{
    return salvus_simcom_get_handle(SALVUS_Y70XX, 0, simcom_handle);
}

salvus_error_type_t y70xx_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = _init_y70xx();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize Y70XX");
    }

    return err;
}

salvus_error_type_t y70xx_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = _start_y70xx();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to start Y70XX");
    }

    return err;
}

salvus_error_type_t y70xx_application_turn_on()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        err = _turn_on(simcom_handle);

        if (err == SALVUS_ERROR_OK) {
            for (uint8_t i = 0; i < 3; i++) {
                err = _query_responsiveness(simcom_handle);

                if (err == SALVUS_ERROR_OK) {
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            if (err != SALVUS_ERROR_OK) {
                _turn_off(simcom_handle);
            }
        }
    }

    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to turn on Y70XX");
    }

    return err;
}

salvus_error_type_t y70xx_application_turn_off()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        err = _turn_off(simcom_handle);
    }

    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to turn off Y70XX");
    }

    return err;
}

salvus_error_type_t y70xx_application_check_power_status(uint8_t *status)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        if (_query_responsiveness(simcom_handle) == SALVUS_ERROR_OK) {
            *status = 1;
        } else {
            *status = 0;
        }
    }

    return err;
}

salvus_error_type_t y70xx_application_configure_network_connection()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        int32_t timeout_ms = NETWORK_CONNECTION_CONFIGURATION_TIMEOUT_MS;

        int32_t elapsed_time_ms = 0;

        TickType_t start_tick = xTaskGetTickCount();

        SALVUS_LOGI(TAG, "Configuring network connection...");

        err = salvus_simcom_deregister_from_network(
            simcom_handle, timeout_ms - elapsed_time_ms);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Error disconnecting from network");
        }

        if (err == SALVUS_ERROR_OK) {
            err = _check_operation_timeout(start_tick, timeout_ms,
                                           &elapsed_time_ms);
        }

        if (err == SALVUS_ERROR_OK) {
            err = salvus_simcom_set_pdp_context(
                simcom_handle, PDP_CID, SALVUS_SIMCOM_IPV4, _access_point_name,
                timeout_ms - elapsed_time_ms);
            if (err != SALVUS_ERROR_OK) {
                SALVUS_LOGE(TAG, "Error setting PDP context");
            }
        }

        if (err == SALVUS_ERROR_OK) {
            err = _check_operation_timeout(start_tick, timeout_ms,
                                           &elapsed_time_ms);
        }

        if (err == SALVUS_ERROR_OK) {
            while (1) {
                err = _check_operation_timeout(start_tick, timeout_ms,
                                               &elapsed_time_ms);
                if (err != SALVUS_ERROR_OK) {
                    break;
                }

                err = salvus_simcom_auto_register_in_network(
                    simcom_handle, timeout_ms - elapsed_time_ms);
                if (err != SALVUS_ERROR_FAIL) {
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(NETWORK_REGISTERING_POLL_INTERVAL_MS));
            }

            if (err != SALVUS_ERROR_OK) {
                SALVUS_LOGE(TAG, "Error registering to network");
            }
        }
    }

    if (err == SALVUS_ERROR_OK) {
        SALVUS_LOGI(TAG, "Network connection configured");
    } else {
        SALVUS_LOGE(TAG, "Failed to configure network connection");
    }

    return err;
}

salvus_error_type_t y70xx_application_establish_network_connection()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        int32_t timeout_ms = ESTABLISH_NETWORK_CONNECTION_TIMEOUT_MS;

        int32_t elapsed_time_ms = 0;

        TickType_t start_tick = xTaskGetTickCount();

        SALVUS_LOGI(TAG, "Establishing network connection...");

        if (err == SALVUS_ERROR_OK) {
            while (1) {
                err = _check_operation_timeout(start_tick, timeout_ms,
                                               &elapsed_time_ms);
                if (err != SALVUS_ERROR_OK) {
                    break;
                }

                uint8_t attached = 0;
                err = salvus_simcom_query_network_attachment(
                    simcom_handle, &attached, timeout_ms - elapsed_time_ms);
                if (err == SALVUS_ERROR_OK) {
                    if (attached) {
                        break;
                    }
                } else {
                    break;
                }

                vTaskDelay(pdMS_TO_TICKS(NETWORK_ATTACHMENT_POLL_INTERVAL_MS));
            }

            if (err == SALVUS_ERROR_OK) {
                SALVUS_LOGI(TAG, "Network attachment successfully");
            } else {
                SALVUS_LOGE(TAG, "Error attaching network");
            }
        }

        if (err == SALVUS_ERROR_OK) {
            err = _check_operation_timeout(start_tick, timeout_ms,
                                           &elapsed_time_ms);
        }

        if (err == SALVUS_ERROR_OK) {
            int8_t rssi = 0;

            err = salvus_simcom_query_signal_quality(
                simcom_handle, &rssi, timeout_ms - elapsed_time_ms);
            if (err == SALVUS_ERROR_OK) {
                SALVUS_LOGI(TAG, "Connection RSSI: %d dbm", rssi);
            }
        }

        if (err == SALVUS_ERROR_OK) {
            SALVUS_LOGI(TAG, "Network connection established");
        } else {
            SALVUS_LOGE(TAG, "Failed to establish network connection");
        }
    }

    return err;
}

salvus_error_type_t y70xx_application_check_connection_status(uint8_t *status)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    salvus_simcom_handle_t simcom_handle = NULL;
    err = salvus_simcom_get_handle(SALVUS_Y70XX, 0, &simcom_handle);

    if (err == SALVUS_ERROR_OK) {
        err = salvus_simcom_query_network_attachment(
            simcom_handle, status, NETWORK_ATTACHMENT_POLL_INTERVAL_MS);
    }

    return err;
}