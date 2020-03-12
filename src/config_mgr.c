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

typedef struct CONFIG_MGR_INFO_TAG
{
    char* config_file;
    JSON_Value* json_root;
    JSON_Object* json_object;
    const char* ntp_address;
    const char* zipcode;
} CONFIG_MGR_INFO;

static int parse_time_value(const char* time, uint8_t time_array[3])
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
                time_array[array_idx++] = accu_val;
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
        time_array[array_idx++] = accu_val;
        // Validate if any value is out of scope
        if (time_array[0] > 23 || time_array[1] > 59 || time_array[2] > 59)
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
            result = true;
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
        // Read Zipcode Option
        if ((handle->ntp_address = json_object_get_string(handle->json_object, ZIPCODE_NODE)) == NULL)
        {
            log_error("Failure getting zipcode json object");
            result = NULL;
        }
        else
        {
            result = handle->ntp_address;
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
                        ((parse_time_value(time, config_alarm.time_array)) != 0))
                    {
                        log_error("Failure parsing the time object");
                        result = __LINE__;
                        break;
                    }
                    else
                    {
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
