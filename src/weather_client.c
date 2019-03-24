// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include "lib-util-c/app_logging.h"
#include "lib-util-c/alarm_timer.h"

#include "weather_client.h"

#include "azure_uhttp_c/uhttp.h"
#include "azure_c_shared_utility/platform.h"
#include "azure_c_shared_utility/socketio.h"

/* http://openweathermap.org/ */
const char* WEATHER_API_HOSTNAME = "api.openweathermap.org";
const char* WEATHER_API_PATH_FMT = "/data/2.5/weather?lat=%f&lon=%f&APPID=%s";

#define HTTP_PORT_VALUE     80
#define MAX_CLOSE_ATTEMPTS  10

typedef enum WEATHER_CLIENT_STATE_TAG
{
    WEATHER_CLIENT_STATE_IDLE,
    WEATHER_CLIENT_STATE_CONNECTED,
    WEATHER_CLIENT_STATE_SENT,
    WEATHER_CLIENT_STATE_RECV,
    WEATHER_CLIENT_STATE_ERROR
} WEATHER_CLIENT_STATE;

typedef struct WEATHER_CLIENT_INFO_TAG
{
    HTTP_CLIENT_HANDLE http_handle;
    ALARM_TIMER_HANDLE timer_handle;

    WEATHER_CLIENT_STATE state;

    double latitude;
    double longitude;
    char* api_key;
    size_t timeout_sec;

    bool is_open;
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
    else
    {

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

static int send_weather_data(WEATHER_CLIENT_INFO* client_info, double latitude, double longitude)
{
    int result;
    char weather_api_path[128];
    sprintf(weather_api_path, WEATHER_API_PATH_FMT, latitude, longitude, client_info->api_key);

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
        else
        {
            result = 0;
        }
    }
    return 0;
}

static void close_http_connection(WEATHER_CLIENT_INFO* client_info)
{
    if (client_info->http_handle != NULL)
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
    }
}

WEATHER_CLIENT_HANDLE weather_client_create(const char* api_key)
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

int weather_client_get_conditions(WEATHER_CLIENT_HANDLE handle, const WEATHER_LOCATION* location, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || location == NULL || conditions_callback == NULL)
    {
        log_error("Invalid parameter specified: handle: %p, location: %p, conditions_callback: %p", handle, location, conditions_callback);
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
            handle->latitude = location->latitude;
            handle->longitude = location->longitude;
            result = 0;
        }
    }
    return result;
}

void weather_client_process(WEATHER_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        if (handle->state == WEATHER_CLIENT_STATE_IDLE)
        {

        }
        else
        {
            uhttp_client_dowork(handle->http_handle);

            if (alarm_timer_start(handle->timer_handle, handle->timeout_sec) != 0)
            {

            }
            /*if (send_weather_data(handle, handle->latitude, handle->longitude) != 0)
            {
                log_error("Failure sending data to weather service");
                result = __LINE__;
            }
    */
        }
    }
}
