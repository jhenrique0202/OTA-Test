#ifndef MEMORY_APPLICATION_H
#define MEMORY_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_hal_nvm.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t
memory_application_get_nvm_handle(salvus_hal_nvm_handle_t *handle);

salvus_error_type_t memory_application_init();

#ifdef __cplusplus
}
#endif

#endif // MEMORY_APPLICATION_H
