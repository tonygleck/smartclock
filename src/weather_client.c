// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib-util-c/sys_debug_shim.h"

#include "lib-util-c/app_logging.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/crt_extensions.h"

#include "weather_client.h"

#include "http_client/http_client.h"
#include "parson.h"

/* http://openweathermap.org/ */
static const char* WEATHER_API_HOSTNAME = "api.openweathermap.org";
static const char* API_COORD_PATH_FMT = "/data/2.5/weather?lat=%f&lon=%f&%s&appid=%s";
static const char* API_NAME_PATH_FMT = "/data/2.5/weather?q=%s&%s&appid=%s";
static const char* API_ZIPCODE_FMT = "/data/2.5/weather?zip=%s,us&%s&appid=%s";

static const char* TEMP_UNIT_FAHRENHEIT_VALUE = "units=imperial";
static const char* TEMP_UNIT_CELSIUS_VALUE = "units=metric";
static const char* TEMP_UNIT_KELVIN_VALUE = "";
/* static const char* DEMO_ACTUAL_WEATHER = "{\"coord\": {\"lon\": -0.13,\"lat\": 51.51 \
 },\"weather\": [ {\"id\": 300,\"main\": \"Drizzle\", \"description\": \"light intensity drizzle\", \
 \"icon\": \"09d\"}],\"base\": \"stations\",\"main\": {\"temp\": 280.32,\"pressure\": 1012,\"humidity\": 81, \
 \"temp_min\": 279.15,\"temp_max\": 281.15},\"visibility\": 10000,\"wind\": {\"speed\": 4.1,\"deg\": 80}, \
 \"clouds\": {\"all\": 90},\"dt\": 1485789600,\"sys\": {\"type\": 1,\"id\": 5091,\"message\": 0.0103,\"country\": \
 \"GB\",\"sunrise\": 1485762037,\"sunset\": 1485794875},\"id\": 2643743,\"name\": \"London\",\"cod\": 200}";*/


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
    WEATHER_CLIENT_STATE_CLOSE,
    WEATHER_CLIENT_STATE_ERROR,
    WEATHER_CLIENT_STATE_CALLBACK,
} WEATHER_CLIENT_STATE;

typedef enum WEATHER_QUERY_TYPE_TAG
{
    QUERY_TYPE_NONE,
    QUERY_TYPE_ZIP_CODE,
    QUERY_TYPE_COORDINATES,
    QUERY_TYPE_NAME
} WEATHER_QUERY_TYPE;

typedef struct WEATHER_CLIENT_INFO_TAG
{
    HTTP_CLIENT_HANDLE http_handle;

    ALARM_TIMER_INFO timer_info;
    char* api_key;
    size_t timeout_sec;

    bool is_open;

    WEATHER_CONDITIONS_CALLBACK conditions_callback;
    void* condition_ctx;
    WEATHER_OPERATION_RESULT op_result;

    const char* temp_units;
    WEATHER_CLIENT_STATE state;
    WEATHER_QUERY_TYPE query_type;
    union
    {
        /* data */
        WEATHER_LOCATION coord_info;
        char* value;
    } weather_query;
    char* weather_data;
} WEATHER_CLIENT_INFO;

static bool is_timed_out(WEATHER_CLIENT_INFO* client_info)
{
    bool result = false;
    if (client_info->timeout_sec > 0)
    {
        result = alarm_timer_is_expired(&client_info->timer_info);
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
            else if ((weather_obj = json_value_get_object(weather_node)) == NULL)
            {
                log_error("Failure getting weather node object");
                result = __LINE__;
            }
            else
            {
                const char* desc_value = json_object_get_string(weather_obj, "description");
                if (desc_value != NULL)
                {
                    char* temp_desc;
                    if (clone_string(&temp_desc, desc_value) == 0)
                    {
                        conditions->description = temp_desc;
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

static int get_forcast_date(JSON_Object* root_obj, WEATHER_CONDITIONS* conditions)
{
    int result;

    time_t forcast_time = json_object_get_number(root_obj, "dt");
    if (forcast_time == 0)
    {
        log_error("Failure unable to get forcast date from weather object");
        result = __LINE__;
    }
    else
    {
        conditions->forcast_date = localtime(&forcast_time);
        result = 0;
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
        else if (get_main_weather_info(root_obj, conditions) != 0)
        {
            // Remove items
            free(conditions->description);
            conditions->description = NULL;
            log_error("Failure parsing main weather info");
            result = __LINE__;
        }
        else if (get_forcast_date(root_obj, conditions) )
        {
            // Remove items
            free(conditions->description);
            conditions->description = NULL;
            log_error("Failure parsing forcast date info");
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

static void on_http_error(void* user_ctx, HTTP_CLIENT_RESULT error_result)
{
    (void)error_result;
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else
    {
        log_error("underlying socket failure encountered %d", (int)error_result);
        client_info->state = WEATHER_CLIENT_STATE_ERROR;
        client_info->op_result = WEATHER_OP_RESULT_COMM_ERR;
    }
}

static void on_http_connected(void* user_ctx, HTTP_CLIENT_RESULT open_result)
{
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else
    {
        if (open_result == HTTP_CLIENT_OK)
        {
            client_info->is_open = true;
            // If the query type is not None then set the state as send
            if (client_info->query_type != QUERY_TYPE_NONE)
            {
                client_info->state = WEATHER_CLIENT_STATE_SEND;
            }
            else
            {
                client_info->state = WEATHER_CLIENT_STATE_CONNECTED;
            }
        }
        else
        {
            // Set Error
            log_error("Failure connecting to server %d", (int)open_result);
            client_info->state = WEATHER_CLIENT_STATE_ERROR;
        }
    }
}

static void on_http_reply_recv(void* user_ctx, HTTP_CLIENT_RESULT request_result, const unsigned char* content, size_t content_len, unsigned int status_code, HTTP_HEADERS_HANDLE response_headers)
{
    (void)response_headers;
    WEATHER_CLIENT_INFO* client_info = (WEATHER_CLIENT_INFO*)user_ctx;
    if (client_info == NULL)
    {
        log_error("Unexpected user context NULL");
    }
    else if (request_result != HTTP_CLIENT_OK)
    {
        log_error("Invalid recv result returned %d", (int)request_result);
        client_info->op_result = WEATHER_OP_RESULT_INVALID_DATA_ERR;
        client_info->state = WEATHER_CLIENT_STATE_ERROR;
    }
    else if (status_code > 300)
    {
        log_error("Invalid status code returned by weather service %d", (int)status_code);
        client_info->op_result = WEATHER_OP_RESULT_STATUS_CODE;
        client_info->state = WEATHER_CLIENT_STATE_ERROR;
    }
    else
    {
        // Make sure we are in the correct state
        if (client_info->state == WEATHER_CLIENT_STATE_SENT)
        {
            // Parse reply
            if (client_info->weather_data == NULL)
            {
                if ((client_info->weather_data = malloc(content_len+1)) == NULL)
                {
                    log_error("Failure allocating weather content of length %zu", content_len);
                    client_info->op_result = WEATHER_OP_RESULT_ALLOCATION_ERR;
                    client_info->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    memset(client_info->weather_data, 0, content_len+1);
                    memcpy(client_info->weather_data, content, content_len);
                    client_info->state = WEATHER_CLIENT_STATE_RECV;
                }
            }
            else
            {
                log_error("Failure incoming weather content when not anticipated");
                free(client_info->weather_data);
                client_info->weather_data = NULL;
                client_info->op_result = WEATHER_OP_RESULT_INVALID_DATA_ERR;
                client_info->state = WEATHER_CLIENT_STATE_ERROR;
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
    int result = 0;
#ifndef DEMO_MODE
    char weather_api_path[128] = {0};
    switch (client_info->query_type)
    {
        case QUERY_TYPE_ZIP_CODE:
            sprintf(weather_api_path, API_ZIPCODE_FMT, client_info->weather_query.value, client_info->temp_units, client_info->api_key);
            break;
        case QUERY_TYPE_COORDINATES:
            sprintf(weather_api_path, API_COORD_PATH_FMT, client_info->weather_query.coord_info.latitude, client_info->weather_query.coord_info.longitude, client_info->temp_units, client_info->api_key);
            break;
        case QUERY_TYPE_NAME:
            sprintf(weather_api_path, API_NAME_PATH_FMT, client_info->weather_query.value, client_info->temp_units, client_info->api_key);
            break;
        case QUERY_TYPE_NONE:
        default:
            result = __LINE__;
            break;
    }

    if (result == 0 && http_client_execute_request(client_info->http_handle, HTTP_CLIENT_REQUEST_GET, weather_api_path, NULL, NULL, 0, on_http_reply_recv, client_info) != 0)
    {
        log_error("Failure executing http request");
        result = __LINE__;
    }
    else if (alarm_timer_start(&client_info->timer_info, client_info->timeout_sec) != 0)
    {
        log_error("Failure setting the timeout value");
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
#endif
    return result;
}

static void close_http_connection(WEATHER_CLIENT_INFO* client_info)
{
#ifndef DEMO_MODE
    if (client_info->is_open)
    {
        http_client_close(client_info->http_handle, on_http_close, client_info);
        size_t attempt = 0;
        do
        {
            http_client_process_item(client_info->http_handle);
            // Make sure we don't just spin here forever
            attempt++;
        } while (client_info->is_open && attempt < MAX_CLOSE_ATTEMPTS);
        if (attempt == MAX_CLOSE_ATTEMPTS)
        {
            log_error("On close callback never called hard close encountered");
            client_info->is_open = false;
        }
    }
    http_client_destroy(client_info->http_handle);
    client_info->http_handle = NULL;
#endif // DEMO_MODE
}

static int open_connection(WEATHER_CLIENT_INFO* client_info)
{
    int result;
#ifndef DEMO_MODE
    HTTP_ADDRESS http_address = {0};
    http_address.hostname = WEATHER_API_HOSTNAME;
    http_address.port = HTTP_PORT_VALUE;
    http_address.is_secure = false;

    if ((client_info->http_handle = http_client_create()) == NULL)
    {
        log_error("Failure creating http client");
        result = __LINE__;
    }
    else
    {
        (void)http_client_set_trace(client_info->http_handle, true);

        if (http_client_open(client_info->http_handle, &http_address, on_http_connected, client_info, on_http_error, client_info) != 0)
        {
            log_error("Failure opening http connection: %s:%d", http_address.hostname, http_address.port);
            http_client_destroy(client_info->http_handle);
            result = __LINE__;
        }
        else if (alarm_timer_start(&client_info->timer_info, client_info->timeout_sec) != 0)
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
#else
    result = 0;
#endif
    return result;
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

        if (clone_string(&result->api_key, api_key) != 0)
        {
            log_error("Failure allocating api key");
            free(result);
            result = NULL;
        }
        else if (alarm_timer_init(&result->timer_info) != 0)
        {
            log_error("Failure creating timer object");
            free(result->api_key);
            free(result);
            result = NULL;
        }
        else
        {
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

        if ((handle->query_type == QUERY_TYPE_NAME || handle->query_type == QUERY_TYPE_ZIP_CODE) && handle->weather_query.value != NULL)
        {
            free(handle->weather_query.value);
            handle->weather_query.value = NULL;
        }
        free(handle->api_key);
        free(handle->weather_data);
        free(handle);
    }
}

int weather_client_close(WEATHER_CLIENT_HANDLE handle)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid parameter specified: handle: NULL");
        result = __LINE__;
    }
    else
    {
        close_http_connection(handle);
        handle->state = WEATHER_CLIENT_STATE_IDLE;
        result = 0;
    }
    return result;
}

int weather_client_get_by_coordinate(WEATHER_CLIENT_HANDLE handle, const WEATHER_LOCATION* location, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || location == NULL || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, location: %p, conditions_callback: %p", handle, location, conditions_callback);
        result = __LINE__;
    }
    else if ((handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CLOSE) && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
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
            handle->state = WEATHER_CLIENT_STATE_SEND;
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

int weather_client_get_by_zipcode(WEATHER_CLIENT_HANDLE handle, const char* zipcode, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || zipcode == NULL || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, zipcode: %s, conditions_callback: %p", handle, zipcode, conditions_callback);
        result = __LINE__;
    }
    else if ((handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CLOSE) && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
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
        else if (clone_string(&handle->weather_query.value, zipcode) != 0)
        {
            log_error("Failure copying city name");
            result = __LINE__;
        }
        else
        {
            handle->state = WEATHER_CLIENT_STATE_SEND;
            // Store data
            handle->query_type = QUERY_TYPE_ZIP_CODE;
            handle->conditions_callback = conditions_callback;
            handle->condition_ctx = user_ctx;
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
    else if ((handle->state != WEATHER_CLIENT_STATE_IDLE && handle->state != WEATHER_CLIENT_STATE_CLOSE) && handle->state != WEATHER_CLIENT_STATE_CONNECTED)
    {
        log_error("Invalid State specified, operation must be complete to add another call");
        result = __LINE__;
    }
    else
    {
        handle->timeout_sec = timeout;
        if (clone_string(&handle->weather_query.value, city_name) != 0)
        {
            log_error("Failure copying city name");
            result = __LINE__;
        }
        else if (!handle->is_open && open_connection(handle) != 0)
        {
            log_error("Failure opening connection");
            free(handle->weather_query.value);
            result = __LINE__;
        }
        else
        {
            handle->state = WEATHER_CLIENT_STATE_SEND;
            handle->conditions_callback = conditions_callback;
            handle->condition_ctx = user_ctx;
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
                    handle->op_result = WEATHER_OP_RESULT_COMM_ERR;
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    handle->state = WEATHER_CLIENT_STATE_SENT;
                }
                break;
            case WEATHER_CLIENT_STATE_CONNECTING:
            case WEATHER_CLIENT_STATE_SENT:
                if (is_timed_out(handle))
                {
                    log_error("Failure, timeout encountered");

                    handle->op_result = WEATHER_OP_RESULT_TIMEOUT;
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                break;
            case WEATHER_CLIENT_STATE_RECV:
            {
                WEATHER_CONDITIONS weather_cond = {0};
                if (parse_weather_data(handle, &weather_cond) != 0)
                {
                    log_error("Failure parsing weather data");
                    handle->op_result = WEATHER_OP_RESULT_INVALID_DATA_ERR;
                    handle->state = WEATHER_CLIENT_STATE_ERROR;
                }
                else
                {
                    handle->conditions_callback(handle->condition_ctx, WEATHER_OP_RESULT_SUCCESS, &weather_cond);
                    handle->state = WEATHER_CLIENT_STATE_CLOSE;

                    // Clear the weather data info
                    free((void*)weather_cond.description);

                }
                if (handle->weather_data != NULL)
                {
                    free(handle->weather_data);
                    handle->weather_data = NULL;
                }
                break;
            }
            case WEATHER_CLIENT_STATE_ERROR:
            case WEATHER_CLIENT_STATE_CALLBACK:
                handle->conditions_callback(handle->condition_ctx, handle->op_result, NULL);
                handle->state = WEATHER_CLIENT_STATE_IDLE;
                if (handle->query_type == QUERY_TYPE_NAME || handle->query_type == QUERY_TYPE_ZIP_CODE)
                {
                    free(handle->weather_query.value);
                    handle->weather_query.value = NULL;
                }
                // fall through
            case WEATHER_CLIENT_STATE_CLOSE:
                close_http_connection(handle);
                break;
        }
        if (handle->state != WEATHER_CLIENT_STATE_CLOSE)
        {
            http_client_process_item(handle->http_handle);
        }
    }
}
