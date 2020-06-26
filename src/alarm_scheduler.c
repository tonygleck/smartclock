
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"
#include "lib-util-c/item_list.h"
#include "lib-util-c/crt_extensions.h"
#include "alarm_scheduler.h"
#include "time_mgr.h"

#define INVALID_TRIGGERED_DATE      400

typedef struct ALARM_STORAGE_ITEM_TAG
{
    ALARM_INFO alarm_info;
    uint16_t triggered_date;
} ALARM_STORAGE_ITEM;

typedef struct ALARM_SCHEDULER_TAG
{
    ITEM_LIST_HANDLE sched_list;
} ALARM_SCHEDULER;

static void destroy_schedule_list_cb(void* user_ctx, void* item)
{
    ALARM_SCHEDULER* alarm_sched = (ALARM_SCHEDULER*)user_ctx;
    if (alarm_sched != NULL)
    {
        ALARM_STORAGE_ITEM* alarm_info = (ALARM_STORAGE_ITEM*)item;
        free(alarm_info->alarm_info.sound_file);
        free(alarm_info->alarm_info.alarm_text);
        free(alarm_info);
    }
}

static uint16_t get_current_day_from_value(int wday)
{
    uint16_t result;
    switch (wday)
    {
        case 0:
            result = Sunday;
            break;
        case 1:
            result = Monday;
            break;
        case 2:
            result = Tuesday;
            break;
        case 3:
            result = Wednesday;
            break;
        case 4:
            result = Thursday;
            break;
        case 5:
            result = Friday;
            break;
        case 6:
            result = Saturday;
            break;
        default:
            result = Everyday;
            break;
    }
    return result;
}

static uint16_t get_current_day(const struct tm* curr_time)
{
    return get_current_day_from_value(curr_time->tm_wday);
}

static int get_next_trigger_day(const struct tm* current_tm, uint32_t trigger_day, const TIME_INFO* trigger_time)
{
    int result;
    result = 7;
    int target_day = current_tm->tm_wday;
    for (uint16_t index = 0; index < 7; index++)
    {
        uint16_t compare_value = get_current_day_from_value(target_day);
        if (compare_value & trigger_day)
        {
            if (target_day == current_tm->tm_wday)
            {
                // If the current hour is greater than the trigger hour
                // then it's too late
                if ((trigger_time->hour > current_tm->tm_hour) || (trigger_time->min > current_tm->tm_min))
                {
                    result = target_day;
                    break;
                }
            }
            else
            {
                result = target_day;
                break;
            }
        }
        if (target_day == 6)
        {
            target_day = 0;
        }
        else
        {
            target_day++;
        }
    }
    return result;
}

static int get_days_till_trigger(const ALARM_INFO* alarm_1, const struct tm* curr_time)
{
    int result = 0;
    int target_day = curr_time->tm_wday;
    do
    {
        uint16_t compare_value = get_current_day_from_value(target_day);
        if (compare_value & alarm_1->trigger_days)
        {
            if (result >= 7)
            {
                break;
            }
            else
            {
                if (target_day == curr_time->tm_wday)
                {
                    if (alarm_1->trigger_time.hour == curr_time->tm_hour)
                    {
                        if (alarm_1->trigger_time.min > curr_time->tm_min)
                        {
                            break;
                        }
                    }
                    // If the current hour is greater than the trigger hour
                    // then it's too late
                    else if (alarm_1->trigger_time.hour > curr_time->tm_hour)
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        target_day = target_day == 6 ? 0 : target_day+1;
        result++;
    } while (true);
    return result;
}

static bool is_alarm_initial_sooner(const ALARM_INFO* ai_initial, const ALARM_INFO* ai_compare, const struct tm* curr_time)
{
    bool result;
    // Convert all time to the number of days till the alarm
    int init_val_delta = get_days_till_trigger(ai_initial, curr_time);
    int cmp_val_delta = get_days_till_trigger(ai_compare, curr_time);
    if (init_val_delta < cmp_val_delta)
    {
        result = false;
    }
    else if (cmp_val_delta < init_val_delta)
    {
        result = true;
    }
    else
    {
        // The days are equal, need to evaluate the hours
        if (ai_initial->trigger_time.hour == ai_compare->trigger_time.hour)
        {
            // Compare minutes
            result = ai_initial->trigger_time.min > ai_compare->trigger_time.min;
        }
        else
        {
            // Let's make sure that one hour has not passed
            // If both the hours are either past or both are before the current hour the just compare them
            if (
                (ai_initial->trigger_time.hour >= curr_time->tm_hour && ai_compare->trigger_time.hour >= curr_time->tm_hour) ||
                (ai_initial->trigger_time.hour <= curr_time->tm_hour && ai_compare->trigger_time.hour <= curr_time->tm_hour)
               )
            {
                result = ai_initial->trigger_time.hour > ai_compare->trigger_time.hour;
            }
            else
            {
                // If the inital hour is past than the current time (it still is coming up) and the compare hour is before
                if (ai_initial->trigger_time.hour >= curr_time->tm_hour && ai_compare->trigger_time.hour < curr_time->tm_hour)
                {
                    result = false;
                }
                else
                {
                    result = true;
                }
            }
        }
    }
    return result;
}

static bool is_alarm_triggered(const ALARM_INFO* alarm_info, const struct tm* curr_time)
{
    bool result = false;
    if (alarm_info->trigger_days != NoDay || alarm_info->trigger_days & get_current_day(curr_time))
    {
        if (alarm_info->trigger_time.hour == curr_time->tm_hour ||
            alarm_info->trigger_time.hour == curr_time->tm_hour - 1)
        {
            if (alarm_info->trigger_time.min == curr_time->tm_min)
            {
                result = true;
            }
        }
    }
    return result;
}

static int store_time_object(ALARM_SCHEDULER* scheduler, const char* alarm_text, const TIME_INFO* time_info, uint32_t trigger_days, const char* sound_file, uint8_t snooze_min)
{
    int result;
    ALARM_STORAGE_ITEM* tm_info;
    // Validate the alarm
    if (time_info->hour >= 24 || time_info->min >= 60)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else if ((tm_info = (ALARM_STORAGE_ITEM*)malloc(sizeof(ALARM_STORAGE_ITEM))) == NULL)
    {
        log_error("Failed to allocate alarm info");
        result = __LINE__;
    }
    else
    {
        tm_info->triggered_date = INVALID_TRIGGERED_DATE;
        tm_info->alarm_info.trigger_days = trigger_days;
        memcpy(&tm_info->alarm_info.trigger_time, time_info, sizeof(TIME_INFO) );
        tm_info->alarm_info.snooze_min = snooze_min;
        if (alarm_text != NULL && clone_string(&tm_info->alarm_info.alarm_text, alarm_text) != 0)
        {
            log_error("Failure copying alarm text");
            free(tm_info);
            result = __LINE__;
        }
        else if (sound_file != NULL && clone_string(&tm_info->alarm_info.sound_file, sound_file) != 0)
        {
            log_error("Failure copying sound file");
            free(tm_info->alarm_info.alarm_text);
            free(tm_info);
            result = __LINE__;
        }
        else if (item_list_add_item(scheduler->sched_list, (const void*)tm_info) != 0)
        {
            log_error("Failure adding items to list");
            free(tm_info->alarm_info.alarm_text);
            free(tm_info->alarm_info.sound_file);
            free(tm_info);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

SCHEDULER_HANDLE alarm_scheduler_create(void)
{
    ALARM_SCHEDULER* result;
    if ((result = (ALARM_SCHEDULER*)malloc(sizeof(ALARM_SCHEDULER))) == NULL)
    {
        log_error("Failure unable to allocate alarm scheduler");
    }
    else
    {
        memset(result, 0, sizeof(ALARM_SCHEDULER));
        if ((result->sched_list = item_list_create(destroy_schedule_list_cb, result)) == NULL)
        {
            log_error("Unable to allocate item list");
            free(result);
            result = NULL;
        }
    }
    return result;
}

void alarm_scheduler_destroy(SCHEDULER_HANDLE handle)
{
    if (handle != NULL)
    {
        item_list_destroy(handle->sched_list);
        free(handle);
    }
}

const ALARM_INFO* alarm_scheduler_is_triggered(SCHEDULER_HANDLE handle, const struct tm* curr_time)
{
    const ALARM_INFO* result = NULL;
    if (handle == NULL || curr_time == NULL)
    {
        log_error("Invalid argument handle: %p, curr_time: %p", handle, curr_time);
    }
    else
    {
        // Loop through the available alarms and see if any are triggered
        size_t alarm_cnt = item_list_item_count(handle->sched_list);
        for (size_t index = 0; index < alarm_cnt; index++)
        {
            ALARM_STORAGE_ITEM* alarm_info = (ALARM_STORAGE_ITEM*)item_list_get_item(handle->sched_list, index);
            if (alarm_info != NULL)
            {
                if (alarm_info->alarm_info.trigger_days != NoDay)
                {
                    if (is_alarm_triggered(&alarm_info->alarm_info, curr_time) &&
                        alarm_info->triggered_date != curr_time->tm_yday)
                    {
                        alarm_info->triggered_date = curr_time->tm_yday;
                        result = &alarm_info->alarm_info;
                        break;
                    }
                }
            }
        }
    }
    return result;
}

int alarm_scheduler_add_alarm_info(SCHEDULER_HANDLE handle, const ALARM_INFO* alarm_info)
{
    int result;
    if (handle == NULL || alarm_info == NULL)
    {
        log_error("Invalid argument value: handle: %p, alarm_info: %p", handle, alarm_info);
        result = __LINE__;
    }
    else if (store_time_object(handle, alarm_info->alarm_text, &alarm_info->trigger_time, alarm_info->trigger_days, alarm_info->sound_file, alarm_info->snooze_min) != 0)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

int alarm_scheduler_add_alarm(SCHEDULER_HANDLE handle, const char* alarm_text, const TIME_INFO* time_info, uint32_t trigger_days, const char* sound_file, uint8_t snooze_min)
{
    int result;
    if (handle == NULL || time_info == NULL)
    {
        log_error("Invalid argument value: handle: %p, time: %p", handle, time_info);
        result = __LINE__;
    }
    else if (store_time_object(handle, alarm_text, time_info, trigger_days, sound_file, snooze_min) != 0)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

int alarm_scheduler_remove_alarm(SCHEDULER_HANDLE handle, size_t alarm_index)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid argument handle:%p", handle);
        result = __LINE__;
    }
    else
    {
        result = 0;
        size_t alarm_cnt = item_list_item_count(handle->sched_list);
        if (alarm_cnt >= alarm_index)
        {
            if (item_list_remove_item(handle->sched_list, alarm_index) != 0)
            {
                log_error("Failure removing item %d", (int)alarm_index);
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
        else
        {
            log_error("remove item index is out of range");
            result = __LINE__;
        }
    }
    return result;
}

const ALARM_INFO* alarm_scheduler_get_next_alarm(SCHEDULER_HANDLE handle)
{
    const ALARM_INFO* result = NULL;
    if (handle == NULL)
    {
        log_error("Invalid argument handle is NULL");
    }
    else
    {
        struct tm* curr_time = get_time_value();

        // Loop through the available alarms and see if any are triggered
        size_t alarm_cnt = item_list_item_count(handle->sched_list);
        for (size_t index = 0; index < alarm_cnt; index++)
        {
            ALARM_STORAGE_ITEM* alarm_info = (ALARM_STORAGE_ITEM*)item_list_get_item(handle->sched_list, index);
            if (alarm_info != NULL)
            {
                if (alarm_info->alarm_info.trigger_days != NoDay)
                {
                    if (result == NULL)
                    {
                        // If we only have 1 alarm then this is the next one
                        result = &alarm_info->alarm_info;
                    }
                    else
                    {
                        if (is_alarm_initial_sooner(result, &alarm_info->alarm_info, curr_time))
                        {
                            result = &alarm_info->alarm_info;
                        }
                    }
                }
            }
        }
    }
    return result;
}

int alarm_scheduler_get_next_day(const ALARM_INFO* alarm_info)
{
    struct tm* curr_time = get_time_value();
    // We are going to trick the algorithm here.  Always make the time
    // after the current
    return get_next_trigger_day(curr_time, alarm_info->trigger_days, &alarm_info->trigger_time);
}

size_t alarm_scheduler_get_alarm_count(SCHEDULER_HANDLE handle)
{
    size_t result;
    if (handle == NULL)
    {
        log_error("Invalid argument handle is NULL");
        result = 0;
    }
    else
    {
        result = item_list_item_count(handle->sched_list);
    }
    return result;
}

const ALARM_INFO* alarm_scheduler_get_alarm(SCHEDULER_HANDLE handle, size_t index)
{
    const ALARM_INFO* result;
    if (handle == NULL)
    {
        log_error("Invalid argument handle is NULL");
        result = NULL;
    }
    else
    {
        const ALARM_STORAGE_ITEM* tm_info = item_list_get_item(handle->sched_list, index);
        if (tm_info == NULL)
        {
            log_error("Failure retrieving item");
            result = NULL;
        }
        else
        {
            result = &tm_info->alarm_info;
        }
    }
    return result;
}

int alarm_scheduler_snooze_alarm(SCHEDULER_HANDLE handle, const ALARM_INFO* alarm_info)
{
    int result;
    if (handle == NULL || alarm_info == NULL)
    {
        log_error("Invalid argument handle is NULL");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

bool alarm_scheduler_is_morning(const TIME_INFO* time_info)
{
    bool result;
    if (time_info == NULL)
    {
        log_error("Invalid argument time_info is NULL");
        result = false;
    }
    else
    {
        result = time_info->hour >= 12 ? false : true;
    }
    return result;
}