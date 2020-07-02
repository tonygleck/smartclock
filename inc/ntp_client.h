// Licensed under the MIT license. See LICENSE file in the project root for full license information.
#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

typedef struct NTP_CLIENT_INFO_TAG* NTP_CLIENT_HANDLE;

typedef enum NTP_OPERATION_RESULT_TAG
{
    NTP_OP_RESULT_SUCCESS,
    NTP_OP_RESULT_COMM_ERR,
    NTP_OP_RESULT_INVALID_DATA_ERR,
    NTP_OP_RESULT_TIMEOUT
} NTP_OPERATION_RESULT;

typedef void(*NTP_TIME_CALLBACK)(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time);

MOCKABLE_FUNCTION(, NTP_CLIENT_HANDLE, ntp_client_create);
MOCKABLE_FUNCTION(, void, ntp_client_destroy, NTP_CLIENT_HANDLE, handle);

MOCKABLE_FUNCTION(, int, ntp_client_get_time, NTP_CLIENT_HANDLE, handle, const char*, time_server, size_t, timeout_sec, NTP_TIME_CALLBACK, ntp_callback, void*, user_ctx);
MOCKABLE_FUNCTION(, void, ntp_client_process, NTP_CLIENT_HANDLE, handle);

MOCKABLE_FUNCTION(, int, ntp_client_set_time, const char*, time_server, size_t, timeout_sec);

#ifdef __cplusplus
}
#endif /* __cplusplus */
