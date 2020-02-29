// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"
#include "lib-util-c/app_logging.h"

#include "ntp_client.h"
#include "weather_client.h"
#include "config_mgr.h"
#include "alarm_scheduler.h"
#include "sound_mgr.h"
#include "gui_mgr.h"

typedef enum OPERATION_STATE_TAG
{
    OPERATION_STATE_IDLE,
    OPERATION_STATE_ERROR,
    OPERATION_STATE_SUCCESS
} OPERATION_STATE;

#define NTP_TIMEOUT         5

static bool g_run_application = true;

static void thread_mgr_sleep(unsigned int milliseconds)
{
#ifdef WIN32
    Sleep(milliseconds);
#else
    time_t seconds = milliseconds / 1000;
    long nsRemainder = (milliseconds % 1000) * 1000000;
    struct timespec timeToSleep = { seconds, nsRemainder };
    (void)nanosleep(&timeToSleep, NULL);
#endif
}

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    OPERATION_STATE* op_state = (OPERATION_STATE*)user_ctx;
    if (ntp_result == NTP_OP_RESULT_SUCCESS)
    {
        #if PRODUCTION
            // Set time
        #endif
        *op_state = OPERATION_STATE_SUCCESS;
        printf("Time: %s\r\n", ctime((const time_t*)&current_time) );
    }
    else
    {
        *op_state = OPERATION_STATE_ERROR;
        printf("Failure retrieving NTP time %d\r\n", ntp_result);
    }
}

static int check_ntp_time(CONFIG_MGR_HANDLE config_mgr)
{
    int result;
    NTP_CLIENT_HANDLE ntp_client = ntp_client_create();
    if (ntp_client != NULL)
    {
        OPERATION_STATE op_state = OPERATION_STATE_IDLE;
        const char* ntp_address;

        if ((ntp_address = config_mgr_get_ntp_address(config_mgr)) == NULL)
        {
            printf("Failure getting ntp address from config file");
            result = __LINE__;
        }
        else if (ntp_client_get_time(ntp_client, ntp_address, NTP_TIMEOUT, ntp_result_callback, &op_state) == 0)
        {
            do
            {
                ntp_client_process(ntp_client);
                // Sleep here
            } while (op_state == OPERATION_STATE_IDLE);

            if (op_state == OPERATION_STATE_SUCCESS)
            {
                result = 0;
            }
            else
            {
                printf("NTP processing operation failure\r\n");
                result = __LINE__;
            }
        }
        else
        {
            printf("NTP get_time operation failure\r\n");
            result = __LINE__;
        }
        ntp_client_destroy(ntp_client);
    }
    else
    {
        printf("Failure creating ntp object");
        result = __LINE__;
    }
    return result;
}

/*static void weather_cond_callback(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* cond)
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
}*/

static int load_alarms_cb(void* context, const CONFIG_ALARM_INFO* cfg_alarm)
{
    int result;
    SCHEDULER_HANDLE sched_mgr = (SCHEDULER_HANDLE)context;
    if (sched_mgr == NULL)
    {
        printf("Invalid user context specfied");
        result = __LINE__;
    }
    else
    {
        TIME_INFO time_info;
        time_info.hour = cfg_alarm->time_array[0];
        time_info.min = cfg_alarm->time_array[1];
        time_info.sec = cfg_alarm->time_array[2];

        if (alarm_scheduler_add_alarm(sched_mgr, cfg_alarm->name, &time_info, cfg_alarm->frequency, cfg_alarm->sound_file) != 0)
        {
            printf("Failure adding alarm %s", cfg_alarm->name);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

static int initialize_system(CONFIG_MGR_HANDLE config_mgr, SCHEDULER_HANDLE sched_mgr)
{
    int result;
    if (config_mgr_load_alarm(config_mgr, load_alarms_cb, sched_mgr) != 0)
    {
        printf("Failure loading alarms from config file");
        result = __LINE__;
    }
    else if (!g_run_application && check_ntp_time(config_mgr) != 0)
    {
        printf("Failure checking ntp time");
        result = __LINE__;
    }
    else
    {
        // Setup the gui window
        result = 0;
    }
    return result;
}

static const char* parse_command_line(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    const char* result;
    result = "/home/jebrando/development/repo/personal/smartclock/config/";
    return result;
}

int main(int argc, char* argv[])
{
    SCHEDULER_HANDLE sched_mgr;
    CONFIG_MGR_HANDLE config_mgr;
    SOUND_MGR_HANDLE sound_mgr;
    GUI_MGR_HANDLE gui_mgr;
    const char* config_filepath;

    if ((config_filepath = parse_command_line(argc, argv)) == NULL)
    {
        printf("Failure parse command line");
    }
    else if ((config_mgr = config_mgr_create(config_filepath)) == NULL ||
        (sched_mgr = alarm_scheduler_create()) == NULL ||
        (sound_mgr = sound_mgr_create()) == NULL ||
        (gui_mgr = gui_mgr_create()) == NULL)
    {
        printf("Failure creating configuration objects");
    }
    else
    {
        if (initialize_system(config_mgr, sched_mgr) != 0)
        {
            printf("Failure initializing system");
        }
        else if (gui_mgr_initialize(gui_mgr) != 0)
        {
            printf("Failure initializing gui manager");
        }
        else if (gui_create_win(gui_mgr) != 0)
        {
            printf("Failure creating window");
        }
        else
        {
            do
            {
                // Sleep here
                gui_mgr_process_items(gui_mgr);

                thread_mgr_sleep(5);
            } while (g_run_application);
        }

        gui_mgr_destroy(gui_mgr);
        sound_mgr_destroy(sound_mgr);
        alarm_scheduler_destroy(sched_mgr);
        config_mgr_destroy(config_mgr);
    }
    return 0;
}