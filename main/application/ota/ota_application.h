#ifndef OTA_APPLICATION_H
#define OTA_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t ota_application_init();

salvus_error_type_t ota_application_start();

#ifdef __cplusplus
}
#endif

#endif // OTA_APPLICATION_H
