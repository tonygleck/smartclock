// Licensed under the MIT license. See LICENSE file in the project root for full license information.

//#define WEATHER_PROD        1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "lib-util-c/sys_debug_shim.h"
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
    uint8_t last_weather_day;

#ifdef USE_NTP_CLIENT
    NTP_CLIENT_HANDLE ntp_client;
#endif
    OPERATION_STATE ntp_operation;

    ALARM_TIMER_INFO ntp_alarm;
    ALARM_STATE_RESULT alarm_op_state;

    WEATHER_CLIENT_HANDLE weather_client;
    ALARM_TIMER_INFO weather_timer;
    OPERATION_STATE weather_operation;

    ALARM_TIMER_INFO max_alarm_len;
    const ALARM_INFO* triggered_alarm;

    uint32_t alarm_volume;
    const char* weather_appid;
    char* config_path;

    TIME_VALUE_STORAGE shade_start;
    TIME_VALUE_STORAGE shade_end;
    bool shades_down;
    bool is_demo_mode;
} SMARTCLOCK_INFO;

typedef enum ARGUEMENT_TYPE_TAG
{
    ARGUEMENT_TYPE_UNKNOWN,
    ARGUEMENT_TYPE_WEATHER_APPID
} ARGUEMENT_TYPE;

#define OPERATION_TIMEOUT       5
#define MAX_TIME_DIFFERENCE     2*60*60 // Every 2 hours
#define MAX_WEATHER_DIFF        3*60*60 // Every 3 hours
#define MAX_TIME_OFFSET         2*60    // 2 min
#define MAX_ALARM_RING_TIME     2*60    // 2 min
#define INVALID_HOUR_VALUE      24      // Invalid hour

//static const char* const ENV_WEATHER_APP_ID = "weather_appid";
static const char* const CONFIG_FOLDER_NAME = "config";
static const char OS_FILE_SEPARATOR = '/';
static const char* OS_FILE_SEPARATOR_FMT = "%s/";

static bool g_run_application = true;

// Callback information
static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    SMARTCLOCK_INFO* clock_info = (SMARTCLOCK_INFO*)user_ctx;
    if (clock_info == NULL)
    {
        log_error("Ntp callback user context is NULL");
    }
    else
    {
        if (ntp_result == NTP_OP_RESULT_SUCCESS)
        {
            time_t sys_time = time(NULL);
            double delta = difftime(sys_time, current_time);
            if ((delta < 0 ? delta*-1 : delta) > MAX_TIME_OFFSET)
            {
                set_machine_time(&current_time);
            }
            clock_info->ntp_operation = OPERATION_STATE_SUCCESS;
        }
        else
        {
            clock_info->ntp_operation = OPERATION_STATE_ERROR;
            log_error("Failure retrieving NTP time %d", ntp_result);
        }
        (void)alarm_timer_reset(&clock_info->ntp_alarm);
    }
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
            gui_mgr_set_forcast(clock_info->gui_mgr, FORCAST_TODAY, cond);
            clock_info->weather_operation = OPERATION_STATE_SUCCESS;
        }
        else
        {
            clock_info->weather_operation = OPERATION_STATE_ERROR;
            log_error("Failure retrieving weather info %d", result);
        }
        (void)alarm_timer_reset(&clock_info->weather_timer);
    }
}

static void check_weather_operation(SMARTCLOCK_INFO* clock_info, uint8_t curr_day)
{
    if (clock_info->weather_operation == OPERATION_STATE_IN_PROCESS)
    {
        if (clock_info->is_demo_mode)
        {
            WEATHER_CONDITIONS cond = {0};
            cond.forcast_date = get_time_value();
            cond.temperature = -11;
            cond.lo_temp = -10;
            cond.hi_temp = 100;
            cond.description = "Partly Sunny";
            cond.weather_icon[0] = '0';
            cond.weather_icon[1] = '9';
            cond.weather_icon[2] = 'd';
            weather_cond_callback(clock_info, WEATHER_OP_RESULT_SUCCESS, &cond);
        }
        else
        {
            weather_client_process(clock_info->weather_client);
        }
    }
    else if (clock_info->weather_operation == OPERATION_STATE_ERROR)
    {
        log_error("Failure getting weather operations");
        // Close the weather client
        weather_client_close(clock_info->weather_client);
        clock_info->weather_operation = OPERATION_STATE_IDLE;
        alarm_timer_reset(&clock_info->weather_timer);
    }
    else if (clock_info->weather_operation == OPERATION_STATE_SUCCESS)
    {
        clock_info->weather_operation = OPERATION_STATE_IDLE;
    }
    else if (alarm_timer_is_expired(&clock_info->weather_timer) || clock_info->last_weather_day != curr_day)
    {
        clock_info->last_weather_day = curr_day;

        log_debug("Calling Weather service");
        const char* zipcode = config_mgr_get_zipcode(clock_info->config_mgr);
        if (zipcode == NULL)
        {
            clock_info->weather_operation = OPERATION_STATE_ERROR;
            log_error("Invalid zipcode specfied");
        }
        else if (!clock_info->is_demo_mode && weather_client_get_by_zipcode(clock_info->weather_client, zipcode, OPERATION_TIMEOUT, weather_cond_callback, clock_info) != 0)
        {
            log_error("Failure getting weather information");
            clock_info->weather_operation = OPERATION_STATE_ERROR;
        }
        else
        {
            clock_info->weather_operation = OPERATION_STATE_IN_PROCESS;
        }
    }
}

static void check_ntp_operation(SMARTCLOCK_INFO* clock_info)
{
    (void)clock_info;
#ifdef USE_NTP_CLIENT
    if (clock_info->ntp_operation == OPERATION_STATE_IN_PROCESS)
    {
        if (clock_info->is_demo_mode)
        {
            ntp_result_callback(clock_info, NTP_OP_RESULT_SUCCESS, time(NULL));
        }
        else
        {
            ntp_client_process(clock_info->ntp_client);
        }
    }
    else if (clock_info->ntp_operation == OPERATION_STATE_ERROR)
    {
        // todo: Need to alert the user and show config dialog
        alarm_timer_reset(&clock_info->ntp_alarm);
        clock_info->ntp_operation = OPERATION_STATE_IDLE;
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
        else if (!clock_info->is_demo_mode && ntp_client_get_time(clock_info->ntp_client, ntp_address, OPERATION_TIMEOUT, ntp_result_callback, clock_info) != 0)
        {
            clock_info->ntp_operation = OPERATION_STATE_ERROR;
            log_error("NTP get_time operation failure");
        }
        else
        {
            clock_info->ntp_operation = OPERATION_STATE_IN_PROCESS;
        }
    }
#endif
}

static void configure_alarms(SMARTCLOCK_INFO* clock_info)
{
    // Show alarm dialog on gui
    gui_mgr_show_alarm_dlg(clock_info->gui_mgr, clock_info->sched_mgr);

    // Check
}

static void adjust_shades(SMARTCLOCK_INFO* clock_info, const struct tm* curr_time)
{
    if ((clock_info->shade_start.hours < INVALID_HOUR_VALUE && clock_info->shade_start.hours < INVALID_HOUR_VALUE) || clock_info->alarm_op_state == ALARM_STATE_TRIGGERED)
    {
        // Check shade
        if (clock_info->alarm_op_state == ALARM_STATE_TRIGGERED)
        {
            if (clock_info->shades_down)
            {
                log_debug("Turning shades on");
                clock_info->shades_down = false;
            }
        }
        else
        {
            if (clock_info->shades_down)
            {
                if (curr_time->tm_hour == clock_info->shade_start.hours && curr_time->tm_min == clock_info->shade_start.minutes)
                {
                    // TODO: Turn up the shades
                    log_debug("Turning shades on");
                    clock_info->shades_down = false;
                }
            }
            else
            {
                if (curr_time->tm_hour == clock_info->shade_end.hours && curr_time->tm_min == clock_info->shade_end.minutes)
                {
                    // TODO: Turn down the shades
                    log_debug("Turning shades off");
                    clock_info->shades_down = true;
                }
            }
        }
    }
}

static void gui_notification_cb(void* user_ctx, GUI_NOTIFICATION_TYPE type, void* res_value)
{
    SMARTCLOCK_INFO* clock_info = (SMARTCLOCK_INFO*)user_ctx;
    if (clock_info == NULL)
    {
        log_error("Invalid user context specfied");
    }
    else
    {
        if (type == NOTIFICATION_ALARM_RESULT)
        {
            ALARM_STATE_RESULT* alarm_result = (ALARM_STATE_RESULT*)res_value;

            sound_mgr_stop(clock_info->sound_mgr);
            if (*alarm_result == ALARM_STATE_STOPPED)
            {
                clock_info->alarm_op_state = ALARM_STATE_STOPPED;
            }
            else
            {
                clock_info->alarm_op_state = ALARM_STATE_SNOOZE;
                (void)alarm_scheduler_snooze_alarm(clock_info->sched_mgr, clock_info->triggered_alarm);
            }
            gui_mgr_set_next_alarm(clock_info->gui_mgr, alarm_scheduler_get_next_alarm(clock_info->sched_mgr));
            clock_info->triggered_alarm = NULL;
        }
        else if (type == NOTIFICATION_APPLICATION_RESULT)
        {
            g_run_application = false;
        }
        else if (type == NOTIFICATION_OPTION_DLG)
        {
            GUI_OPTION_DLG_INFO* option_info = (GUI_OPTION_DLG_INFO*)res_value;
            if (option_info->dlg_state == OPTION_DLG_OPEN)
            {
                configure_alarms(clock_info);
            }
            else
            {
                if (option_info->is_dirty)
                {
                    if (!config_mgr_save(clock_info->config_mgr))
                    {
                        log_error("Failure saving configuration");
                    }
                }
                // The option dialog just closed
                gui_mgr_set_next_alarm(clock_info->gui_mgr, alarm_scheduler_get_next_alarm(clock_info->sched_mgr));
            }
        }
    }
}

static void play_alarm_sound(SMARTCLOCK_INFO* clock_info, const ALARM_INFO* alarm_info)
{
    char sound_filename[1024];
    const char* audio_dir = config_mgr_get_audio_dir(clock_info->config_mgr);
    if (audio_dir == NULL)
    {
        // todo: need to do something to tell the user
        log_error("Invalid configuration file unable to retrieve audio directory");
    }
    else
    {
        // construct names
        sprintf(sound_filename, "%s%s", audio_dir, alarm_info->sound_file);
        if (sound_mgr_play(clock_info->sound_mgr, sound_filename, true, false) != 0)
        {
            // todo: need to do something to tell the user
            log_error("Failure playing sound file for alarm: %s", alarm_info->alarm_text);
        }
    }
}

static void stop_alarm_sound(SMARTCLOCK_INFO* clock_info)
{
    if (sound_mgr_stop(clock_info->sound_mgr) != 0)
    {
        log_error("Failure stopping alarm sound");
    }
}

static void check_alarm_operation(SMARTCLOCK_INFO* clock_info, const struct tm* curr_time)
{
    if (clock_info->last_alarm_min != curr_time->tm_min)
    {
        const struct tm* time_value = get_time_value();
        const ALARM_INFO* triggered = alarm_scheduler_is_triggered(clock_info->sched_mgr, time_value);
        if (triggered != NULL)
        {
            // Trigger Alarm to fire
            gui_mgr_set_alarm_triggered(clock_info->gui_mgr, triggered);

            play_alarm_sound(clock_info, triggered);

            clock_info->alarm_op_state = ALARM_STATE_TRIGGERED;
            (void)alarm_timer_start(&clock_info->max_alarm_len, MAX_ALARM_RING_TIME);
            clock_info->triggered_alarm = triggered;
        }
        clock_info->last_alarm_min = curr_time->tm_min;
    }
    if (clock_info->alarm_op_state == ALARM_STATE_TRIGGERED)
    {
        if (alarm_timer_is_expired(&clock_info->max_alarm_len))
        {
            gui_mgr_set_alarm_triggered(clock_info->gui_mgr, NULL);
            gui_mgr_set_next_alarm(clock_info->gui_mgr, alarm_scheduler_get_next_alarm(clock_info->sched_mgr));
            stop_alarm_sound(clock_info);
            clock_info->alarm_op_state = ALARM_STATE_STOPPED;
        }
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
        time_info.hour = cfg_alarm->time_value.hours;
        time_info.min = cfg_alarm->time_value.minutes;
        time_info.sec = cfg_alarm->time_value.seconds;
        if (alarm_scheduler_add_alarm(sched_mgr, cfg_alarm->name, &time_info, cfg_alarm->frequency, cfg_alarm->sound_file, cfg_alarm->snooze) != 0)
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
                    free(clock_info->config_path);
                    result = __LINE__;
                    break;
            }
        }
    }

    if (clock_info->weather_appid == NULL)
    {
        log_error("Invalid weather app Id specified");
        free(clock_info->config_path);
        result = __LINE__;
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
    else if ((clock_info->gui_mgr = gui_mgr_create(clock_info->config_mgr, gui_notification_cb, clock_info)) == NULL)
    {
        log_error("Failure creating gui object");
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        result = __LINE__;
    }
#ifdef USE_NTP_CLIENT
    else if ((clock_info->ntp_client = ntp_client_create()) == NULL)
    {
        log_error("Failure creating ntp client object");
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        gui_mgr_destroy(clock_info->gui_mgr);
        result = __LINE__;
    }
#endif
    else if ((clock_info->weather_client = weather_client_create(clock_info->weather_appid)) == NULL)
    {
        log_error("Failure creating weather client object");
#ifdef USE_NTP_CLIENT
        ntp_client_destroy(clock_info->ntp_client);
#endif
        config_mgr_destroy(clock_info->config_mgr);
        sound_mgr_destroy(clock_info->sound_mgr);
        alarm_scheduler_destroy(clock_info->sched_mgr);
        gui_mgr_destroy(clock_info->gui_mgr);
        result = __LINE__;
    }
    else
    {
        (void)alarm_timer_init(&clock_info->weather_timer);
        (void)alarm_timer_init(&clock_info->ntp_alarm);
        (void)alarm_timer_init(&clock_info->max_alarm_len);
        result = 0;
    }
    return result;
}

int run_application(int argc, char* argv[])
{
    int result;
    SMARTCLOCK_INFO clock_info = {0};
    if (parse_command_line(argc, argv, &clock_info) != 0)
    {
        log_error("Failure parse command line");
        result = __LINE__;
    }
    else if (initialize_data(&clock_info) != 0)
    {
        log_error("Failure creating configuration objects");
        free(clock_info.config_path);
        result = __LINE__;
    }
    else
    {
        if (config_mgr_load_alarm(clock_info.config_mgr, load_alarms_cb, clock_info.sched_mgr) != 0)
        {
            log_error("Failure loading alarms from config file");
            result = __LINE__;
        }
        else if (gui_mgr_create_win(clock_info.gui_mgr) != 0)
        {
            log_error("Failure creating window");
            result = __LINE__;
        }
        else
        {
            struct tm* curr_time = get_time_value();
            size_t refresh_time = 0;
            clock_info.ntp_operation = OPERATION_STATE_IDLE;
            clock_info.weather_operation = OPERATION_STATE_IDLE;

            if (config_mgr_get_shade_times(clock_info.config_mgr, &clock_info.shade_start, &clock_info.shade_end) != 0)
            {
                log_warning("Shade times not validate.  Check configuration");
                clock_info.shade_start.hours = INVALID_HOUR_VALUE;
                clock_info.shade_end.hours = INVALID_HOUR_VALUE;
            }

            clock_info.is_demo_mode = config_mgr_is_demo_mode(clock_info.config_mgr);

            // Get the inital weather
            check_ntp_operation(&clock_info);
            check_weather_operation(&clock_info, curr_time->tm_yday);

            (void)alarm_timer_start(&clock_info.ntp_alarm, MAX_TIME_DIFFERENCE);
            (void)alarm_timer_start(&clock_info.weather_timer, MAX_WEATHER_DIFF);

            // Show the next alarm
            gui_mgr_set_next_alarm(clock_info.gui_mgr, alarm_scheduler_get_next_alarm(clock_info.sched_mgr));
            refresh_time = gui_mgr_get_refresh_resolution();

            do
            {
                curr_time = get_time_value();

                // Check against the Ntp server
                check_ntp_operation(&clock_info);

                check_weather_operation(&clock_info, curr_time->tm_yday);

                // Get the current time value
                check_alarm_operation(&clock_info, curr_time);

                // Check the shades
                adjust_shades(&clock_info, curr_time);

                gui_mgr_set_time_item(clock_info.gui_mgr, curr_time);
                gui_mgr_process_items(clock_info.gui_mgr);

                thread_mgr_sleep(refresh_time);
            } while (g_run_application);
            result = 0;
        }

#ifdef USE_NTP_CLIENT
        ntp_client_destroy(clock_info->ntp_client);
#endif
        gui_mgr_destroy(clock_info.gui_mgr);
        sound_mgr_destroy(clock_info.sound_mgr);
        alarm_scheduler_destroy(clock_info.sched_mgr);
        config_mgr_destroy(clock_info.config_mgr);
        weather_client_destroy(clock_info.weather_client);
        free(clock_info.config_path);
    }
    return result;
}
