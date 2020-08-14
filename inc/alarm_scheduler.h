// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef ALARM_SCHEDULER_H
#define ALARM_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include <time.h>

#include "umock_c/umock_c_prod.h"

typedef struct TIME_INFO_TAG
{
    uint8_t hour;
    uint8_t min;
    uint8_t sec;
} TIME_INFO;

typedef enum ALARM_STATE_RESULT_TAG
{
    ALARM_STATE_STOPPED,
    ALARM_STATE_TRIGGERED,
    ALARM_STATE_SNOOZE
} ALARM_STATE_RESULT;

typedef enum DayOfTheWeek_TAG
{
    NoDay = 0x0,
    Monday = 0x1,
    Tuesday = 0x2,
    Wednesday = 0x4,
    Thursday = 0x8,
    Friday = 0x10,
    Saturday = 0x20,
    Sunday = 0x40,
    OneTime = 0x80,
    Everyday = 0x7F
} DayOfTheWeek;

typedef struct ALARM_INFO_TAG
{
    TIME_INFO trigger_time;
    uint32_t trigger_days;
    uint8_t snooze_min;
    char* alarm_text;
    char* sound_file;
} ALARM_INFO;

typedef struct ALARM_SCHEDULER_TAG* SCHEDULER_HANDLE;

MOCKABLE_FUNCTION(, SCHEDULER_HANDLE, alarm_scheduler_create);
MOCKABLE_FUNCTION(, void, alarm_scheduler_destroy, SCHEDULER_HANDLE, handle);

MOCKABLE_FUNCTION(, int, alarm_scheduler_add_alarm, SCHEDULER_HANDLE, handle, const char*, alarm_text, const TIME_INFO*, time, uint32_t, trigger_days, const char*, sound_file, uint8_t, snooze_min);
MOCKABLE_FUNCTION(, int, alarm_scheduler_add_alarm_info, SCHEDULER_HANDLE, handle, const ALARM_INFO*, alarm_info);
MOCKABLE_FUNCTION(, int, alarm_scheduler_remove_alarm, SCHEDULER_HANDLE, handle, size_t, alarm_index);


MOCKABLE_FUNCTION(, size_t, alarm_scheduler_get_alarm_count, SCHEDULER_HANDLE, handle);
MOCKABLE_FUNCTION(, const ALARM_INFO*, alarm_scheduler_get_alarm, SCHEDULER_HANDLE, handle, size_t, index);

MOCKABLE_FUNCTION(, const ALARM_INFO*, alarm_scheduler_get_next_alarm, SCHEDULER_HANDLE, handle);
MOCKABLE_FUNCTION(, const ALARM_INFO*, alarm_scheduler_is_triggered, SCHEDULER_HANDLE, handle, const struct tm*, curr_time);
MOCKABLE_FUNCTION(, int, alarm_scheduler_snooze_alarm, SCHEDULER_HANDLE, handle, const ALARM_INFO*, alarm_info);
MOCKABLE_FUNCTION(, int, alarm_scheduler_get_next_day, const ALARM_INFO*, alarm_info);

MOCKABLE_FUNCTION(, bool, alarm_scheduler_is_morning, const TIME_INFO*, time_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // ALARM_SCHEDULER
