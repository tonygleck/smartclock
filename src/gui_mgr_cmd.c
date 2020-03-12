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

#define CLOCK_HOUR_12           0x00000001

#define ASCII_ZERO_VALUE        48

#define TIME_START_POS_X        0
#define TIME_START_POS_Y        0

#define DATE_LINE_POS_X         1
#define DATE_LINE_POS_Y         0

#define ALARM_LINE_POS_X        2
#define ALARM_LINE_POS_Y        0

#define FORCAST_POS_X           0
#define FORCAST_POS_Y           20

#define SNOOZE_ALARM_KEY        's'
#define KILL_ALARM_KEY          'q'

typedef struct GUI_MGR_INFO_TAG
{
    int option;
    ALARM_RESULT_CB alarm_result_cb;
    void* user_ctx;
    ALARM_TRIGGERED_RESULT alarm_state;
} GUI_MGR_INFO;

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

static void write_line(int x_pos, int y_pos, const char* write_line)
{
    size_t date_len = strlen(write_line);
    for (size_t index = 0; index < date_len; index++)
    {
        mvwaddch(stdscr, x_pos, y_pos++, write_line[index]);
    }
}

GUI_MGR_HANDLE gui_mgr_create(void)
{
    GUI_MGR_INFO* result;
    if ((result = (GUI_MGR_INFO*)malloc(sizeof(GUI_MGR_INFO))) == NULL)
    {
        log_error("Failure allocating gui manager");
    }
    else
    {
        memset(result, 0, sizeof(GUI_MGR_INFO));
        result->option |= CLOCK_HOUR_12;

        initscr();
        noecho();
        nodelay(stdscr, TRUE);
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

int gui_create_win(GUI_MGR_HANDLE handle)
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
        uint8_t adjusted_hour;

        if (handle->option & CLOCK_HOUR_12)
        {
            adjusted_hour = (uint8_t)(curr_time->tm_hour > 12 ? curr_time->tm_hour - 12 : curr_time->tm_hour);
        }

        char time_char = ' ';
        for (size_t index = 0; index < 5; index++)
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
            }
            mvwaddch(stdscr, time_y, time_x++, time_char);
        }

        char date_line[128];
        sprintf(date_line, "%s %d, %d", get_month_name(curr_time->tm_mon), curr_time->tm_mday, curr_time->tm_year+1900);
        write_line(DATE_LINE_POS_X, DATE_LINE_POS_Y, date_line);
    }
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
                    handle->alarm_result_cb(handle->user_ctx, ALARM_TRIGGERED_SNOOZE);
                    handle->alarm_state = ALARM_TRIGGERED_SNOOZE;
                    break;
                case KILL_ALARM_KEY:
                    handle->alarm_result_cb(handle->user_ctx, ALARM_TRIGGERED_STOPPED);
                    handle->alarm_state = ALARM_TRIGGERED_STOPPED;
                    break;
            }
        }
        else if (handle->alarm_state == ALARM_TRIGGERED_SNOOZE)
        {
            if (response_ch == KILL_ALARM_KEY)
            {
                handle->alarm_result_cb(handle->user_ctx, ALARM_TRIGGERED_STOPPED);
                handle->alarm_state = ALARM_TRIGGERED_STOPPED;
            }
        }
    }
    refresh();
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
        sprintf(forcast_line, "%f f", weather_cond->temperature);
        write_line(FORCAST_POS_X, FORCAST_POS_Y, forcast_line);
    }
}

int gui_mgr_set_alarm_triggered(GUI_MGR_HANDLE handle, const ALARM_INFO* alarm_triggered, ALARM_RESULT_CB result_cb, void* user_ctx)
{
    int result;
    if (handle == NULL || alarm_triggered == NULL || result_cb == NULL)
    {
        log_error("Invalid variable specified handle: %p, alarm_triggered: %p, result_cb: %p", handle, alarm_triggered, result_cb);
        result = __LINE__;
    }
    else
    {
        char alarm_line[128];
        sprintf(alarm_line, "Alarm Alert: %s", alarm_triggered->alarm_text);
        write_line(ALARM_LINE_POS_X, ALARM_LINE_POS_Y, alarm_line);
        handle->alarm_state = ALARM_TRIGGERED_TRIGGERED;

        // Store the alarm callback
        handle->alarm_result_cb = result_cb;
        handle->user_ctx = user_ctx;
        result = 0;
    }
    return result;
}