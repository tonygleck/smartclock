// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#pragma once

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

#include "alarm_scheduler.h"

MOCKABLE_FUNCTION(, int, clock_alarm_load, SCHEDULER_HANDLE sched_mgr);

MOCKABLE_FUNCTION(, int, clock_alarm_get_next, SCHEDULER_HANDLE sched_mgr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

