// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parson.h"
#include "lib-util-c/app_logging.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/thread_mgr.h"
#include "lib-util-c/crt_extensions.h"

#include "ntp_client.h"
#include "weather_client.h"
#include "config_mgr.h"
#include "alarm_scheduler.h"
#include "sound_mgr.h"
#include "gui_mgr.h"
#include "time_mgr.h"

#include "smartclock.h"

typedef enum OPERATION_STATE_TAG
{
    OPERATION_STATE_IDLE,
    OPERATION_STATE_IN_PROCESS,
    OPERATION_STATE_ERROR,
    OPERATION_STATE_SUCCESS
} OPERATION_STATE;

typedef struct SMARTCLOCK_INFO_TAG
{
    SCHEDULER_HANDLE sched_mgr;
    CONFIG_MGR_HANDLE config_mgr;
    SOUND_MGR_HANDLE sound_mgr;
    GUI_MGR_HANDLE gui_mgr;

    uint8_t last_alarm_min;

    NTP_CLIENT_HANDLE ntp_client;
    OPERATION_STATE ntp_operation;
    ALARM_TIMER_INFO ntp_alarm;

    WEATHER_CLIENT_HANDLE weather_client;
    ALARM_TIMER_INFO weather_alarm;
    OPERATION_STATE weather_operation;

    const char* weather_appid;
    char* config_path;
} SMARTCLOCK_INFO;

typedef enum ARGUEMENT_TYPE_TAG
{
    ARGUEMENT_TYPE_UNKNOWN,
    ARGUEMENT_TYPE_WEATHER_APPID
} ARGUEMENT_TYPE;

#define OPERATION_TIMEOUT       5
#define MAX_TIME_DIFFERENCE     50
#define MAX_WEATHER_DIFF        6*60*60 // Every 6 hours
#define MAX_TIME_OFFSET         2*60    // 2 min

//static const char* const ENV_WEATHER_APP_ID = "weather_appid";
static const char* const CONFIG_FOLDER_NAME = "config";
static const char OS_FILE_SEPARATOR = '/';
static const char* OS_FILE_SEPARATOR_FMT = "%s/";

static bool g_run_application = true;

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    OPERATION_STATE* op_state = (OPERATION_STATE*)user_ctx;
    if (ntp_result == NTP_OP_RESULT_SUCCESS)
    {
        time_t sys_time = time(NULL);
        double delta = difftime(sys_time, current_time);
        if ((delta < 0 ? delta*-1 : delta) > MAX_TIME_OFFSET)
        {
            set_machine_time(&current_time);
        }
        *op_state = OPERATION_STATE_SUCCESS;
    }
    else
    {
        *op_state = OPERATION_STATE_ERROR;
        log_error("Failure retrieving NTP time %d", ntp_result);
    }
}

static int check_ntp_time(const char* ntp_address, SMARTCLOCK_INFO* clock_info)
{
    int result;
    if (ntp_client_get_time(clock_info->ntp_client, ntp_address, OPERATION_TIMEOUT, ntp_result_callback, &clock_info->ntp_operation) == 0)
    {
        clock_info->ntp_operation = OPERATION_STATE_IN_PROCESS;
        result = 0;
    }
    else
    {
        clock_info->ntp_operation = OPERATION_STATE_ERROR;
        log_error("NTP get_time operation failure");
        result = __LINE__;
    }
    return result;
}

static void weather_cond_callback(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* cond)
{
    SMARTCLOCK_INFO* clock_info = (SMARTCLOCK_INFO*)user_ctx;
    if (clock_info == NULL)
    {
        log_error("weather callback failure with invalid user ctx");
    }
    else
    {
        if (result == WEATHER_OP_RESULT_SUCCESS)
        {
            (void)printf("Icon %s\nDesc: %s\nTemp: %f\nHi: %f\nLo: %f\nHumidity: %d\nPressure: %d\n",
                cond->weather_icon, cond->description, cond->temperature, cond->hi_temp, cond->lo_temp, cond->humidity, cond->pressure);
            gui_mgr_set_forcast(clock_info->gui_mgr, FORCAST_TODAY, cond);
        }
        else
        {
        }
        (void)alarm_timer_reset(&clock_info->weather_alarm);
    }
}

static void check_weather_operation(SMARTCLOCK_INFO* clock_info)
{
    if (clock_info->weather_operation == OPERATION_STATE_IN_PROCESS)
    {
        weather_client_process(clock_info->weather_client);
    }
    else if (clock_info->weather_operation == OPERATION_STATE_ERROR)
    {
        // todo: Need to alert the user and show config dialog
        alarm_timer_reset(&clock_info->weather_alarm);
    }
    else if (alarm_timer_is_expired(&clock_info->weather_alarm))
    {
        const char* zipcode = config_mgr_get_zipcode(clock_info->config_mgr);
        if (zipcode == NULL)
        {
            // todo: Need to alert the user and show config dialog
            clock_info->weather_operation = OPERATION_STATE_ERROR;
            log_error("Invalid zipcode specfied");
        }
#if PRODUCTION
        else if (weather_client_get_by_zipcode(clock_info->weather_client, zipcode, OPERATION_TIMEOUT, weather_cond_callback, clock_info))
        {
            log_error("Failure getting weather information");
        }
#else
        WEATHER_CONDITIONS cond = {0};
        cond.temperature = 43;
        cond.weather_icon[0] = '0';
        cond.weather_icon[1] = '9';
        cond.weather_icon[2] = 'd';

        weather_cond_callback(clock_info, WEATHER_OP_RESULT_SUCCESS, &cond);
#endif
    }
}

void check_ntp_operation(SMARTCLOCK_INFO* clock_info)
{
    if (clock_info->ntp_operation == OPERATION_STATE_IN_PROCESS)
    {
        ntp_client_process(clock_info->ntp_client);
    }
    else if (clock_info->ntp_operation == OPERATION_STATE_ERROR)
    {
        // todo: Need to alert the user and show config dialog
        alarm_timer_reset(&clock_info->ntp_alarm);
    }
    else if (alarm_timer_is_expired(&clock_info->ntp_alarm))
    {
        const char* ntp_address = config_mgr_get_ntp_address(clock_info->config_mgr);
        if (ntp_address == NULL)
        {
            // todo: Need to alert the user and show config dialog
            log_error("Ntp Address is not entered");
            clock_info->ntp_operation = OPERATION_STATE_ERROR;
        }
#if PRODUCTION
        else if (check_ntp_time(ntp_address, clock_info) != 0)
        {
            log_error("Failure getting ntp information");
        }
#endif
        (void)alarm_timer_reset(&clock_info->ntp_alarm);
    }
}

void check_alarm_operation(SMARTCLOCK_INFO* clock_info, const struct tm* curr_time)
{
    if (clock_info->last_alarm_min != curr_time->tm_min)
    {
        log_debug("Checking if alarm should trigger %d", curr_time->tm_min);
        clock_info->last_alarm_min = curr_time->tm_min;
    }
}

static int load_alarms_cb(void* context, const CONFIG_ALARM_INFO* cfg_alarm)
{
    int result;
    SCHEDULER_HANDLE sched_mgr = (SCHEDULER_HANDLE)context;
    if (sched_mgr == NULL)
    {
        log_error("Invalid user context specfied");
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
            log_error("Failure adding alarm %s", cfg_alarm->name);
            result = __LINE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

static int parse_command_line(int argc, char* argv[], SMARTCLOCK_INFO* clock_info)
{
    int result = 0;
    ARGUEMENT_TYPE argument_type = ARGUEMENT_TYPE_UNKNOWN;

    for (size_t index = 0; index < (size_t)argc; index++)
    {
        if (index == 0)
        {
            // Parse the exe line
            // todo: Reverse through the string until you find the file separator
            const char* iterator = (const char*)argv[index];
            size_t len = strlen(iterator);
            for (size_t inner = len; inner > 0; inner--)
            {
                if (iterator[inner] == OS_FILE_SEPARATOR)
                {
                    if (clone_string_with_size_format(&clock_info->config_path, iterator, inner+1, OS_FILE_SEPARATOR_FMT, CONFIG_FOLDER_NAME) != 0)
                    {
                        log_error("Failure cloning the configuration path");
                        result = __LINE__;
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }
        else if (argument_type == ARGUEMENT_TYPE_UNKNOWN)
        {
            if (argv[index][0] == '-')
            {
                if (argv[index][1] == '-')
                {
                    if (strcmp(argv[index], "--weather_appid") == 0)
                    {
                        argument_type = ARGUEMENT_TYPE_WEATHER_APPID;
                    }
                }
            }
        }
        else
        {
            switch (argument_type)
            {
                case ARGUEMENT_TYPE_WEATHER_APPID:
                    clock_info->weather_appid = argv[index];
                    break;
                case ARGUEMENT_TYPE_UNKNOWN:
                    result = __LINE__;
                    break;
            }
        }
    }
    return result;
}

int initialize_data(SMARTCLOCK_INFO* clock_info)
{
    int result;
    if ((clock_info->config_mgr = config_mgr_create(clock_info->config_path)) == NULL)
    {
        log_error("Failure creating config object");
        result = __LINE__;
    }
    else if ((clock_info->sched_mgr = alarm_scheduler_create()) == NULL)
    {
        log_error("Failure creating scheduler object");
        config_mgr_destroy(clock_info->config_mgr);
        result = __LINE__;
    }
    else if ((clock_info->sound_mgr = sound_mgr_create()) == NULL)
    {
        log_error("Failure creating sound manager object");
        config_mgr_destroy(clock_info->config_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        result = __LINE__;
    }
    else if ((clock_info->gui_mgr = gui_mgr_create()) == NULL)
    {
        log_error("Failure creating gui object");
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        result = __LINE__;
    }
    else if ((clock_info->ntp_client = ntp_client_create()) == NULL)
    {
        log_error("Failure creating ntp client object");
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        gui_mgr_destroy(clock_info->gui_mgr);
        result = __LINE__;
    }
    else if ((clock_info->weather_client = weather_client_create(clock_info->weather_appid, UNIT_FAHRENHEIGHT)) == NULL)
    {
        log_error("Failure creating weather client object");
        ntp_client_destroy(clock_info->ntp_client);
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        gui_mgr_destroy(clock_info->gui_mgr);
        result = __LINE__;
    }
    else
    {
        result = 0;
    }
    return result;
}

int run_application(int argc, char* argv[])
{
    SMARTCLOCK_INFO clock_info = {0};

    if (parse_command_line(argc, argv, &clock_info) != 0)
    {
        log_error("Failure parse command line");
    }
    else if (initialize_data(&clock_info) != 0)
    {
        log_error("Failure creating configuration objects");
        free(clock_info.config_path);
    }
    else
    {
        if (config_mgr_load_alarm(clock_info.config_mgr, load_alarms_cb, clock_info.sched_mgr) != 0)
        {
            log_error("Failure loading alarms from config file");
        }
        else if (gui_create_win(clock_info.gui_mgr) != 0)
        {
            log_error("Failure creating window");
        }
        else
        {
            clock_info.ntp_operation = OPERATION_STATE_IDLE;
            clock_info.weather_operation = OPERATION_STATE_IDLE;
            (void)alarm_timer_init(&clock_info.ntp_alarm);
            (void)alarm_timer_init(&clock_info.weather_alarm);

            // Get the inital weather
            check_ntp_operation(&clock_info);
            check_weather_operation(&clock_info);

            (void)alarm_timer_start(&clock_info.ntp_alarm, MAX_TIME_DIFFERENCE);
            (void)alarm_timer_start(&clock_info.weather_alarm, MAX_WEATHER_DIFF);

            do
            {
                check_ntp_operation(&clock_info);

                check_weather_operation(&clock_info);

                // Get the current time value
                struct tm* curr_time = get_time_value();
                check_alarm_operation(&clock_info, curr_time);

                gui_mgr_set_time_item(clock_info.gui_mgr, curr_time);

                // Sleep here
                gui_mgr_process_items(clock_info.gui_mgr);

                thread_mgr_sleep(5);
            } while (g_run_application);
        }

        ntp_client_destroy(clock_info.ntp_client);
        gui_mgr_destroy(clock_info.gui_mgr);
        sound_mgr_destroy(clock_info.sound_mgr);
        alarm_scheduler_destroy(clock_info.sched_mgr);
        config_mgr_destroy(clock_info.config_mgr);
        weather_client_destroy(clock_info.weather_client);
        free(clock_info.config_path);
    }
    return 0;
}
