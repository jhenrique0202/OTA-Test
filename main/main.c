#include <stdio.h>

#include "salvus_error.h"
#include "salvus_logging.h"

#include "application.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    err = application_init();

    if (err == SALVUS_ERROR_OK) {
        err = application_start();
    }

    if (err != SALVUS_ERROR_OK) {
        SALVUS_LOGE(TAG, "Error: %d", err);
    }
}
