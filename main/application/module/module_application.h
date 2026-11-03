#ifndef MODULE_APPLICATION_H
#define MODULE_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t module_application_init();

salvus_error_type_t module_application_start();

#ifdef __cplusplus
}
#endif

#endif // MODULE_APPLICATION_H
