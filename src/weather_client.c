// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib-util-c/app_logging.h"
#include "lib-util-c/alarm_timer.h"

#include "weather_client.h"

#include "azure_uhttp_c/uhttp.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/socketio.h"
#include "parson.h"

/* http://openweathermap.org/ */
static const char* WEATHER_API_HOSTNAME = "api.openweathermap.org";
static const char* API_COORD_PATH_FMT = "/data/2.5/weather?lat=%f&lon=%f&%s&appid=%s";
static const char* API_NAME_PATH_FMT = "/data/2.5/weather?q=%s&%s&appid=%s";
static const char* API_ZIPCODE_FMT = "/data/2.5/weather?id=%d&%s&appid=%s";

static const char* TEMP_UNIT_FAHRENHEIT_VALUE = "units=imperial";
static const char* TEMP_UNIT_CELSIUS_VALUE = "units=metric";
static const char* TEMP_UNIT_KELVIN_VALUE = "";


#define HTTP_PORT_VALUE     80
#define MAX_CLOSE_ATTEMPTS  10

typedef enum WEATHER_CLIENT_STATE_TAG
{
    WEATHER_CLIENT_STATE_IDLE,
    WEATHER_CLIENT_STATE_CONNECTING,
    WEATHER_CLIENT_STATE_CONNECTED,
    WEATHER_CLIENT_STATE_SEND,
    WEATHER_CLIENT_STATE_SENT,
    WEATHER_CLIENT_STATE_RECV,
    WEATHER_CLIENT_STATE_ERROR
} WEATHER_CLIENT_STATE;

typedef enum WEATHER_QUERY_TYPE_TAG
{
    QUERY_TYPE_ZIP_CODE,
    QUERY_TYPE_COORDINATES,
    QUERY_TYPE_NAME
} WEATHER_QUERY_TYPE;

typedef struct WEATHER_CLIENT_INFO_TAG
{
    HTTP_CLIENT_HANDLE http_handle;
    ALARM_TIMER_HANDLE timer_handle;
    char* api_key;
    size_t timeout_sec;

    bool is_open;

    WEATHER_CONDITIONS_CALLBACK conditions_callback;
    void* condition_ctx;

    const char* temp_units;
    WEATHER_CLIENT_STATE state;
    WEATHER_QUERY_TYPE query_type;
    union
    {
        /* data */
        WEATHER_LOCATION coord_info;
        uint8_t zip_code;
        char* name;
    } weather_query;
    char* weather_data;
} WEATHER_CLIENT_INFO;

static bool is_timed_out(WEATHER_CLIENT_INFO* client_info)
{
    bool result = false;
    if (client_info->timeout_sec > 0)
    {
        result = alarm_timer_is_expired(client_info->timer_handle);
    }
    return result;
}

static void set_temp_units(WEATHER_CLIENT_INFO* client_info, TEMPERATURE_UNITS units)
{
    switch (units)
    {
        case UNIT_KELVIN:
            client_info->temp_units = TEMP_UNIT_KELVIN_VALUE;
            break;
        case UNIT_CELSIUS:
            client_info->temp_units = TEMP_UNIT_CELSIUS_VALUE;
            break;
        case UNIT_FAHRENHEIGHT:
        default:
            client_info->temp_units = TEMP_UNIT_FAHRENHEIT_VALUE;
            break;
    }
}

static int get_weather_description(JSON_Object* root_obj, WEATHER_CONDITIONS* conditions)
{
    int result;
    // Get the Description
    JSON_Array* weather_array = json_object_get_array(root_obj, "weather");
    if (weather_array == NULL)
    {
        log_error("Weather array not present");
        result = __LINE__;
    }
    else
    {
        size_t count = json_array_get_count(weather_array);
        if (count > 0)
        {
            JSON_Object* weather_obj;
            JSON_Value* weather_node = json_array_get_value(weather_array, 0);
            if (weather_node == NULL)
            {
                log_error("Failure getting initial array node");
                result = __LINE__;
            }
            else if ((weather_obj = json_value_get_object(weather_node)) != NULL)
            {
                const char* desc_value = json_object_get_string(weather_obj, "description");
                if (desc_value != NULL)
                {
                    size_t desc_len = strlen(desc_value);
                    conditions->description = (const char*)malloc(desc_len+1);
                    if (conditions->description != NULL)
                    {
                        strcpy((char*)conditions->description, desc_value);
                        const char* icon_value = json_object_get_string(weather_obj, "icon");
                        if (icon_value != NULL && strlen(icon_value) < ICON_MAX_LENGTH)
                        {
                            strcpy((char*)conditions->weather_icon, icon_value);
                        }
                        result = 0;
                    }
                    else
                    {
                        log_error("Failure allocating description");
                        result = __LINE__;
                    }
                }
                else
                {
                    log_error("Failure getting description node value");
                    result = __LINE__;
                }
            }
        }
        else
        {
            log_error("Empty array encountered");
            result = __LINE__;
        }
    }
    return result;
}

static int get_main_weather_info(JSON_Object* root_obj, WEATHER_CONDITIONS* conditions)
{
    int result;
    JSON_Object* main_json = json_object_get_object(root_obj, "main");
    if (main_json == NULL)
    {
        log_error("Failure retreiving the main weather node");
        result = __LINE__;
    }
    else
    {
        conditions->temperature = json_object_get_number(main_json, "temp");
        conditions->pressure = (uint32_t)json_object_get_number(main_json, "pressure");
        conditions->humidity = (uint8_t)json_object_get_number(main_json, "humidity");
        conditions->lo_temp = json_object_get_number(main_json, "temp_min");
        conditions->hi_temp = json_object_get_number(main_json, "temp_max");
        result = 0;
    }
    return result;
}

static int parse_weather_data(WEATHER_CLIENT_INFO* client_info, WEATHER_CONDITIONS* conditions)
{
    int result;
    JSON_Value* json_root = json_parse_string(client_info->weather_data);
    if (json_root == NULL)
    {
        log_error("Failure parsing invalid json presented");
        result = __LINE__;
    }
    else
    {
        JSON_Object* root_obj = json_value_get_object(json_root);
        if (root_obj == NULL)
        {
            log_error("Failure getting root object");
            result = __LINE__;
        }
        else if (get_weather_description(root_obj, conditions) != 0)
        {
            log_error("Failure parsing weather description");
            result = __LINE__;
        }
        else if (get_main_weather_info(root_obj, conditions))
        {
            log_error("Failure parsing main weather info");
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
        json_value_free(json_root);
    }
    return result;
}

static void on_http_error(void* user_ctx, HTTP_CALLBACK_REASON error_result)
{
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else
    {

    }
}

static void on_http_connected(void* user_ctx, HTTP_CALLBACK_REASON open_result)
{
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else
    {
        if (open_result == HTTP_CALLBACK_REASON_OK)
        {
            client_info->is_open = true;
        }
        else
        {
            // Set Error
            log_error("Failure connecting to server %d", (int)open_result);
            client_info->state = WEATHER_CLIENT_STATE_ERROR;
        }
    }
}

static void on_http_reply_recv(void* user_ctx, HTTP_CALLBACK_REASON request_result, const unsigned char* content, size_t content_len, unsigned int status_code, HTTP_HEADERS_HANDLE responseHeadersHandle)
{
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else if (request_result == HTTP_CALLBACK_REASON_OK)
    {
        log_error("Invalid recv result returned %d", (int)request_result);
        client_info->state = WEATHER_CLIENT_STATE_ERROR;
    }
    else
    {
        if (client_info->state == WEATHER_CLIENT_STATE_SENT)
        {
            // Check status_code
            // Parse reply
            if (client_info->weather_data == NULL)
            {
                if ((client_info->weather_data = malloc(content_len+1)) == NULL)
                {
                    log_error("Failure allocating weather content of length %zu", content_len);
                    client_info->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    memset(client_info->weather_data, 0, content_len+1);
                    memcpy(client_info->weather_data, content, content_len);
                    client_info->state = WEATHER_CLIENT_STATE_RECV;
                }
            }
        }
    }
}

static void on_http_close(void* user_ctx)
{
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else
    {
        client_info->is_open = false;
    }
}

static int send_weather_data(WEATHER_CLIENT_INFO* client_info)
{
    int result;
    char weather_api_path[128];
    switch (client_info->query_type)
    {
        case QUERY_TYPE_ZIP_CODE:
            sprintf(weather_api_path, API_ZIPCODE_FMT, client_info->weather_query.zip_code, client_info->temp_units, client_info->api_key);
            break;
        case QUERY_TYPE_COORDINATES:
            sprintf(weather_api_path, API_COORD_PATH_FMT, client_info->weather_query.coord_info.latitude, client_info->weather_query.coord_info.longitude, client_info->temp_units, client_info->api_key);
            break;
        case QUERY_TYPE_NAME:
            sprintf(weather_api_path, API_NAME_PATH_FMT, client_info->weather_query.name, client_info->temp_units, client_info->api_key);
            break;
    }

    if (uhttp_client_execute_request(client_info->http_handle, HTTP_CLIENT_REQUEST_GET, weather_api_path, NULL, NULL, 0, on_http_reply_recv, client_info) != HTTP_CLIENT_OK)
    {
        log_error("Failure executing http request");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

static void close_http_connection(WEATHER_CLIENT_INFO* client_info)
{
    uhttp_client_close(client_info->http_handle, on_http_close, client_info);
    size_t attempt = 0;
    do
    {
        uhttp_client_dowork(client_info->http_handle);
        // Make sure we don't just spin here forever
        attempt++;
    } while (client_info->is_open && attempt < MAX_CLOSE_ATTEMPTS);

    uhttp_client_destroy(client_info->http_handle);
    client_info->http_handle = NULL;
}

static int open_connection(WEATHER_CLIENT_INFO* client_info)
{
    int result;
    SOCKETIO_CONFIG config;
    config.accepted_socket = NULL;
    config.hostname = WEATHER_API_HOSTNAME;
    config.port = HTTP_PORT_VALUE;

    if ((client_info->http_handle = uhttp_client_create(socketio_get_interface_description(), &config, on_http_error, client_info)) == NULL)
    {
        log_error("Failure creating http client");
        result = __LINE__;
    }
    else
    {
        (void)uhttp_client_set_trace(client_info->http_handle, true, true);
        if (uhttp_client_open(client_info->http_handle, config.hostname, config.port, on_http_connected, client_info) != HTTP_CLIENT_OK)
        {
            log_error("Failure opening http connection: %s:%d", config.hostname, config.port);
            result = __LINE__;
        }
        else if (alarm_timer_start(client_info->timer_handle, client_info->timeout_sec) != 0)
        {
            log_error("Failure setting the timeout value");
            close_http_connection(client_info);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return 0;
}

WEATHER_CLIENT_HANDLE weather_client_create(const char* api_key, TEMPERATURE_UNITS units)
{
    WEATHER_CLIENT_INFO* result;
    if (api_key == NULL)
    {
        log_error("Invalid api_key value");
        result = NULL;
    }
    else if ((result = (WEATHER_CLIENT_INFO*)malloc(sizeof(WEATHER_CLIENT_INFO))) == NULL)
    {
        log_error("Failure allocating Weather Client");
    }
    else
    {
        memset(result, 0, sizeof(WEATHER_CLIENT_INFO));

        size_t key_len = strlen(api_key);
        if ((result->api_key = (char*)malloc(key_len+1)) == NULL)
        {
            log_error("Failure allocating api key");
            free(result);
            result = NULL;
        }
        else if ((result->timer_handle = alarm_timer_create()) == NULL)
        {
            log_error("Failure creating timer object");
            free(result->api_key);
            free(result);
            result = NULL;
        }
        else
        {
            strcpy(result->api_key, api_key);
            result->state = WEATHER_CLIENT_STATE_IDLE;
            set_temp_units(result, units);
        }
    }
    return result;
}

void weather_client_destroy(WEATHER_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        close_http_connection(handle);

        alarm_timer_destroy(handle->timer_handle);
        free(handle->api_key);
        free(handle);
    }
}

int weather_client_get_by_coordinate(WEATHER_CLIENT_HANDLE handle, const WEATHER_LOCATION* location, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || location == NULL || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, location: %p, conditions_callback: %p", handle, location, conditions_callback);
        result = __LINE__;
    }
    else if (handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
    {
        log_error("Invalid State specified, operation must be complete to add another call");
        result = __LINE__;
    }
    else
    {
        handle->timeout_sec = timeout;
        if (!handle->is_open && open_connection(handle) != 0)
        {
            log_error("Failure opening connection");
            result = __LINE__;
        }
        else
        {
            // Store data
            handle->query_type = QUERY_TYPE_COORDINATES;
            handle->weather_query.coord_info.latitude = location->latitude;
            handle->weather_query.coord_info.longitude = location->longitude;
            handle->conditions_callback = conditions_callback;
            handle->condition_ctx = user_ctx;
            result = 0;
        }
    }
    return result;
}

int weather_client_get_by_zipcode(WEATHER_CLIENT_HANDLE handle, const uint8_t zipcode, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || zipcode == 0 || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, zipcode: %d, conditions_callback: %p", handle, zipcode, conditions_callback);
        result = __LINE__;
    }
    else if (handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
    {
        log_error("Invalid State specified, operation must be complete to add another call");
        result = __LINE__;
    }
    else
    {
        handle->timeout_sec = timeout;
        if (!handle->is_open && open_connection(handle) != 0)
        {
            log_error("Failure opening connection");
            result = __LINE__;
        }
        else
        {
            // Store data
            handle->query_type = QUERY_TYPE_ZIP_CODE;
            handle->weather_query.zip_code = zipcode;
            result = 0;
        }
    }
    return result;
}

int weather_client_get_by_city(WEATHER_CLIENT_HANDLE handle, const char* city_name, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || city_name == NULL || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, city_name: %p, conditions_callback: %p", handle, city_name, conditions_callback);
        result = __LINE__;
    }
    else if (handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
    {
        log_error("Invalid State specified, operation must be complete to add another call");
        result = __LINE__;
    }
    else
    {
        size_t length = strlen(city_name);
        if ((handle->weather_query.name = (char*)malloc(length+1)) == NULL)
        {
            log_error("Failure allocating city name");
            result = __LINE__;
        }
        else if (strcpy(handle->weather_query.name, city_name) == NULL)
        {
            log_error("Failure copying city name");
            free(handle->weather_query.name);
            result = __LINE__;
        }
        else if (!handle->is_open && open_connection(handle) != 0)
        {
            log_error("Failure opening connection");
            free(handle->weather_query.name);
            result = __LINE__;
        }
        else
        {
            handle->timeout_sec = timeout;
            handle->query_type = QUERY_TYPE_NAME;
            result = 0;
        }
    }
    return result;
}

void weather_client_process(WEATHER_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        uhttp_client_dowork(handle->http_handle);

        switch (handle->state)
        {
            case WEATHER_CLIENT_STATE_IDLE:
            case WEATHER_CLIENT_STATE_CONNECTED:
                break;
            case WEATHER_CLIENT_STATE_SEND:
                // Send the weather data
                if (send_weather_data(handle) != 0)
                {
                    log_error("Failure sending data to weather service");
                    handle->conditions_callback(handle->condition_ctx, WEATHER_OP_RESULT_COMM_ERR, NULL);
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else if (handle->timeout_sec != 0 && alarm_timer_start(handle->timer_handle, handle->timeout_sec) != 0)
                {
                    log_error("Failure setting the timeout value");
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    handle->state = WEATHER_CLIENT_STATE_SENT;
                }
                break;
            case WEATHER_CLIENT_STATE_CONNECTING:
            case WEATHER_CLIENT_STATE_SENT:
                if (alarm_timer_is_expired(handle->timer_handle))
                {
                    handle->conditions_callback(handle->condition_ctx, WEATHER_OP_RESULT_TIMEOUT, NULL);
                    close_http_connection(handle);
                    handle->state = WEATHER_CLIENT_STATE_IDLE;
                }
                break;
            case WEATHER_CLIENT_STATE_RECV:
            {
                WEATHER_CONDITIONS weather_cond = {0};
                if (parse_weather_data(handle, &weather_cond) != 0)
                {
                    log_error("Failure parsing weather data");
                    handle->conditions_callback(handle->condition_ctx, WEATHER_OP_RESULT_INVALID_DATA_ERR, NULL);
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    handle->state = WEATHER_CLIENT_STATE_IDLE;
                }
                break;
            }
            case WEATHER_CLIENT_STATE_ERROR:
                break;
        }
    }
}
