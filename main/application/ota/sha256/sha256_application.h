#ifndef SHA256_APPLICATION_H
#define SHA256_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#define SHA256_APPLICATION_HASH_SIZE 32

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t sha256_application_init();

salvus_error_type_t sha256_application_start();

salvus_error_type_t sha256_application_update(const uint8_t *buffer,
                                              size_t size);

salvus_error_type_t sha256_application_verify(
    const uint8_t expected_hash[SHA256_APPLICATION_HASH_SIZE]);

salvus_error_type_t sha256_application_abort();

#ifdef __cplusplus
}
#endif

#endif // OTA_APPLICATION_H
