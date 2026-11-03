#include "nvm_ota.h"

#include <stdlib.h>
#include <string.h>

#include "memory_application.h"

#include "salvus_hal_nvm.h"

static const char *_namespace = "storage";

typedef enum
{
    SERVER_HOSTNAME_KEY,
    SERVER_CA_KEY,
    CLIENT_CERTIFICATE_KEY,
    CLIENT_KEY_KEY,
    MAX_KEY_TYPE
} keys_type_t;

static const char *_keys[MAX_KEY_TYPE] = {"ota_hostname", "ota_server_crt",
                                          "ota_client_crt", "ota_client_key"};

static salvus_error_type_t _read(keys_type_t key, char **value);

static salvus_error_type_t _read(keys_type_t key, char **value)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    const struct salvus_hal_nvm_locator_t locator = {
        .type = SALVUS_HAL_NVM_LOCATOR_KEY,
        .namespace_name = _namespace,
        .key = _keys[key]};

    char *data = NULL;
    size_t data_len = 0;

    salvus_hal_nvm_handle_t nvm_handle = NULL;
    err = memory_application_get_nvm_handle(&nvm_handle);

    if (err == SALVUS_ERROR_OK) {
        err = salvus_hal_nvm_read_str(nvm_handle, &locator, data, 0, &data_len);
    }

    if (err == SALVUS_ERROR_OK) {
        data = (char *) malloc(data_len);
        if (data != NULL) {
            memset(data, 0, data_len);
        } else {
            err = SALVUS_ERROR_NO_MEM;
        }
    }

    if (err == SALVUS_ERROR_OK) {
        err = salvus_hal_nvm_read_str(nvm_handle, &locator, data, data_len,
                                      &data_len);
    }

    if (err == SALVUS_ERROR_OK) {
        *value = (char *) data;
    } else {
        free(data);
        data = NULL;
    }

    return err;
}

salvus_error_type_t nvm_ota_read_server_hostname(char **value)
{
    return _read(SERVER_HOSTNAME_KEY, value);
}

salvus_error_type_t nvm_ota_read_server_certificate(char **value)
{
    return _read(SERVER_CA_KEY, value);
}

salvus_error_type_t nvm_ota_read_client_certificate(char **value)
{
    return _read(CLIENT_CERTIFICATE_KEY, value);
}

salvus_error_type_t nvm_ota_read_client_key(char **value)
{
    return _read(CLIENT_KEY_KEY, value);
}
