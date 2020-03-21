// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <curses.h>

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"

#include "gui_mgr.h"
#include "config_mgr.h"

#define ASCII_ZERO_VALUE        48
#define SHOW_SEC_VALUE          5
#define SHOW_MIN_VALUE          8
#define RESOLUTION_TIME         500

// Screen Coordinates
#define TIME_START_POS_X        0
#define TIME_START_POS_Y        0

#define DATE_LINE_POS_X         1
#define DATE_LINE_POS_Y         0

#define ALARM_LINE_POS_X        2
#define ALARM_LINE_POS_Y        0

#define NEXT_ALARM_LINE_POS_X   1
#define NEXT_ALARM_LINE_POS_Y   30

#define FORCAST_POS_X           0
#define FORCAST_POS_Y           30

#define CURSOR_REST_POS_X       5
#define CURSOR_REST_POS_Y       0


#define SNOOZE_ALARM_KEY        's'
#define KILL_ALARM_KEY          'k'
#define QUIT_APP_KEY            'q'
#define SHOW_ALARM_DLG          'a'

typedef struct GUI_MGR_INFO_TAG
{
    WINDOW* clock_win;
    GUI_MGR_NOTIFICATION_CB notify_cb;
    void* user_ctx;
    ALARM_TRIGGERED_RESULT alarm_state;
    CONFIG_MGR_HANDLE config_mgr;

} GUI_MGR_INFO;

static const char* get_day_name(int day)
{
    const char* result;
    switch (day)
    {
        case 0:
            result = "Sunday";
            break;
        case 1:
            result = "Monday";
            break;
        case 2:
            result = "Tuesday";
            break;
        case 3:
            result = "Wednesday";
            break;
        case 4:
            result = "Thursday";
            break;
        case 5:
            result = "Friday";
            break;
        case 6:
            result = "Saturday";
            break;
    }
    return result;
}

static const char* get_month_name(int month)
{
    const char* result;
    switch (month)
    {
        case 0:
            result = "January";
            break;
        case 1:
            result = "February";
            break;
        case 2:
            result = "March";
            break;
        case 3:
            result = "April";
            break;
        case 4:
            result = "May";
            break;
        case 5:
            result = "June";
            break;
        case 6:
            result = "July";
            break;
        case 7:
            result = "August";
            break;
        case 8:
            result = "September";
            break;
        case 9:
            result = "October";
            break;
        case 10:
            result = "November";
            break;
        case 11:
            result = "December";
            break;
    }
    return result;
}

static void write_line(GUI_MGR_INFO* gui_info, int x_pos, int y_pos, const char* write_line)
{
    /*size_t date_len = strlen(write_line);
    for (size_t index = 0; index < date_len; index++)
    {
        mvwaddch(gui_info->clock_win, x_pos, y_pos++, write_line[index]);
    }*/
    wmove(gui_info->clock_win, x_pos, y_pos);
    waddstr(gui_info->clock_win, write_line);

    // Always put the cursor back to it's rest position for logging purposes
    wmove(gui_info->clock_win, CURSOR_REST_POS_X, CURSOR_REST_POS_Y);
}

static void set_alarm_state(GUI_MGR_INFO* gui_info, ALARM_TRIGGERED_RESULT alarm_result)
{
    gui_info->notify_cb(gui_info->user_ctx, NOTIFICATION_ALARM_RESULT, &alarm_result);
    gui_info->alarm_state = alarm_result;
    write_line(gui_info, ALARM_LINE_POS_X, ALARM_LINE_POS_Y, "Removing alarm");
    wrefresh(gui_info->clock_win);
}

GUI_MGR_HANDLE gui_mgr_create(CONFIG_MGR_HANDLE config_mgr, GUI_MGR_NOTIFICATION_CB notify_cb, void* user_ctx)
{
    GUI_MGR_INFO* result;
    if (config_mgr == NULL || notify_cb == NULL)
    {
        log_error("Invalid parameter specified config_mgr: NULL");
        result = NULL;
    }
    else if ((result = (GUI_MGR_INFO*)malloc(sizeof(GUI_MGR_INFO))) == NULL)
    {
        log_error("Failure allocating gui manager");
    }
    else
    {
        memset(result, 0, sizeof(GUI_MGR_INFO));
        initscr();
        noecho();
        nodelay(stdscr, TRUE);

        result->config_mgr = config_mgr;
        result->notify_cb = notify_cb;
        result->user_ctx = user_ctx;
        result->clock_win = stdscr;
    }
    return result;
}

void gui_mgr_destroy(GUI_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        endwin();
        free(handle);
    }
}

size_t gui_mgr_get_refresh_resolution(void)
{
    return RESOLUTION_TIME;
}

int gui_mgr_create_win(GUI_MGR_HANDLE handle)
{
    (void)handle;
    return 0;
}

void gui_mgr_set_time_item(GUI_MGR_HANDLE handle, const struct tm* curr_time)
{
    if (handle != NULL)
    {
        int time_x = TIME_START_POS_X;
        int time_y = TIME_START_POS_Y;
        uint8_t adjusted_hour = config_mgr_format_hour(handle->config_mgr, curr_time->tm_hour);

        char time_char = ' ';
        size_t iteration_count = config_mgr_show_seconds(handle->config_mgr) ? SHOW_SEC_VALUE : SHOW_MIN_VALUE;
        for (size_t index = 0; index < iteration_count; index++)
        {
            switch (index)
            {
                case 0:
                    if (adjusted_hour > 19)
                    {
                        time_char = '2';
                    }
                    else if (adjusted_hour > 9)
                    {
                        time_char = '1';
                    }
                    break;
                case 1:
                {
                    int digit = adjusted_hour - ((adjusted_hour / 10)*10);
                    time_char = digit+ASCII_ZERO_VALUE;
                    break;
                }
                case 2:
                    time_char = ':';
                    break;
                case 3:
                {
                    int digit = curr_time->tm_min / 10;
                    time_char = digit+ASCII_ZERO_VALUE;
                    break;
                }
                case 4:
                {
                    int digit = curr_time->tm_min - ((curr_time->tm_min / 10)*10);
                    time_char = digit+ASCII_ZERO_VALUE;
                    break;
                }
                case 5:
                {
                    time_char = ':';
                    break;
                }
                case 6:
                {
                    int digit = curr_time->tm_sec / 10;
                    time_char = digit+ASCII_ZERO_VALUE;
                    break;
                }
                case 7:
                {
                    int digit = curr_time->tm_sec - ((curr_time->tm_sec / 10)*10);
                    time_char = digit+ASCII_ZERO_VALUE;
                    break;
                }
            }
            mvwaddch(stdscr, time_y, time_x++, time_char);
        }

        char date_line[128];
        sprintf(date_line, "%s, %s %d", get_day_name(curr_time->tm_wday), get_month_name(curr_time->tm_mon), curr_time->tm_mday);
        write_line(handle, DATE_LINE_POS_X, DATE_LINE_POS_Y, date_line);
    }
}

void gui_mgr_set_forcast(GUI_MGR_HANDLE handle, FORCAST_TIME timeframe, const WEATHER_CONDITIONS* weather_cond)
{
    (void)timeframe;
    if (handle == NULL || weather_cond == NULL)
    {
        log_error("Invalid variable specified handle: %p, weather_cond: %p", handle, weather_cond);
    }
    else
    {
        char forcast_line[128];
        sprintf(forcast_line, "%.0f f", weather_cond->temperature);
        write_line(handle, FORCAST_POS_X, FORCAST_POS_Y, forcast_line);
    }
}

void gui_mgr_set_next_alarm(GUI_MGR_HANDLE handle, const ALARM_INFO* next_alarm)
{
    if (handle == NULL)
    {
        log_error("Invalid variable specified handle: %p", handle);
    }
    // If we have a next alarm
    else if (next_alarm != NULL)
    {
        char alarm_line[128];
        int trigger_day;
        if ((trigger_day = alarm_scheduler_get_next_day(next_alarm->trigger_days)) >= 0)
        {
            sprintf(alarm_line, "Alarm set: %s %d:%02d %s", get_day_name(trigger_day), config_mgr_format_hour(handle->config_mgr, next_alarm->trigger_time.hour), next_alarm->trigger_time.min, next_alarm->trigger_time.hour > 12 ? "pm" : "am");
            write_line(handle, NEXT_ALARM_LINE_POS_X, NEXT_ALARM_LINE_POS_Y, alarm_line);
        }
    }
}

int gui_mgr_set_alarm_triggered(GUI_MGR_HANDLE handle, const ALARM_INFO* alarm_triggered)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid variable specified handle: %p", handle);
        result = __LINE__;
    }
    else
    {
        if (alarm_triggered == NULL)
        {
            set_alarm_state(handle, ALARM_TRIGGERED_STOPPED);
        }
        else
        {
            char alarm_line[128];
            sprintf(alarm_line, "Alarm Alert: %d:%02d %s (Snooze (%c) - Stop (%c)", config_mgr_format_hour(handle->config_mgr, alarm_triggered->trigger_time.hour), alarm_triggered->trigger_time.min,
                alarm_triggered->alarm_text, SNOOZE_ALARM_KEY, KILL_ALARM_KEY);
            write_line(handle, ALARM_LINE_POS_X, ALARM_LINE_POS_Y, alarm_line);
            handle->alarm_state = ALARM_TRIGGERED_TRIGGERED;
        }
        result = 0;
    }
    return result;
}

void gui_mgr_process_items(GUI_MGR_HANDLE handle)
{
    (void)handle;
    int response_ch;

    if ((response_ch = getch()) != ERR)
    {
        if (handle->alarm_state == ALARM_TRIGGERED_TRIGGERED)
        {
            switch (response_ch)
            {
                case SNOOZE_ALARM_KEY:
                    set_alarm_state(handle, ALARM_TRIGGERED_SNOOZE);
                    break;
                case KILL_ALARM_KEY:
                    set_alarm_state(handle, ALARM_TRIGGERED_STOPPED);
                    break;
            }
        }
        else if (handle->alarm_state == ALARM_TRIGGERED_SNOOZE)
        {
            if (response_ch == KILL_ALARM_KEY)
            {
                set_alarm_state(handle, ALARM_TRIGGERED_STOPPED);
            }
        }
        else if (response_ch == QUIT_APP_KEY)
        {
            handle->notify_cb(handle->user_ctx, NOTIFICATION_APPLICATION_RESULT, NULL);

            // Need to quit here
        }
        else if (response_ch == SHOW_ALARM_DLG)
        {
            handle->notify_cb(handle->user_ctx, NOTIFICATION_ALARM_DLG, NULL);
        }
    }
    refresh();
}
