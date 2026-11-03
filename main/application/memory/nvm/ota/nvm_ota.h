#ifndef NVM_OTA_H
#define NVM_OTA_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t nvm_ota_read_server_hostname(char **value);

salvus_error_type_t nvm_ota_read_server_certificate(char **value);

salvus_error_type_t nvm_ota_read_client_certificate(char **value);

salvus_error_type_t nvm_ota_read_client_key(char **value);

#ifdef __cplusplus
}
#endif

#endif // NVM_OTA_H
