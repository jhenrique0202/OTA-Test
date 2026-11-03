#ifndef LOGGING_APPLICATION_H
#define LOGGING_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t logging_application_init();

#ifdef __cplusplus
}
#endif

#endif // LOGGING_APPLICATION_H
