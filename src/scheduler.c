
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "lib-util-c/app_logging.h"
#include "lib-util-c/item_list.h"
#include "scheduler.h"

typedef struct ALARM_SCHEDULER_TAG
{
    ITEM_LIST_HANDLE sched_list;
} ALARM_SCHEDULER;

static void destroy_schedule_list_cb(void* user_ctx, void* item)
{
    ALARM_SCHEDULER* alarm_sched = (ALARM_SCHEDULER*)user_ctx;
    if (alarm_sched == NULL)
    {
        ALARM_INFO* alarm_info = (ALARM_INFO*)item;
        free(alarm_info);
    }
}

static char* copy_string(const char* text)
{
    char* result = NULL;
    if (text != NULL)
    {
        size_t len = strlen(text);
        if (len > 0)
        {
            result = (char*)malloc(len);
            if (result == NULL)
            {
                log_error("Failed to allocate value");
            }
            else
            {
                strcpy(result, text);
            }
        }
    }
    return result;
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
        if (alarm_info->trigger_time.hour == curr_time->tm_hour &&
            alarm_info->trigger_time.min == curr_time->tm_min)
        {
            result = true;
        }
    }
    return result;
}

static int store_time_object(ALARM_SCHEDULER* scheduler, const ALARM_INFO* tm_info, bool make_copy)
{
    int result;
    if (tm_info->trigger_time.hour >= 24 || tm_info->trigger_time.min >= 60)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else
    {
        size_t additional_len = 0;
        if (tm_info->alarm_text != NULL)
        {
            additional_len = strlen(tm_info->alarm_text);
        }
        if (tm_info->sound_file != NULL)
        {
            additional_len += strlen(tm_info->sound_file);
        }
        if (make_copy)
        {
            result = item_list_add_copy(scheduler->sched_list, tm_info, sizeof(ALARM_INFO) + additional_len);
        }
        else
        {
            result = item_list_add_item(scheduler->sched_list, (void*)tm_info);
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
        else
        {

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
    //bool result;
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
            const ALARM_INFO* alarm_info = item_list_get_item(handle->sched_list, index);
            if (is_alarm_triggered(alarm_info, curr_time) )
            {
                result = alarm_info;
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
    // validate the alarm info
    else if (store_time_object(handle, alarm_info, true) != 0)
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

int alarm_scheduler_add_alarm(SCHEDULER_HANDLE handle, const char* alarm_text, const TIME_INFO* time_info, uint32_t trigger_days, const char* sound_file)
{
    int result;
    ALARM_INFO* tm_info;
    if (handle == NULL || time_info == NULL)
    {
        log_error("Invalid argument value: handle: %p, time: %p", handle, time_info);
        result = __LINE__;
    }
    else if ((tm_info = (ALARM_INFO*)malloc(sizeof(ALARM_INFO))) == NULL)
    {
        log_error("Failed to allocate alarm info");
        result = __LINE__;
    }
    else
    {
        tm_info->trigger_days = trigger_days;
        tm_info->trigger_time.hour = time_info->hour;
        tm_info->trigger_time.min = time_info->min;

        if (alarm_text != NULL && (tm_info->alarm_text = copy_string(alarm_text)) == NULL)
        {
            log_error("Failure copying alarm text");
            result = __LINE__;
        }
        else if (sound_file != NULL && (tm_info->sound_file = copy_string(sound_file)) == NULL)
        {
            log_error("Failure copying sound file");
            result = __LINE__;
        }
        else if (store_time_object(handle, tm_info, false) != 0)
        {
            log_error("Invalid time value specified");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}
