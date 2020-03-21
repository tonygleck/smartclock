// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <time.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

MOCKABLE_FUNCTION(, time_t, get_time);
MOCKABLE_FUNCTION(, struct tm*, get_time_value);

MOCKABLE_FUNCTION(, int, set_machine_time, time_t*, set_time);
