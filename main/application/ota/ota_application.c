#include "ota_application.h"

#include <string.h>

#include "salvus_logging.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "cJSON.h"

#include "nvm_ota.h"
#include "sha256_application.h"
#include "simcom_application.h"

#define OTA_APPLICATION_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define OTA_APPLICATION_TASK_STACK_SIZE (4 * 1024)

#define OTA_APPLICATION_EVT_QUEUE_LENGTH 16

#define LAST_FIRMWARE_RELEASE_VERIFICATION_PERIOD_MS 60000

#define HTTP_INITIALIZATION_TIMEOUT_MS 15000
#define STATUS_CODE_READING_TIMEOUT_MS 1000
#define CONTENT_LENGTH_READING_TIMEOUT_MS 1000
#define CONTENT_READING_TIMEOUT_MS 1000

#define MANIFEST_REQUEST_TIMEOUT_MS 60000

#define FIRMWARE_DOWNLOAD_RETRY_PERIOD_MS 60000

#define MAX_CONTINUOS_FIRMWARE_DOWNLOADING_ATTEMPTS 3

#define FIRMWARE_CHUNCK_SIZE 1024 * 4
#define FIRMWARE_CHUNCK_REQUEST_TIMEOUT_MS 60000

static const char *TAG = "OTA_APPLICATION";

typedef enum
{
    WAITING_FOR_A_NEW_RELEASE_LAST_FIRMWARE_DOWNLOADING_STATE,
    DOWNLOADING_FIRMWARE_LAST_FIRMWARE_DOWNLOADING_STATE,
    LAST_FIRMWARE_DOWNLOADING_MAX_STATE_TYPE
} last_firmware_downloading_state_type_t;

typedef enum
{
    WAITING_FOR_APPLICATION_STARTING_APPLICATION_STATE,
    APPLICATION_RUNNING_APPLICATION_STATE,
    APPLCATION_STATE_MAX
} application_state_type_t;

typedef enum
{
    START_APPLICATION_APPLICATION_EVT,
    VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT,
    DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT,
    APPLICATION_EVT_MAX,
} application_evt_type_t;

struct application_evt_t;
typedef struct application_evt_t *application_evt_handle_t;

struct application_evt_t {
    application_evt_type_t type;
};

struct application_t;
typedef struct application_t *application_handle_t;

struct application_t {
    union {
        uint8_t flags;
        struct {
            uint8_t initialized : 1;
            uint8_t started : 1;
            uint8_t ota_started : 1;
            uint8_t reserved : 5;
        };
    };

    struct {
        TaskHandle_t handle;
    } task;

    struct {
        struct {
            QueueHandle_t handle;
        } queue;
    } evt;

    struct {
        salvus_simcom_handle_t handle;

        union {
            uint8_t flags;
            struct {
                uint8_t network_connection_configured : 1;
                uint8_t reserved : 7;
            };
        };
    } simcom;

    struct {
        esp_ota_handle_t handle;
    } ota;

    struct {
        struct {
            struct {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
            } version;
        } actual_release;

        struct {
            esp_partition_t *new_firmware_partition;

            struct {
                uint16_t major;
                uint16_t minor;
                uint16_t patch;
            } version;

            size_t size;

            uint8_t hash[SHA256_APPLICATION_HASH_SIZE];

            struct {
                last_firmware_downloading_state_type_t state;

                char *path;
                size_t offset;
            } download;

            struct {
                TimerHandle_t handle;
            } timer;
        } last_release;
    } firmware;
};

static void _last_firmware_release_timer_alarm_callback(TimerHandle_t handle);

static salvus_error_type_t _start_ota(application_handle_t handle);

static salvus_error_type_t _finish_ota(application_handle_t handle);

static salvus_error_type_t _cancel_ota(application_handle_t handle);

static salvus_error_type_t _parse_firmware_version(const char *version_str,
                                                   uint16_t *version_major,
                                                   uint16_t *version_minor,
                                                   uint16_t *version_patch);

static salvus_error_type_t _parse_firmware_hash(const char *hash_str,
                                                uint8_t *hash);

static salvus_error_type_t
_establish_network_connection(application_handle_t handle);

static salvus_error_type_t _init_http(application_handle_t handle);

static salvus_error_type_t
_query_last_firmware_release_data(application_handle_t handle);

static salvus_error_type_t
_download_last_firmware_relase(application_handle_t handle);

static salvus_error_type_t
_verify_last_firmware_hash(application_handle_t handle);

static salvus_error_type_t _process_last_firmware_downloading_evt(
    application_handle_t handle, struct application_evt_t *application_evt);

static void _application_task(void *arg);

static salvus_error_type_t _init(application_handle_t *handle);

static salvus_error_type_t _deinit(application_handle_t *handle);

static salvus_error_type_t _start(application_handle_t handle);

static application_handle_t _application_handle = NULL;

static void _last_firmware_release_timer_alarm_callback(TimerHandle_t handle)
{
    application_handle_t application_handle =
        (application_handle_t) pvTimerGetTimerID(handle);

    struct application_evt_t application_evt = {0};

    if (application_handle->firmware.last_release.download.state ==
        WAITING_FOR_A_NEW_RELEASE_LAST_FIRMWARE_DOWNLOADING_STATE) {
        application_evt.type = VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT;
    } else {
        application_evt.type = DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT;
    }

    xQueueSend(application_handle->evt.queue.handle, &application_evt,
               portMAX_DELAY);
}

static salvus_error_type_t _start_ota(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    handle->firmware.last_release.new_firmware_partition =
        (esp_partition_t *) esp_ota_get_next_update_partition(NULL);
    if (handle->firmware.last_release.new_firmware_partition == NULL) {
        SALVUS_LOGE(TAG, "Failed to select partition to write new firmware");

        err = SALVUS_ERROR_FAIL;
    }

    if (err == SALVUS_ERROR_OK) {
        esp_err_t esp_err = esp_ota_begin(
            (const esp_partition_t *)
                handle->firmware.last_release.new_firmware_partition,
            OTA_SIZE_UNKNOWN, &handle->ota.handle);
        if (esp_err != ESP_OK) {
            err = SALVUS_ERROR_FAIL;

            SALVUS_LOGE(TAG, "Failed to begin OTA procedure");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        handle->ota_started = 1;
    }

    return err;
}

static salvus_error_type_t _finish_ota(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    esp_ota_end(handle->ota.handle);
    handle->ota.handle = 0;

    esp_err_t esp_err = esp_ota_set_boot_partition(
        (const esp_partition_t *)
            handle->firmware.last_release.new_firmware_partition);

    if (esp_err != ESP_OK) {
        SALVUS_LOGE(TAG, "Error setting boot partition for next reboot");

        err = SALVUS_ERROR_FAIL;
    }

    return err;
}

static salvus_error_type_t _cancel_ota(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    esp_ota_end(handle->ota.handle);
    handle->ota.handle = 0;

    return err;
}

static salvus_error_type_t _parse_firmware_version(const char *version_str,
                                                   uint16_t *version_major,
                                                   uint16_t *version_minor,
                                                   uint16_t *version_patch)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (version_str == NULL) {
        err = SALVUS_ERROR_INVALID_ARG;
    }

    if (err == SALVUS_ERROR_OK) {
        const char *cursor = version_str;

        long values[3] = {0};
        for (int i = 0; i < 3; i++) {
            char *end = NULL;
            values[i] = strtol(cursor, &end, 10);

            if (end == cursor) {
                err = SALVUS_ERROR_INVALID_RESPONSE;
            }

            if (err == SALVUS_ERROR_OK) {
                cursor = end + 1;
            }
        }

        if (err == SALVUS_ERROR_OK) {
            *version_major = values[0];
            *version_minor = values[1];
            *version_patch = values[2];
        }
    }

    return err;
}

static int _hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static salvus_error_type_t _parse_firmware_hash(const char *hash_str,
                                                uint8_t *hash)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    for (size_t i = 0; i < SHA256_APPLICATION_HASH_SIZE; i++) {
        int hi = _hex_nibble(hash_str[2 * i]);
        int lo = _hex_nibble(hash_str[2 * i + 1]);

        if (hi < 0 || lo < 0) {
            err = SALVUS_ERROR_INVALID_ARG;
        }

        hash[i] = (uint8_t) ((hi << 4) | lo);
    }

    return err;
}

static salvus_error_type_t
_establish_network_connection(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (! handle->simcom.network_connection_configured) {
        err = simcom_application_configure_network_connection();

        if (err == SALVUS_ERROR_OK) {
            handle->simcom.network_connection_configured = 1;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = simcom_application_establish_network_connection();

        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to establish network connection");

            handle->simcom.network_connection_configured = 0;
        }
    }

    return err;
}

static salvus_error_type_t _init_http(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    char *server_hostname = NULL;
    char *server_ca = NULL;
    char *client_cert = NULL;
    char *client_key = NULL;

    err = nvm_ota_read_server_hostname(&server_hostname);
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to read server hostname from nvm");
    }

    if (err == SALVUS_ERROR_OK) {
        err = nvm_ota_read_server_certificate(&server_ca);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to read server certificate from nvm");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = nvm_ota_read_client_certificate(&client_cert);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to read client certificate from nvm");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = nvm_ota_read_client_key(&client_key);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to read client key from nvm");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        salvus_simcom_http_config_t http_config = {0};
        http_config.server.address.hostname = server_hostname;
        http_config.server.address.port = 443;
        http_config.server.scheme = SALVUS_SIMCOM_HTTPS;
        http_config.credentials.authentication.server_cert = server_ca;
        http_config.credentials.authentication.client_cert = client_cert;
        http_config.credentials.authentication.client_key = client_key;

        err = salvus_simcom_init_http(handle->simcom.handle, &http_config,
                                      HTTP_INITIALIZATION_TIMEOUT_MS);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Failed to init HTTP");
        }
    }

    free(server_hostname);
    server_hostname = NULL;

    free(server_ca);
    server_ca = NULL;

    free(client_cert);
    client_cert = NULL;

    free(client_key);
    client_key = NULL;

    return err;
}

static salvus_error_type_t
_query_last_firmware_release_data(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    uint8_t *content = NULL;
    size_t content_length = 0;

    err = simcom_application_turn_on();

    if (err == SALVUS_ERROR_OK) {
        err = _establish_network_connection(handle);

        if (err == SALVUS_ERROR_OK) {
            SALVUS_LOGI(TAG, "Requesting manifest.json file...");

            err = _init_http(handle);

            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_send_http_request(
                    handle->simcom.handle, SALVUS_SIMCOM_GET_HTTP_REQUEST,
                    "/fw/poc/manifest.json", MANIFEST_REQUEST_TIMEOUT_MS);

                if (err != SALVUS_ERROR_OK) {
                    SALVUS_LOGE(TAG, "Failed to request manifest");

                    handle->simcom.network_connection_configured = 0;
                }
            }

            int status_code = 0;
            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_get_http_response_status_code(
                    handle->simcom.handle, &status_code,
                    STATUS_CODE_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "Status code: %d", status_code);
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response status code");
                }
            }

            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_get_http_response_content_length(
                    handle->simcom.handle, &content_length,
                    CONTENT_LENGTH_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "Content length: %zu", content_length);

                    if (content_length == 0) {
                        err = SALVUS_ERROR_INVALID_RESPONSE;
                    }
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response content length");
                }
            }

            if (err == SALVUS_ERROR_OK) {
                content = (uint8_t *) malloc(content_length + 1);
                if (content != NULL) {
                    memset(content, 0, content_length + 1);
                } else {
                    err = SALVUS_ERROR_NO_MEM;
                }
            }

            if (err == SALVUS_ERROR_OK) {
                size_t content_read = 0;

                err = salvus_simcom_read_http_response_content(
                    handle->simcom.handle, content, content_length,
                    &content_read, CONTENT_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "Content: %s", content);
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response content");
                }
            }

            salvus_simcom_deinit_http(handle->simcom.handle, -1);
        }

        simcom_application_turn_off();
    }

    cJSON *content_object = NULL;
    if (err == SALVUS_ERROR_OK) {
        content_object = cJSON_Parse((const char *) content);
        if (content_object == NULL) {
            SALVUS_LOGE(TAG, "Failed to parse response content");

            err = SALVUS_ERROR_NOT_FOUND;
        }
    }

    cJSON *version_string = NULL;
    if (err == SALVUS_ERROR_OK) {
        version_string =
            cJSON_GetObjectItemCaseSensitive(content_object, "version");
        if (version_string == NULL) {
            SALVUS_LOGE(TAG, "Failed to get last firmware version");

            err = SALVUS_ERROR_NOT_FOUND;
        }
    }

    cJSON *size_number = NULL;
    if (err == SALVUS_ERROR_OK) {
        size_number = cJSON_GetObjectItemCaseSensitive(content_object, "size");
        if (size_number == NULL) {
            SALVUS_LOGE(TAG, "Failed to get last firmware size");

            err = SALVUS_ERROR_NOT_FOUND;
        }
    }

    cJSON *hash_string = NULL;
    if (err == SALVUS_ERROR_OK) {
        hash_string =
            cJSON_GetObjectItemCaseSensitive(content_object, "sha256");
        if (hash_string == NULL) {
            SALVUS_LOGE(TAG, "Failed to get last firmware hash");

            err = SALVUS_ERROR_NOT_FOUND;
        }
    }

    cJSON *download_path_string = NULL;
    if (err == SALVUS_ERROR_OK) {
        download_path_string =
            cJSON_GetObjectItemCaseSensitive(content_object, "path");
        if (download_path_string == NULL) {
            SALVUS_LOGE(TAG, "Failed to get last firmware download path");

            err = SALVUS_ERROR_NOT_FOUND;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = _parse_firmware_version(
            version_string->valuestring,
            &handle->firmware.last_release.version.major,
            &handle->firmware.last_release.version.minor,
            &handle->firmware.last_release.version.patch);
    }

    if (err == SALVUS_ERROR_OK) {
        err = _parse_firmware_hash(hash_string->valuestring,
                                   handle->firmware.last_release.hash);
    }

    if (err == SALVUS_ERROR_OK) {
        if (handle->firmware.last_release.download.path != NULL) {
            free(handle->firmware.last_release.download.path);
            handle->firmware.last_release.download.path = NULL;
        }

        size_t download_path_size = strlen(download_path_string->valuestring);

        handle->firmware.last_release.download.path =
            (char *) malloc(download_path_size + 1);
        if (handle->firmware.last_release.download.path != NULL) {
            memset(handle->firmware.last_release.download.path, 0,
                   download_path_size + 1);
            strcpy(handle->firmware.last_release.download.path,
                   download_path_string->valuestring);
        } else {
            err = SALVUS_ERROR_NO_MEM;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        handle->firmware.last_release.size = size_number->valueint;
    }

    if (err == SALVUS_ERROR_OK) {
        SALVUS_LOGI(TAG, "Last firmware:");
        SALVUS_LOGI(TAG, "  version: %" PRIu16 ".%" PRIu16 ".%" PRIu16,
                    handle->firmware.last_release.version.major,
                    handle->firmware.last_release.version.minor,
                    handle->firmware.last_release.version.patch);
        SALVUS_LOGI(TAG, "  size: %zu", handle->firmware.last_release.size);
        SALVUS_LOGI(TAG, "  download path: %s",
                    handle->firmware.last_release.download.path);
    }

    if (err != SALVUS_ERROR_OK) {
        if (handle->firmware.last_release.download.path != NULL) {
            free(handle->firmware.last_release.download.path);
            handle->firmware.last_release.download.path = NULL;
        }
    }

    if (content_object != NULL) {
        cJSON_Delete(content_object);
        content_object = NULL;
    }

    if (content != NULL) {
        free(content);
        content = NULL;
    }

    return err;
}

static salvus_error_type_t
_download_last_firmware_relase(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (handle->firmware.last_release.download.offset == 0) {
        SALVUS_LOGI(TAG, "Starting new firmware download");
        SALVUS_LOGI(TAG, "  version: %" PRIu16 ".%" PRIu16 ".%" PRIu16,
                    handle->firmware.last_release.version.major,
                    handle->firmware.last_release.version.minor,
                    handle->firmware.last_release.version.patch);
        SALVUS_LOGI(TAG, "  size: %zu bytes",
                    handle->firmware.last_release.size);
    } else {
        SALVUS_LOGI(TAG, "Returning new firmware download");
        SALVUS_LOGI(TAG, "   version: %" PRIu16 ".%" PRIu16 ".%" PRIu16,
                    handle->firmware.last_release.version.major,
                    handle->firmware.last_release.version.minor,
                    handle->firmware.last_release.version.patch);
        SALVUS_LOGI(TAG, "  offset: %zu bytes",
                    handle->firmware.last_release.download.offset);
        SALVUS_LOGI(TAG, "  size: %zu bytes",
                    handle->firmware.last_release.size);
    }

    int downloading_attempts = 0;
    while (1) {
        SALVUS_LOGI(TAG, "New firmware continuos downloading attempt %d/%d",
                    downloading_attempts,
                    MAX_CONTINUOS_FIRMWARE_DOWNLOADING_ATTEMPTS);

        uint8_t stop_firmware_downloading = 0;
        while (! stop_firmware_downloading) {
            uint8_t simcom_power_status = 0;
            err = simcom_application_check_power_status(&simcom_power_status);

            if (err == SALVUS_ERROR_OK) {
                if (! simcom_power_status) {
                    err = simcom_application_turn_on();

                    if (err == SALVUS_ERROR_OK) {
                        simcom_power_status = 1;
                    }
                }
            }

            uint8_t connection_status = 0;
            if (err == SALVUS_ERROR_OK) {
                err = simcom_application_check_connection_status(
                    &connection_status);

                if (err == SALVUS_ERROR_OK) {
                    if (! connection_status) {
                        err = _establish_network_connection(handle);

                        if (err == SALVUS_ERROR_OK) {
                            connection_status = 1;
                        }
                    }
                }
            }

            uint8_t http_initialized = 0;
            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_check_http_initialization(
                    handle->simcom.handle, &http_initialized, -1);

                if (err == SALVUS_ERROR_OK) {
                    if (! http_initialized) {
                        err = _init_http(handle);

                        if (err == SALVUS_ERROR_OK) {
                            http_initialized = 1;
                        }
                    }
                }
            }

            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_clear_http_request_header(
                    handle->simcom.handle, -1);
            }

            size_t chunck_size = 0;
            if (err == SALVUS_ERROR_OK) {
                chunck_size =
                    handle->firmware.last_release.download.offset +
                                FIRMWARE_CHUNCK_SIZE <
                            handle->firmware.last_release.size
                        ? FIRMWARE_CHUNCK_SIZE
                        : handle->firmware.last_release.size -
                              handle->firmware.last_release.download.offset;

                char range_value[25] = {0};
                snprintf(range_value, sizeof(range_value), "bytes=%zu-%zu",
                         handle->firmware.last_release.download.offset,
                         handle->firmware.last_release.download.offset +
                             chunck_size - 1);

                err = salvus_simcom_add_http_request_header_field(
                    handle->simcom.handle, "Range", range_value, -1);
            }

            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_add_http_request_header_field(
                    handle->simcom.handle, "Accept", "*/*", -1);
            }

            if (err == SALVUS_ERROR_OK) {
                SALVUS_LOGI(TAG, "Requesting new firmware chunck downloading");
                SALVUS_LOGI(TAG, "  offset: %zu bytes",
                            handle->firmware.last_release.download.offset);
                SALVUS_LOGI(TAG, "  size: %zu bytes", chunck_size);

                err = salvus_simcom_send_http_request(
                    handle->simcom.handle, SALVUS_SIMCOM_GET_HTTP_REQUEST,
                    handle->firmware.last_release.download.path,
                    FIRMWARE_CHUNCK_REQUEST_TIMEOUT_MS);

                if (err != SALVUS_ERROR_OK) {
                    SALVUS_LOGE(TAG, "Failed to request firmware chunck");

                    handle->simcom.network_connection_configured = 0;
                }
            }

            int status_code = 0;
            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_get_http_response_status_code(
                    handle->simcom.handle, &status_code,
                    STATUS_CODE_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "Status code: %d", status_code);
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response status code");
                }
            }

            size_t content_length = 0;
            if (err == SALVUS_ERROR_OK) {
                err = salvus_simcom_get_http_response_content_length(
                    handle->simcom.handle, &content_length,
                    CONTENT_LENGTH_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "Content length: %zu", content_length);

                    if (content_length == 0) {
                        err = SALVUS_ERROR_INVALID_RESPONSE;
                    }
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response content length");
                }
            }

            uint8_t *content = NULL;
            if (err == SALVUS_ERROR_OK) {
                content = (uint8_t *) malloc(content_length + 1);
                if (content != NULL) {
                    memset(content, 0, content_length + 1);
                } else {
                    err = SALVUS_ERROR_NO_MEM;
                }
            }

            if (err == SALVUS_ERROR_OK) {
                size_t content_read = 0;

                err = salvus_simcom_read_http_response_content(
                    handle->simcom.handle, content, content_length,
                    &content_read, CONTENT_READING_TIMEOUT_MS);

                if (err == SALVUS_ERROR_OK) {
                    handle->firmware.last_release.download.offset +=
                        chunck_size;
                } else {
                    SALVUS_LOGE(TAG, "Failed to get response content");
                }
            }

            if (err == SALVUS_ERROR_OK) {
                esp_err_t esp_err =
                    esp_ota_write(handle->ota.handle, content, content_length);

                if (esp_err == ESP_OK) {
                    float downloading_progress =
                        (100.0 *
                         handle->firmware.last_release.download.offset) /
                        handle->firmware.last_release.size;

                    SALVUS_LOGI(TAG,
                                "Downloading progress: %f%% (%zu/%zu total)",
                                downloading_progress,
                                handle->firmware.last_release.download.offset,
                                handle->firmware.last_release.size);
                } else {
                    SALVUS_LOGE(TAG, "Failed to write new firmware "
                                     "chunck in memory");

                    err = SALVUS_ERROR_FAIL;
                }
            }

            if (content != NULL) {
                free(content);
                content = NULL;
            }

            if (handle->firmware.last_release.download.offset >=
                handle->firmware.last_release.size) {
                stop_firmware_downloading = 1;
            }

            if (err != SALVUS_ERROR_OK) {
                downloading_attempts++;

                stop_firmware_downloading = 1;
            }

            if (stop_firmware_downloading) {
                salvus_simcom_deinit_http(handle->simcom.handle,
                                          HTTP_INITIALIZATION_TIMEOUT_MS);
                simcom_application_turn_off();

                http_initialized = 0;
                simcom_power_status = 0;
            }
        }

        if (err == SALVUS_ERROR_OK) {
            SALVUS_LOGI(TAG, "Finished the downloading of the new firmware");

            handle->firmware.last_release.download.offset = 0;
            break;
        } else {
            if (downloading_attempts >=
                MAX_CONTINUOS_FIRMWARE_DOWNLOADING_ATTEMPTS) {
                SALVUS_LOGE(TAG, "Reached new firmware continuos downloading "
                                 "maximun attempt");
                break;
            }
        }
    }

    return err;
}

static salvus_error_type_t
_verify_last_firmware_hash(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    SALVUS_LOGI(TAG, "Verifying new firmware hash...");

    err = sha256_application_init();
    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Failed to initialize SHA256 hashing");
    }

    uint8_t hashing_started = 0;
    if (err == SALVUS_ERROR_OK) {
        err = sha256_application_start();

        if (err == SALVUS_ERROR_OK) {
            hashing_started = 1;
        } else {
            SALVUS_LOGE(TAG, "Failed to start SHA256 hashing");
        }
    }

    uint8_t *chunck = NULL;
    if (err == SALVUS_ERROR_OK) {
        chunck = (uint8_t *) malloc(FIRMWARE_CHUNCK_SIZE);
        if (chunck == NULL) {
            err = SALVUS_ERROR_NO_MEM;

            SALVUS_LOGE(TAG, "Failed to allocate firmware hashing chunck");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        size_t firmware_offset = 0;

        size_t chunck_size = 0;
        while (1) {
            chunck_size =
                (handle->firmware.last_release.size - firmware_offset <
                 FIRMWARE_CHUNCK_SIZE)
                    ? handle->firmware.last_release.size - firmware_offset
                    : FIRMWARE_CHUNCK_SIZE;

            esp_err_t esp_err = esp_partition_read(
                (const esp_partition_t *)
                    handle->firmware.last_release.new_firmware_partition,
                firmware_offset, chunck, chunck_size);

            if (esp_err == ESP_OK) {
                err = sha256_application_update(chunck, chunck_size);
            } else {
                err = SALVUS_ERROR_FAIL;

                SALVUS_LOGE(TAG,
                            "Failed to read new firmware for hashing "
                            "(offset %zu, size %zu)",
                            firmware_offset, chunck_size);
            }

            if (err == SALVUS_ERROR_OK) {
                firmware_offset += chunck_size;

                if (firmware_offset >= handle->firmware.last_release.size) {
                    break;
                }
            } else {
                break;
            }
        }
    }

    if (chunck != NULL) {
        free(chunck);
        chunck = NULL;
    }

    if (err == SALVUS_ERROR_OK) {
        err = sha256_application_verify(handle->firmware.last_release.hash);

        hashing_started = 0;

        if (err == SALVUS_ERROR_OK) {
            SALVUS_LOGI(TAG, "New firmware hash verified successfully");
        } else {
            SALVUS_LOGE(TAG, "New firmware hash verification failed");
        }
    } else if (hashing_started) {
        /* The hash operation was started but never reached
         * sha256_application_verify (read/update failed partway): abort it
         * explicitly, otherwise it stays active and the next OTA attempt's
         * sha256_application_start() fails. */
        sha256_application_abort();
    }

    return err;
}

static salvus_error_type_t _process_last_firmware_downloading_evt(
    application_handle_t handle, struct application_evt_t *application_evt)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    switch (handle->firmware.last_release.download.state) {
    case WAITING_FOR_A_NEW_RELEASE_LAST_FIRMWARE_DOWNLOADING_STATE :
        switch (application_evt->type) {
        case VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT :
        {
            xTimerStop(handle->firmware.last_release.timer.handle,
                       portMAX_DELAY);

            err = _query_last_firmware_release_data(handle);

            uint8_t available_firmware_relase = 0;
            if (err == SALVUS_ERROR_OK) {
                if (handle->firmware.last_release.version.major >
                        handle->firmware.actual_release.version.major ||
                    handle->firmware.last_release.version.minor >
                        handle->firmware.actual_release.version.minor ||
                    handle->firmware.last_release.version.patch >
                        handle->firmware.actual_release.version.patch) {
                    available_firmware_relase = 1;
                }

                if (! available_firmware_relase) {
                    xTimerChangePeriod(
                        handle->firmware.last_release.timer.handle,
                        pdMS_TO_TICKS(
                            LAST_FIRMWARE_RELEASE_VERIFICATION_PERIOD_MS),
                        portMAX_DELAY);

                    xTimerReset(handle->firmware.last_release.timer.handle,
                                portMAX_DELAY);
                    xTimerStart(handle->firmware.last_release.timer.handle,
                                portMAX_DELAY);
                } else {
                    handle->firmware.last_release.download.state =
                        DOWNLOADING_FIRMWARE_LAST_FIRMWARE_DOWNLOADING_STATE;

                    struct application_evt_t application_evt = {
                        .type = DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT};

                    xQueueSend(handle->evt.queue.handle, &application_evt,
                               portMAX_DELAY);
                }
            }
        } break;

        default :
            break;
        }
        break;

    case DOWNLOADING_FIRMWARE_LAST_FIRMWARE_DOWNLOADING_STATE :
        switch (application_evt->type) {
        case DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT :
            xTimerStop(handle->firmware.last_release.timer.handle,
                       portMAX_DELAY);

            if (! handle->ota_started) {
                err = _start_ota(handle);
            }

            if (err == SALVUS_ERROR_OK) {
                err = _download_last_firmware_relase(handle);

                if (err != SALVUS_ERROR_OK) {
                    xTimerChangePeriod(
                        handle->firmware.last_release.timer.handle,
                        pdMS_TO_TICKS(FIRMWARE_DOWNLOAD_RETRY_PERIOD_MS),
                        portMAX_DELAY);

                    xTimerReset(handle->firmware.last_release.timer.handle,
                                portMAX_DELAY);
                    xTimerStart(handle->firmware.last_release.timer.handle,
                                portMAX_DELAY);
                }
            }

            if (err == SALVUS_ERROR_OK) {
                err = _verify_last_firmware_hash(handle);

                if (err == SALVUS_ERROR_OK) {
                    SALVUS_LOGI(TAG, "New firmware is valid, finishing OTA "
                                     "update");

                    err = _finish_ota(handle);

                    if (err == SALVUS_ERROR_OK) {
                        esp_restart();
                    }
                } else {
                    SALVUS_LOGE(TAG, "New firmware hash verification "
                                     "failed, cancelling OTA update");

                    _cancel_ota(handle);
                }
            }

            break;

        default :
            break;
        }

    default :
        break;
    }

    return err;
}

static void _application_task(void *arg)
{
    application_handle_t application_handle = (application_handle_t) arg;

    application_state_type_t application_state =
        WAITING_FOR_APPLICATION_STARTING_APPLICATION_STATE;
    while (1) {
        struct application_evt_t application_evt = {0};
        xQueueReceive(application_handle->evt.queue.handle, &application_evt,
                      portMAX_DELAY);

        switch (application_state) {
        case WAITING_FOR_APPLICATION_STARTING_APPLICATION_STATE :
            switch (application_evt.type) {
            case START_APPLICATION_APPLICATION_EVT :
            {
                struct application_evt_t application_evt = {
                    .type = VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT};

                xQueueSend(application_handle->evt.queue.handle,
                           &application_evt, portMAX_DELAY);

                application_state = APPLICATION_RUNNING_APPLICATION_STATE;
            } break;

            default :
                break;
            }
            break;

        case APPLICATION_RUNNING_APPLICATION_STATE :
            switch (application_evt.type) {
            case VERIFY_LAST_FIRMWARE_RELEASE_APPLICATION_EVT :
            case DOWNLOAD_LAST_FIRMWARE_RELEASE_APPLICATION_EVT :
                _process_last_firmware_downloading_evt(application_handle,
                                                       &application_evt);

                application_state = APPLICATION_RUNNING_APPLICATION_STATE;
                break;

            default :
                break;
            }
            break;

        default :
            break;
        }
    }
}

static salvus_error_type_t _init(application_handle_t *handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    *handle = malloc(sizeof(struct application_t));
    if (*handle != NULL) {
        *(*handle) = (struct application_t){0};
    } else {
        err = SALVUS_ERROR_NO_MEM;
    }

    if (err == SALVUS_ERROR_OK) {
        err = simcom_application_get_simcom_handle(&(*handle)->simcom.handle);
        if (err != SALVUS_ERROR_OK) {
            SALVUS_LOGE(TAG, "Error getting SIMCom module handle");
        }
    }

    if (err == SALVUS_ERROR_OK) {
        (*handle)->firmware.last_release.timer.handle = xTimerCreate(
            NULL, pdMS_TO_TICKS(LAST_FIRMWARE_RELEASE_VERIFICATION_PERIOD_MS),
            pdTRUE, (void *) *handle,
            _last_firmware_release_timer_alarm_callback);
        if ((*handle)->firmware.last_release.timer.handle == NULL) {
            err = SALVUS_ERROR_NO_MEM;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        (*handle)->evt.queue.handle = xQueueCreate(
            OTA_APPLICATION_EVT_QUEUE_LENGTH, sizeof(struct application_evt_t));
        if ((*handle)->evt.queue.handle == NULL) {
            err = SALVUS_ERROR_NO_MEM;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        xTaskCreate(_application_task, "OTA application task",
                    OTA_APPLICATION_TASK_STACK_SIZE, (void *) *handle,
                    OTA_APPLICATION_TASK_PRIORITY, &(*handle)->task.handle);

        if ((*handle)->task.handle == NULL) {
            SALVUS_LOGE(TAG, "Error creating OTA application");

            err = SALVUS_ERROR_NO_MEM;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        (*handle)->initialized = 1;
    } else {
        _deinit(handle);

        SALVUS_LOGE(TAG, "Error initializing OTA application");
    }

    return err;
}

static salvus_error_type_t _deinit(application_handle_t *handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (handle != NULL && *handle != NULL) {
        if ((*handle)->task.handle != NULL) {
            vTaskDelete((*handle)->task.handle);
            (*handle)->task.handle = NULL;
        }

        if ((*handle)->evt.queue.handle != NULL) {
            vQueueDelete((*handle)->evt.queue.handle);
            (*handle)->evt.queue.handle = NULL;
        }

        if ((*handle)->firmware.last_release.timer.handle != NULL) {
            xTimerDelete((*handle)->firmware.last_release.timer.handle,
                         portMAX_DELAY);
            (*handle)->firmware.last_release.timer.handle = NULL;
        }

        free(*handle);
        *handle = NULL;
    }

    return err;
}

static salvus_error_type_t _start(application_handle_t handle)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    const esp_app_desc_t *app_desc = esp_app_get_description();
    if (app_desc == NULL) {
        err = SALVUS_ERROR_FAIL;
    }

    if (err == SALVUS_ERROR_OK) {
        err = _parse_firmware_version(
            app_desc->version, &handle->firmware.actual_release.version.major,
            &handle->firmware.actual_release.version.minor,
            &handle->firmware.actual_release.version.patch);
    }

    if (err == SALVUS_ERROR_OK) {
        SALVUS_LOGI(TAG, "Actual firmware:");
        SALVUS_LOGI(TAG, "   version: %" PRIu16 ".%" PRIu16 ".%" PRIu16,
                    handle->firmware.actual_release.version.major,
                    handle->firmware.actual_release.version.minor,
                    handle->firmware.actual_release.version.patch);
        SALVUS_LOGI(TAG, "   compiled in %s-%s", app_desc->date,
                    app_desc->time);
    }

    if (err == SALVUS_ERROR_OK) {
        handle->started = 1;

        struct application_evt_t application_evt = {
            .type = START_APPLICATION_APPLICATION_EVT};
        xQueueSend(handle->evt.queue.handle, &application_evt, -1);
    }

    return err;
}

salvus_error_type_t ota_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (_application_handle != NULL) {
        SALVUS_LOGE(TAG, "OTA application already initialized");

        err = SALVUS_ERROR_NOT_ALLOWED;
    }

    if (err == SALVUS_ERROR_OK) {
        err = _init(&_application_handle);
    }

    return err;
}

salvus_error_type_t ota_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (_application_handle == NULL) {
        SALVUS_LOGE(TAG, "OTA application not initialized");

        err = SALVUS_ERROR_NOT_ALLOWED;
    }

    if (err == SALVUS_ERROR_OK) {
        err = _start(_application_handle);
    }

    return err;
}