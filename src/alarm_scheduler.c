
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

static uint8_t get_current_day(const struct tm* curr_time)
{
    uint8_t result;
    switch (curr_time->tm_wday)
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
            result = NoDay;
            break;
    }
    return result;
}

static bool is_alarm_triggered(const ALARM_INFO* alarm_info, const struct tm* curr_time)
{
    bool result = false;
    if (alarm_info->trigger_days & get_current_day(curr_time))
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

static int store_time_object(ALARM_SCHEDULER* scheduler, const ALARM_STORAGE_ITEM* tm_info)
{
    int result;
    if (tm_info->alarm_info.trigger_time.hour >= 24 || tm_info->alarm_info.trigger_time.min >= 60)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else if (item_list_add_item(scheduler->sched_list, (const void*)tm_info) != 0)
    {
        log_error("Failure adding items to list");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

static int play_alarm()
{

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

const ALARM_INFO* alarm_scheduler_is_triggered(SCHEDULER_HANDLE handle)
{
    const ALARM_INFO* result = NULL;
    if (handle == NULL)
    {
        log_error("Invalid argument handle is NULL");
    }
    else
    {
        time_t mark_time = time(NULL);
        struct tm* curr_time = gmtime(&mark_time);

        // Loop through the available alarms and see if any are triggered
        size_t alarm_cnt = item_list_item_count(handle->sched_list);
        for (size_t index = 0; index < alarm_cnt; index++)
        {
            ALARM_STORAGE_ITEM* alarm_info = (ALARM_STORAGE_ITEM*)item_list_get_item(handle->sched_list, index);
            if (alarm_info != NULL)
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
    return result;
}

int alarm_scheduler_add_alarm_info(SCHEDULER_HANDLE handle, const ALARM_INFO* alarm_info)
{
    int result;
    ALARM_STORAGE_ITEM* tm_info;
    if (handle == NULL || alarm_info == NULL)
    {
        log_error("Invalid argument value: handle: %p, alarm_info: %p", handle, alarm_info);
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
        tm_info->alarm_info.trigger_days = alarm_info->trigger_days;
        tm_info->alarm_info.trigger_time.hour = alarm_info->trigger_time.hour;
        tm_info->alarm_info.trigger_time.min = alarm_info->trigger_time.min;
        if (alarm_info->alarm_text != NULL && clone_string(&tm_info->alarm_info.alarm_text, alarm_info->alarm_text) != 0)
        {
            log_error("Failure copying alarm text");
            free(tm_info);
            result = __LINE__;
        }
        else if (alarm_info->sound_file != NULL && clone_string(&tm_info->alarm_info.sound_file, alarm_info->sound_file) != 0)
        {
            log_error("Failure copying sound file");
            free(tm_info->alarm_info.alarm_text);
            free(tm_info);
            result = __LINE__;
        }
        else if (store_time_object(handle, tm_info) != 0)
        {
            log_error("Invalid time value specified");
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

int alarm_scheduler_add_alarm(SCHEDULER_HANDLE handle, const char* alarm_text, const TIME_INFO* time_info, uint32_t trigger_days, const char* sound_file)
{
    int result;
    ALARM_STORAGE_ITEM* tm_info;
    if (handle == NULL || time_info == NULL)
    {
        log_error("Invalid argument value: handle: %p, time: %p", handle, time_info);
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
        tm_info->alarm_info.trigger_time.hour = time_info->hour;
        tm_info->alarm_info.trigger_time.min = time_info->min;

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
        else if (store_time_object(handle, tm_info) != 0)
        {
            log_error("Invalid time value specified");
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
