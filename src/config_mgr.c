// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "parson.h"

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"
#include "lib-util-c/crt_extensions.h"
#include "config_mgr.h"

static const char* CONFIG_JSON_FILE = "clock_config.json";
static const char* NTP_ADDRESS_NODE = "ntpAddress";
static const char* ZIPCODE_NODE = "zipcode";
static const char* ALARMS_ARRAY_NODE = "alarms";
static const char* AUDIO_DIR_NODE = "audioDirectory";
static const char* DIGIT_COLOR_NODE = "digitColor";
static const char* OPTION_NODE = "option";
static const char* SHADE_START_NODE = "shadeStart";
static const char* SHADE_END_NODE = "shadeEnd";
static const char* DEMO_MODE_NODE = "demo_mode";

static const char* ALARM_NODE_NAME = "name";
static const char* ALARM_NODE_TIME = "time";
static const char* ALARM_NODE_FREQUENCY = "frequency";
static const char* ALARM_NODE_SOUND = "sound";
static const char* ALARM_NODE_SNOOZE = "snooze";
static const char* ALARM_NODE_ID = "id";

#define CLOCK_HOUR_24           0x00000001
#define CLOCK_SHOW_SECONDS      0x00000002
#define USE_CELSIUS             0x00000004
#define DEFAULT_DIGIT_COLOR     0
#define INVALID_DIGIT_COLOR     0xFFFFFFFF

typedef struct CONFIG_MGR_INFO_TAG
{
    char* config_file;
    uint32_t option;
    uint32_t digit_color;
    JSON_Value* json_root;
    JSON_Object* json_object;
    const char* ntp_address;
    const char* zipcode;
    const char* audio_directory;
} CONFIG_MGR_INFO;

static int validate_time(const TIME_VALUE_STORAGE* time_value)
{
    int result;
    if (time_value->hours > 23 || time_value->minutes > 59 || time_value->seconds > 59)
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

static int parse_time_value(const char* time, TIME_VALUE_STORAGE* time_value)
{
    int result = 0;
    const char* iterator = time;
    uint8_t accu_val = 0;
    size_t array_idx = 0;
    do
    {
        if ((*iterator < 0x30 || *iterator > 0x39) && *iterator != 0x3A)
        {
            log_error("Invalid time value specified");
            result = __LINE__;
            break;
        }
        else
        {
            if (*iterator == ':')
            {
                if (array_idx == 0)
                {
                    time_value->hours = accu_val;
                }
                else if (array_idx == 1)
                {
                    time_value->minutes = accu_val;
                }
                else if (array_idx == 2)
                {
                    time_value->seconds = accu_val;
                }
                array_idx++;
                accu_val = 0;
            }
            else
            {
                accu_val = accu_val*10 + (*iterator - 0x30);
            }
            iterator++;
        }
    } while (*iterator != '\0');
    if (result == 0)
    {
        time_value->seconds = accu_val;
        // Validate if any value is out of scope
        if (time_value->hours > 23 || time_value->minutes > 59 || time_value->seconds > 59)
        {
            log_error("Invalid time value out of bounds");
            result = __LINE__;
        }
    }
    return result;
}

CONFIG_MGR_HANDLE config_mgr_create(const char* config_path)
{
    CONFIG_MGR_INFO* result;
    if ((result = (CONFIG_MGR_INFO*)malloc(sizeof(CONFIG_MGR_INFO))) == NULL)
    {
        log_error("Failure allocating config manager");
    }
    else
    {
        memset(result, 0, sizeof(CONFIG_MGR_INFO));
         if (clone_string_with_format(&result->config_file, "%s%s", config_path, CONFIG_JSON_FILE) != 0)
        {
            log_error("Failure allocating config manager");
            free(result);
            result = NULL;
        }
        else if ((result->json_root = json_parse_file(result->config_file)) == NULL)
        {
            log_error("Failure allocating config manager");
            free(result->config_file);
            free(result);
            result = NULL;
        }
        else if ((result->json_object = json_value_get_object(result->json_root)) == NULL)
        {
            log_error("Failure allocating config manager");
            json_value_free(result->json_root);
            free(result->config_file);
            free(result);
            result = NULL;
        }
        else
        {
            if ((result->option = (uint32_t)json_object_get_number(result->json_object, OPTION_NODE)) == ((uint32_t)JSONError))
            {
                log_warning("Failure retrieving option node");
                result->option = 0;
            }
            // TODO: Digital Color defaults
            result->digit_color = INVALID_DIGIT_COLOR;
        }
    }
    return result;
}

void config_mgr_destroy(CONFIG_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        json_value_free(handle->json_root);
        free(handle->config_file);
        free(handle);
    }
}

bool config_mgr_save(CONFIG_MGR_HANDLE handle)
{
    bool result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = false;
    }
    else
    {
        if (json_serialize_to_file_pretty(handle->json_root, handle->config_file) != JSONSuccess)
        {
            log_error("Failure serializing json");
            result = false;
        }
        else
        {
            log_debug("Configuration file saved");
            result = true;
        }
    }
    return result;
}

int config_mgr_set_zipcode(CONFIG_MGR_HANDLE handle, const char* zipecode)
{
    int result;
    if (handle == NULL || zipecode == NULL || zipecode[0] == '\0')
    {
        log_error("Invalid parameter handle: %p zipcode: %p", handle, zipecode);
        result = __LINE__;
    }
    else
    {
        if (json_object_set_string(handle->json_object, ZIPCODE_NODE, zipecode) != JSONSuccess)
        {
            log_error("Failed setting zipcode value");
            result = __LINE__;
        }
        else if ((handle->zipcode = json_object_get_string(handle->json_object, ZIPCODE_NODE)) == NULL)
        {
            log_error("Failure getting zipcode json object");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

const char* config_mgr_get_zipcode(CONFIG_MGR_HANDLE handle)
{
    const char* result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = NULL;
    }
    else if (handle->zipcode == NULL)
    {
        // Read Zip code Option
        if ((handle->zipcode = json_object_get_string(handle->json_object, ZIPCODE_NODE)) == NULL)
        {
            log_error("Failure getting zipcode json object");
            result = NULL;
        }
        else
        {
            result = handle->zipcode;
        }
    }
    else
    {
        result = handle->zipcode;
    }
    return result;
}

const char* config_mgr_get_ntp_address(CONFIG_MGR_HANDLE handle)
{
    const char* result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = NULL;
    }
    else
    {
        if (handle->ntp_address == NULL)
        {
            // Read Ntp Option
            if ((handle->ntp_address = json_object_get_string(handle->json_object, NTP_ADDRESS_NODE)) == NULL)
            {
                log_error("Failure getting json object");
                result = NULL;
            }
            else
            {
                result = handle->ntp_address;
            }
        }
        else
        {
            result = handle->ntp_address;
        }
    }
    return result;
}

const char* config_mgr_get_audio_dir(CONFIG_MGR_HANDLE handle)
{
    const char* result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = NULL;
    }
    else
    {
        if (handle->audio_directory == NULL)
        {
            if ((handle->audio_directory = json_object_get_string(handle->json_object, AUDIO_DIR_NODE)) == NULL)
            {
                log_error("Failure getting json object");
                result = NULL;
            }
            else
            {
                result = handle->audio_directory;
            }
        }
        else
        {
            result = handle->audio_directory;
        }
    }
    return result;
}

uint32_t config_mgr_get_digit_color(CONFIG_MGR_HANDLE handle)
{
    uint32_t result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = DEFAULT_DIGIT_COLOR;
    }
    else
    {
        if (handle->digit_color == INVALID_DIGIT_COLOR)
        {
            if ((handle->digit_color = (uint32_t)json_object_get_number(handle->json_object, DIGIT_COLOR_NODE)) == (uint32_t)JSONError)
            {
                log_error("Failure getting json object");
                result = DEFAULT_DIGIT_COLOR;
            }
            else
            {
                result = handle->digit_color;
            }
        }
        else
        {
            result = handle->digit_color;
        }
    }
    return result;
}

int config_mgr_set_digit_color(CONFIG_MGR_HANDLE handle, uint32_t digit_color)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = __LINE__;
    }
    else
    {
        if (json_object_set_number(handle->json_object, DIGIT_COLOR_NODE, digit_color) != JSONSuccess)
        {
            log_error("Failure setting digit color value");
            result = __LINE__;
        }
        else
        {
            handle->digit_color = digit_color;
            result = 0;
        }
    }
    return result;
}

int config_mgr_load_alarm(CONFIG_MGR_HANDLE handle, ON_ALARM_LOAD_CALLBACK alarm_cb, void* user_ctx)
{
    int result;
    if (handle == NULL || alarm_cb == NULL)
    {
        log_error("Invalid parameter specified handle: %p alarm_cb: %p", handle, alarm_cb);
        result = __LINE__;
    }
    else
    {
        JSON_Array* alarms;
        if ((alarms = json_object_get_array(handle->json_object, ALARMS_ARRAY_NODE)) == NULL)
        {
            log_error("Failure getting array object");
            result = __LINE__;
        }
        else
        {
            // Loop through the array
            size_t alarm_count = json_array_get_count(alarms);
            for (size_t index = 0; index < alarm_count; index++)
            {
                CONFIG_ALARM_INFO config_alarm = {0};
                JSON_Object* alarm_item;
                if ((alarm_item = json_array_get_object(alarms, index)) == NULL)
                {
                    log_error("Failure getting config alarm object");
                    result = __LINE__;
                    break;
                }
                else
                {
                    const char* time;
                    if (((config_alarm.name = json_object_get_string(alarm_item, "name")) == NULL) ||
                        ((config_alarm.sound_file = json_object_get_string(alarm_item, "sound")) == NULL) ||
                        ((time = json_object_get_string(alarm_item, "time")) == NULL) ||
                        ((parse_time_value(time, &config_alarm.time_value)) != 0))
                    {
                        log_error("Failure parsing the time object");
                        result = __LINE__;
                        break;
                    }
                    else
                    {
                        config_alarm.id = (uint8_t)json_object_get_number(alarm_item, "id");
                        config_alarm.snooze = (uint8_t)json_object_get_number(alarm_item, "snooze");
                        config_alarm.frequency = (uint32_t)json_object_get_number(alarm_item, "frequency");
                        if (alarm_cb(user_ctx, &config_alarm) != 0)
                        {
                            break;
                        }
                        result = 0;
                    }
                }
            }
        }
    }
    return result;
}

int config_mgr_store_alarm(CONFIG_MGR_HANDLE handle, const char* name, const TIME_VALUE_STORAGE* time_value, const char* sound_file, uint32_t frequency, uint8_t snooze, uint8_t id)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid handle value");
        result = __LINE__;
    }
    // validate time
    else if (validate_time(time_value) != 0)
    {
        log_error("Invalid time value specified");
        result = __LINE__;
    }
    else
    {
        JSON_Array* alarms;
        JSON_Object* alarm_object;
        JSON_Value* alarm_node;
        if ((alarms = json_object_get_array(handle->json_object, ALARMS_ARRAY_NODE)) == NULL)
        {
            log_error("Failure getting alarm arrays");
            result = __LINE__;
        }
        else if ((alarm_node = json_value_init_object()) == NULL)
        {
            log_error("Failure initializing alarm arrays");
            result = __LINE__;
        }
        else if ((alarm_object = json_value_get_object(alarm_node)) == NULL)
        {
            log_error("Failure getting alarm object");
            result = __LINE__;
        }
        else
        {
            char time_string[16];
            sprintf(time_string, "%d:%02d:%02d", time_value->hours, time_value->minutes, time_value->seconds);
            if (json_object_set_string(alarm_object, ALARM_NODE_NAME, name == NULL ? "" : name) != JSONSuccess)
            {
                log_error("Failure constructing alarm name");
                result = __LINE__;
            }
            else if (json_object_set_string(alarm_object, ALARM_NODE_TIME, time_string) != JSONSuccess)
            {
                log_error("Failure constructing alarm time");
                result = __LINE__;
            }
            else if (json_object_set_number(alarm_object, ALARM_NODE_FREQUENCY, frequency) != JSONSuccess)
            {
                log_error("Failure constructing alarm frequency");
                result = __LINE__;
            }
            else if (json_object_set_string(alarm_object, ALARM_NODE_SOUND, sound_file ? "" : sound_file) != JSONSuccess)
            {
                log_error("Failure constructing alarm time");
                result = __LINE__;
            }
            else if (json_object_set_number(alarm_object, ALARM_NODE_SNOOZE, snooze) != JSONSuccess)
            {
                log_error("Failure constructing alarm snooze");
                result = __LINE__;
            }
            else if (json_object_set_number(alarm_object, ALARM_NODE_ID, id) != JSONSuccess)
            {
                log_error("Failure constructing alarm snooze");
                result = __LINE__;
            }
            else if (json_array_append_value(alarms, alarm_node))
            {
                log_error("Failure appending json array");
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

uint8_t config_mgr_format_hour(CONFIG_MGR_HANDLE handle, int hour)
{
    uint8_t result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = 0;
    }
    else
    {
        if (handle->option & CLOCK_HOUR_24)
        {
            result = hour;
        }
        else
        {
            result = hour > 12 ? hour - 12 : hour;
        }
    }
    return result;
}

bool config_mgr_show_seconds(CONFIG_MGR_HANDLE handle)
{
    bool result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = false;
    }
    else
    {
        result = (handle->option & CLOCK_SHOW_SECONDS);
    }
    return result;
}

int config_mgr_set_24h_clock(CONFIG_MGR_HANDLE handle, bool is_24h_clock)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = __LINE__;
    }
    else
    {
        if (is_24h_clock)
        {
            handle->option |= CLOCK_HOUR_24;
        }
        else
        {
            handle->option &= ~CLOCK_HOUR_24;
        }
        result = 0;
    }
    return result;
}

bool config_mgr_is_24h_clock(CONFIG_MGR_HANDLE handle)
{
    bool result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = false;
    }
    else
    {
        result = (handle->option & CLOCK_HOUR_24);
    }
    return result;
}

int config_mgr_set_celsius(CONFIG_MGR_HANDLE handle, bool is_celsius)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = __LINE__;
    }
    else
    {
        if (is_celsius)
        {
            handle->option |= USE_CELSIUS;
        }
        else
        {
            handle->option &= ~USE_CELSIUS;
        }
        result = 0;
    }
    return result;
}

bool config_mgr_is_celsius(CONFIG_MGR_HANDLE handle)
{
    bool result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = false;
    }
    else
    {
        result = (handle->option & USE_CELSIUS);
    }
    return result;
}

int config_mgr_get_shade_times(CONFIG_MGR_HANDLE handle, TIME_VALUE_STORAGE* start_time, TIME_VALUE_STORAGE* end_time)
{
    int result;
    if (handle == NULL || start_time == NULL || end_time == NULL)
    {
        log_error("Invalid parameter specified handle: %p, start_time: %p end_time: %p", handle, start_time, end_time);
        result = __LINE__;
    }
    else
    {
        const char* shade_start;
        const char* shade_end;
        if ((shade_start = json_object_get_string(handle->json_object, SHADE_START_NODE)) == NULL ||
            parse_time_value(shade_start, start_time) != 0)
        {
            log_error("Failure retrieving shade start time");
            result = __LINE__;
        }
        else if ((shade_end = json_object_get_string(handle->json_object, SHADE_END_NODE)) == NULL ||
            parse_time_value(shade_end, end_time) != 0)
        {
            log_error("Failure retrieving shade end time");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

int config_mgr_set_shade_times(CONFIG_MGR_HANDLE handle, const TIME_VALUE_STORAGE* start_time, const TIME_VALUE_STORAGE* end_time)
{
    int result;
    if (handle == NULL || start_time == NULL || end_time == NULL)
    {
        log_error("Invalid parameter specified handle: %p, start_time: %p end_time: %p", handle, start_time, end_time);
        result = __LINE__;
    }
    else if (validate_time(start_time) != 0)
    {
        log_error("Invalid start time specified");
        result = __LINE__;
    }
    else if (validate_time(end_time) != 0)
    {
        log_error("Invalid end time specified");
        result = __LINE__;
    }
    else
    {
        char time_string[16];
        sprintf(time_string, "%d:%02d:%02d", start_time->hours, start_time->minutes, start_time->seconds);
        if (json_object_set_string(handle->json_object, SHADE_START_NODE, time_string) != JSONSuccess)
        {
            log_error("Failed setting shade start value");
            result = __LINE__;
        }
        else
        {
            sprintf(time_string, "%d:%02d:%02d", end_time->hours, end_time->minutes, end_time->seconds);
            if (json_object_set_string(handle->json_object, SHADE_END_NODE, time_string) != JSONSuccess)
            {
                log_error("Failed setting shade end value");
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

bool config_mgr_is_demo_mode(CONFIG_MGR_HANDLE handle)
{
    bool result;
    if (handle == NULL)
    {
        log_error("Invalid handle specified");
        result = true;
    }
    else
    {
        int boolean_res = json_object_get_boolean(handle->json_object, DEMO_MODE_NODE);
        if (boolean_res == -1)
        {
            result = true;
        }
        else
        {
            result = (bool)boolean_res;
        }
    }
    return result;
}