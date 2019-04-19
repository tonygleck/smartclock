
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ntp_client.h"
#include "weather_client.h"
#include "parson.h"
#include "lib-util-c/app_logging.h"

static const char* NTP_SERVER_ADDRESS = "0.north-america.pool.ntp.org";

#define NTP_TIMEOUT         5

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    int* operation_complete = (int*)user_ctx;
    if (ntp_result == NTP_OP_RESULT_SUCCESS)
    {
        printf("Time: %s\r\n", ctime((const time_t*)&current_time) );
    }
    else
    {
        printf("Failure retrieving NTP time %d\r\n", ntp_result);
    }
    *operation_complete = 1;
}

static int check_ntp_time(void)
{
    NTP_CLIENT_HANDLE ntp_client = ntp_client_create();
    if (ntp_client != NULL)
    {
        int operation_complete = 0;
        printf("Calling ntp client get time\r\n");
        if (ntp_client_get_time(ntp_client, NTP_SERVER_ADDRESS, NTP_TIMEOUT, ntp_result_callback, &operation_complete) == 0)
        {
            do
            {
                ntp_client_process(ntp_client);
                // Sleep here
            } while (operation_complete == 0);
        }
    }
    return 0;
}

static void weather_cond_callback(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* cond)
{
    if (result == WEATHER_OP_RESULT_SUCCESS)
    {
        (void)printf("Icon %s\nDesc: %s\nTemp: %f\nHi: %f\nLo: %f\nHumidity: %d\nPressure: %d\n",
            cond->weather_icon, cond->description, cond->temperature, cond->hi_temp, cond->lo_temp, cond->humidity, cond->pressure);
    }
}

static void check_weather_value()
{
    WEATHER_CLIENT_HANDLE handle = weather_client_create("", UNIT_FAHRENHEIGHT);
    if (handle == NULL)
    {
        weather_client_destroy(handle);
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    return 0;
}