#include "sha256_application.h"

#include <string.h>

#include "psa/crypto.h"

#include "salvus_logging.h"

static const char *TAG = "SHA256_APPLICATION";

static psa_hash_operation_t _operation = PSA_HASH_OPERATION_INIT;

salvus_error_type_t sha256_application_init()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (psa_crypto_init() != PSA_SUCCESS) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "Error initializing PSA crypto");
    }

    return err;
}

salvus_error_type_t sha256_application_start()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (psa_hash_setup(&_operation, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "Error starting SHA256 hash");
    }

    return err;
}

salvus_error_type_t sha256_application_update(const uint8_t *buffer,
                                              size_t size)
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (psa_hash_update(&_operation, buffer, size) != PSA_SUCCESS) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "Error updating SHA256 hash");
    }

    return err;
}

salvus_error_type_t
sha256_application_verify(const uint8_t expected_hash[SHA256_APPLICATION_HASH_SIZE])
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    psa_status_t status = psa_hash_verify(&_operation, expected_hash,
                                          SHA256_APPLICATION_HASH_SIZE);

    if (status == PSA_ERROR_INVALID_SIGNATURE) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "SHA256 hash mismatch");
    } else if (status != PSA_SUCCESS) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "Error verifying SHA256 hash");
    }

    /* Safe to call unconditionally: psa_hash_verify() leaves the operation
     * active on error, and a leftover active/error operation makes the next
     * psa_hash_setup() fail with PSA_ERROR_BAD_STATE. */
    psa_hash_abort(&_operation);

    return err;
}

salvus_error_type_t sha256_application_abort()
{
    salvus_error_type_t err = SALVUS_ERROR_OK;

    if (psa_hash_abort(&_operation) != PSA_SUCCESS) {
        err = SALVUS_ERROR_FAIL;

        SALVUS_LOGE(TAG, "Error aborting SHA256 hash");
    }

    return err;
}