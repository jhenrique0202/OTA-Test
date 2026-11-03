#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t application_init();

salvus_error_type_t application_start();

#ifdef __cplusplus
}
#endif

#endif // APPLICATION_H
