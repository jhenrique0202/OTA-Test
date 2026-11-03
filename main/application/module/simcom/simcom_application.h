#ifndef SIMCOM_APPLICATION_H
#define SIMCOM_APPLICATION_H

#include <stdint.h>
#include <stdio.h>

#include "salvus_error.h"
#include "salvus_simcom.h"

#ifdef __cplusplus
extern "C" {
#endif

salvus_error_type_t
simcom_application_get_simcom_handle(salvus_simcom_handle_t *simcom_handle);

salvus_error_type_t simcom_application_init();

salvus_error_type_t simcom_application_start();

salvus_error_type_t simcom_application_turn_on();

salvus_error_type_t simcom_application_turn_off();

salvus_error_type_t simcom_application_check_power_status(uint8_t *status);

salvus_error_type_t simcom_application_configure_network_connection();

salvus_error_type_t simcom_application_establish_network_connection();

salvus_error_type_t simcom_application_check_connection_status(uint8_t *status);

#ifdef __cplusplus
}
#endif

#endif // SIMCOM_APPLICATION_H
