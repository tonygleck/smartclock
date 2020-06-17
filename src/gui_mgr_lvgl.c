// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"

// lvgl includes
#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"
#include <SDL2/SDL.h>
#include "lvgl/lvgl.h"

#include "time_mgr.h"
#include "config_mgr.h"
#include "alarm_scheduler.h"
#include "gui_mgr.h"

#define SHOW_SEC_VALUE          5
#define SHOW_MIN_VALUE          8
#define RESOLUTION_TIME         5

#define TOP_MARGIN              20
#define LEFT_MARGIN             20
#define VALUE_BUFFER_SIZE       10

#define CLOCK_IMG_WIDTH         128
#define CLOCK_IMG_HEIGHT        256
#define ELLIPSE_IMG_WIDTH       27
#define FORCAST_IMG_DIMENSIONS  65
#define DEFAULT_SNOOZE_TIME     10
#define MAX_CLOCK_FACE_THEMES   4

static const char* g_alarm_buttons[] = {"Snooze", "Dismiss", ""};

static lv_color_t g_image_colors[] =
{
    LV_COLOR_MAKE(0xFF, 0x33, 0x33),
    LV_COLOR_MAKE(0x33, 0xD5, 0xE5),
    LV_COLOR_MAKE(0xFF, 0xFF, 0xFF),
    LV_COLOR_MAKE(0x3B, 0xB0, 0x3B)
};

// Declare the font
LV_FONT_DECLARE(arial_20)
LV_IMG_DECLARE(zero_img)
LV_IMG_DECLARE(one_img)
LV_IMG_DECLARE(two_img)
LV_IMG_DECLARE(three_img)
LV_IMG_DECLARE(four_img)
LV_IMG_DECLARE(five_img)
LV_IMG_DECLARE(six_img)
LV_IMG_DECLARE(seven_img)
LV_IMG_DECLARE(eight_img)
LV_IMG_DECLARE(nine_img)

LV_IMG_DECLARE(ellipse_img)
LV_IMG_DECLARE(blank_img)

LV_IMG_DECLARE(alarm_img)

LV_IMG_DECLARE(cloudy_img)
LV_IMG_DECLARE(light_rain_img)
LV_IMG_DECLARE(partly_sunny_img)
LV_IMG_DECLARE(rain_showers_img)
LV_IMG_DECLARE(sunny_img)

typedef enum CLOCK_FACE_THEME_TAG
{
    THEME_FIRE_ENGINE_RED,
    THEME_COOL_OCEAN_BLUE,
    THEME_BLACK_AND_WHITE,
    THEME_MOSS_GREEN
} CLOCK_FACE_THEME;

typedef enum IMAGE_TYPE_TAG
{
    IMAGE_HOUR_1,
    IMAGE_HOUR_2,
    IMAGE_ELLIPSIS,
    IMAGE_MIN_1,
    IMAGE_MIN_2,
    IMAGE_FORCAST,
    IMAGE_COUNT
} IMAGE_TYPE;

typedef enum DAY_NAME_TYPE_TAG
{
    DAY_NAME_FULLNAME,
    DAY_NAME_ABBREV,
    DAY_NAME_LETTER
} DAY_NAME_TYPE;

typedef enum WINDOW_MODE_TAG
{
    WINDOW_MODE_CLOCK,
    WINDOW_MODE_SHOW_OPTIONS,
    WINDOW_MODE_CLOSE_OPTIONS,
    WINDOW_MODE_OPTIONS
} WINDOW_MODE;

typedef struct TIME_CONTROL_TAG
{
    lv_obj_t* hour_roller;
    lv_obj_t* min_roller;
    lv_obj_t* period_roller;
} TIME_CONTROL;

typedef struct NEW_ALARM_DLG_TAG
{
    SCHEDULER_HANDLE sched_handle;
    TIME_CONTROL alarm_time;
    TIME_CONTROL start_shade;
    TIME_CONTROL end_shade;
    lv_obj_t* ringtone_roller;
    lv_obj_t* alarm_text;
    lv_obj_t* alert_day_cb[7];
    lv_obj_t* table;
    lv_obj_t* zipcode_text;
    lv_obj_t* ta_keybrd;
    lv_obj_t* parent;

    const char* orig_zipcode;
    bool is_dirty;
} NEW_ALARM_DLG;

typedef struct GUI_MGR_INFO_TAG
{
    // Main Window
    lv_obj_t* date_label;
    lv_obj_t* alarm_label;

    lv_obj_t* image_items[IMAGE_COUNT];

    lv_obj_t* forcast_date_label;
    lv_obj_t* curr_temp_label;
    lv_obj_t* forcast_desc_label;
    lv_obj_t* forcast_temp_label;

    lv_obj_t* alarm_dlg_label;
    lv_obj_t* alarm_option_img;
    //lv_obj_t* tomorrow_temp_label;
    //lv_obj_t* tomorrow_temp_img;

    lv_color_t digit_color;
    lv_opa_t intense;
    lv_style_t img_style;
    lv_style_t main_win_style;
    lv_theme_t* main_win_theme;

    lv_obj_t* alarm_box;
    lv_obj_t* win_bkgrd;

    int last_min;
    GUI_MGR_NOTIFICATION_CB notify_cb;
    void* user_ctx;
    ALARM_STATE_RESULT alarm_state;
    WINDOW_MODE window_mode;
    CONFIG_MGR_HANDLE config_mgr;

    NEW_ALARM_DLG new_alarm_dlg;
} GUI_MGR_INFO;

static void setup_win_style(GUI_MGR_INFO* gui_info)
{
    /*lv_theme_material_init(lv_theme_get_color_primary(), lv_theme_get_color_secondary(),
                LV_THEME_MATERIAL_FLAG_DARK,
                lv_theme_get_font_small(), lv_theme_get_font_normal(), lv_theme_get_font_subtitle(), lv_theme_get_font_title() );*/
    lv_color_t bg_color = LV_COLOR_BLACK;

    uint32_t digit_color_idx = config_mgr_get_digit_color(gui_info->config_mgr);
    gui_info->digit_color = g_image_colors[digit_color_idx];
    gui_info->intense = 225;

    lv_style_init(&gui_info->main_win_style);

    lv_style_set_radius(&gui_info->main_win_style, LV_STATE_DEFAULT, 10);
    lv_style_set_bg_opa(&gui_info->main_win_style, LV_STATE_DEFAULT, LV_OPA_COVER);
    /*lv_style_set_bg_color(&style_btn, LV_STATE_DEFAULT, LV_DEMO_PRINTER_BLUE);
    lv_style_set_bg_color(&style_btn, LV_STATE_PRESSED, lv_color_darken(LV_DEMO_PRINTER_BLUE, LV_OPA_20));
    lv_style_set_text_color(&style_btn, LV_STATE_DEFAULT, LV_DEMO_PRINTER_WHITE);
    lv_style_set_value_color(&style_btn, LV_STATE_DEFAULT, LV_DEMO_PRINTER_WHITE);
    lv_style_set_pad_top(&style_btn, LV_STATE_DEFAULT, LV_VER_RES / 40);
    lv_style_set_pad_bottom(&style_btn, LV_STATE_DEFAULT, LV_VER_RES / 40);*/

    lv_style_set_bg_color(&gui_info->main_win_style, LV_STYLE_BG_COLOR, bg_color);
    lv_style_set_border_color(&gui_info->main_win_style, LV_STYLE_BORDER_COLOR, bg_color);
    //lv_style_set_text_color(&gui_info->main_win_style, LV_STYLE_TEXT_COLOR, LV_COLOR_BLACK);
}

static void set_image_color(GUI_MGR_INFO* gui_info)
{
    for (size_t index = 0; index < IMAGE_COUNT; index++)
    {
        lv_obj_set_style_local_image_recolor_opa(gui_info->image_items[index], LV_IMG_PART_MAIN, LV_STATE_DEFAULT, gui_info->intense);
        lv_obj_set_style_local_image_recolor(gui_info->image_items[index], LV_IMG_PART_MAIN, LV_STATE_DEFAULT, gui_info->digit_color);
    }
}

static DayOfTheWeek get_trigger_day_value(size_t index)
{
    DayOfTheWeek result;
    switch (index)
    {
        case 0:
            result = Sunday;
            break;
        case 1:
            result = Monday;
            break;
        case 2:
            result = Tuesday;
            break;
        case 3:
            result = Wednesday;
            break;
        case 4:
            result = Thursday;
            break;
        case 5:
            result = Friday;
            break;
        case 6:
            result = Saturday;
            break;
        default:
            result = NoDay;
            break;
    }
    return result;
}

static const char* get_day_name(uint8_t day, DAY_NAME_TYPE type)
{
    const char* result;
    switch (day)
    {
        case 0:
            result = (type == DAY_NAME_FULLNAME) ? "Sunday" : (type == DAY_NAME_ABBREV) ? "Sun" : "Su";
            break;
        case 1:
            result = (type == DAY_NAME_FULLNAME) ? "Monday" : (type == DAY_NAME_ABBREV) ? "Mon" : "M";
            break;
        case 2:
            result = (type == DAY_NAME_FULLNAME) ? "Tuesday" : (type == DAY_NAME_ABBREV) ? "Tue" : "T";
            break;
        case 3:
            result = (type == DAY_NAME_FULLNAME) ? "Wednesday" : (type == DAY_NAME_ABBREV) ? "Wed" : "W";
            break;
        case 4:
            result = (type == DAY_NAME_FULLNAME) ? "Thursday" : (type == DAY_NAME_ABBREV) ? "Thu" : "Th";
            break;
        case 5:
            result = (type == DAY_NAME_FULLNAME) ? "Friday" : (type == DAY_NAME_ABBREV) ? "Fri" : "F";
            break;
        case 6:
            result = (type == DAY_NAME_FULLNAME) ? "Saturday" : (type == DAY_NAME_ABBREV) ? "Sat" : "S";
            break;
        default:
            result = "";
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
        default:
            result = "";
            break;
    }
    return result;
}

static void create_time_roller(lv_obj_t* parent, lv_coord_t* x_pos, lv_coord_t* y_pos, TIME_CONTROL* time_ctrl, const struct tm* curr_time)
{
    time_ctrl->hour_roller = lv_roller_create(parent, NULL);
    lv_roller_set_options(time_ctrl->hour_roller,
                    "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12", LV_ROLLER_MODE_INIFINITE);
    lv_obj_set_pos(time_ctrl->hour_roller, *x_pos, *y_pos);
    lv_obj_set_width(time_ctrl->hour_roller, 20);
    lv_roller_set_selected(time_ctrl->hour_roller, curr_time->tm_hour > 12 ? curr_time->tm_hour - 11 : curr_time->tm_hour - 1, LV_ANIM_OFF);

    time_ctrl->min_roller = lv_roller_create(parent, NULL);
    lv_roller_set_options(time_ctrl->min_roller,
                    "00\n1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n"
                    "11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n"
                    "21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n"
                    "31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n"
                    "41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n"
                    "51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_INIFINITE);
    lv_roller_set_selected(time_ctrl->min_roller, curr_time->tm_min, LV_ANIM_OFF);
    lv_obj_set_width(time_ctrl->min_roller, 20);
    lv_obj_align(time_ctrl->min_roller, time_ctrl->hour_roller, LV_ALIGN_IN_TOP_RIGHT, 75, 0);

    time_ctrl->period_roller = lv_roller_create(parent, NULL);
    lv_roller_set_options(time_ctrl->period_roller,
                    "am\npm", LV_ROLLER_MODE_NORMAL);
    //lv_roller_set_visible_row_count(time_ctrl->period_roller, 2);
    lv_obj_set_width(time_ctrl->period_roller, 20);
    lv_roller_set_selected(time_ctrl->period_roller, curr_time->tm_hour > 12 ? 1 : 0, LV_ANIM_OFF);
    lv_obj_align(time_ctrl->period_roller, time_ctrl->min_roller, LV_ALIGN_IN_TOP_RIGHT, 85, 0);

    lv_coord_t coord = lv_obj_get_width(time_ctrl->period_roller);
    *x_pos += lv_obj_get_x(time_ctrl->period_roller)+coord;
    coord = lv_obj_get_height(time_ctrl->period_roller);
    *y_pos += lv_obj_get_y(time_ctrl->period_roller)+coord;
}

static void add_item_to_table(lv_obj_t* table, uint16_t row, const char* alarm_text, const TIME_INFO* trigger_time, uint32_t trigger_days)
{
    char table_value[64];

    uint16_t row_cnt = lv_table_get_row_cnt(table);
    lv_table_set_row_cnt(table, row_cnt + 1);

    lv_table_set_cell_value(table, row, 0, alarm_text);
    sprintf(table_value, "%02d:%02d %s", trigger_time->hour > 12 ? trigger_time->hour - 12 : trigger_time->hour,
        trigger_time->min, alarm_scheduler_is_morning(trigger_time) ? "am" : "pm");
    lv_table_set_cell_value(table, row, 1, table_value);

    if (trigger_days != NoDay)
    {
        if (trigger_days == Everyday)
        {
            strcpy(table_value, "M T W Th F S Su");
        }
        else
        {
            memset(table_value, 0, sizeof(char)*64);
            char* iterator = table_value;
            bool day_entered = false;
            for (size_t inner = 0; inner < 7; inner++)
            {
                if (trigger_days & get_trigger_day_value(inner))
                {
                    if (day_entered)
                    {
                        memcpy(iterator, ", ", 2);
                        iterator += 2;
                    }
                    const char* day = get_day_name(inner, DAY_NAME_LETTER);
                    size_t len = strlen(day);
                    memcpy(iterator, day, len);
                    iterator += len;
                    day_entered = true;
                }
            }
        }
        lv_table_set_cell_value(table, row, 2, table_value);
    }

}

// Callbacks
static void save_option_callback(lv_obj_t* save_btn, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(save_btn);
        if (gui_info != NULL)
        {
            uint32_t trigger_days = NoDay;
            for (size_t index = 0; index < 7; index++)
            {
                if (lv_checkbox_is_checked(gui_info->new_alarm_dlg.alert_day_cb[index]))
                {
                    switch (index)
                    {
                        case 0:
                            trigger_days |= Monday;
                            break;
                        case 1:
                            trigger_days |= Tuesday;
                            break;
                        case 2:
                            trigger_days |= Wednesday;
                            break;
                        case 3:
                            trigger_days |= Thursday;
                            break;
                        case 4:
                            trigger_days |= Friday;
                            break;
                        case 5:
                            trigger_days |= Saturday;
                            break;
                        case 6:
                            trigger_days |= Sunday;
                            break;
                    }
                }
            }
            TIME_VALUE_STORAGE tm_value = {0};
            TIME_INFO alarm_time = {0};
            char time_value[8];
            char ringtone[32];
            const char* alarm_text_value;

            lv_roller_get_selected_str(gui_info->new_alarm_dlg.alarm_time.hour_roller, time_value, 8);
            tm_value.hours = alarm_time.hour = (uint8_t)atol(time_value);

            lv_roller_get_selected_str(gui_info->new_alarm_dlg.alarm_time.min_roller, time_value, 8);
            tm_value.minutes = alarm_time.min = (uint8_t)atol(time_value);

            lv_roller_get_selected_str(gui_info->new_alarm_dlg.ringtone_roller, ringtone, 32);

            alarm_text_value = lv_textarea_get_text(gui_info->new_alarm_dlg.alarm_text);
            if (alarm_text_value == NULL || strlen(alarm_text_value) == 0)
            {
                alarm_text_value = "New alarm text";
            }

            if (alarm_scheduler_add_alarm(gui_info->new_alarm_dlg.sched_handle, alarm_text_value,
                &alarm_time, trigger_days, ringtone, DEFAULT_SNOOZE_TIME) != 0)
            {
                log_error("Failure saving alarm");
            }
            else if (config_mgr_store_alarm(gui_info->config_mgr, alarm_text_value, &tm_value, ringtone, trigger_days, DEFAULT_SNOOZE_TIME) != 0)
            {
                log_error("Failure saving alarm to config");
            }
            else
            {
                uint16_t row_cnt = lv_table_get_row_cnt(gui_info->new_alarm_dlg.table);
                lv_table_set_row_cnt(gui_info->new_alarm_dlg.table, row_cnt+1);
                add_item_to_table(gui_info->new_alarm_dlg.table, row_cnt+1, alarm_text_value, &alarm_time, trigger_days);

                // Clear the items
                lv_textarea_set_text(gui_info->new_alarm_dlg.alarm_text, "");
                for (size_t index = 0; index < 7; index++)
                {
                    lv_checkbox_set_checked(gui_info->new_alarm_dlg.alert_day_cb[index], false);
                }
                struct tm* curr_time = get_time_value();
                lv_roller_set_selected(gui_info->new_alarm_dlg.alarm_time.hour_roller, curr_time->tm_hour > 12 ? curr_time->tm_hour - 11 : curr_time->tm_hour - 1, LV_ANIM_OFF);
                lv_roller_set_selected(gui_info->new_alarm_dlg.alarm_time.min_roller, curr_time->tm_min, LV_ANIM_OFF);
                lv_roller_set_selected(gui_info->new_alarm_dlg.alarm_time.period_roller, curr_time->tm_hour > 12 ? 1 : 0, LV_ANIM_OFF);

                gui_info->new_alarm_dlg.is_dirty = true;
            }
        }
        else
        {
            log_error("Failure save box - User data is NULL");
        }
    }
}

static void alarm_dlg_day_select(lv_obj_t * obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        log_debug("Alarm Clicked: %s\n", lv_list_get_btn_text(obj));
    }
}

static void digit_color_cb(lv_obj_t* digital_roll, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        char buf[32];
        lv_roller_get_selected_str(digital_roll, buf, sizeof(buf));
        uint32_t index_sel = (uint32_t)lv_roller_get_selected(digital_roll);

        GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(digital_roll);
        if (gui_info != NULL)
        {
            if (index_sel >= MAX_CLOCK_FACE_THEMES)
            {
                log_error("Invalid index selection for clock face theme");
            }
            else
            {
                gui_info->digit_color = g_image_colors[index_sel];
                set_image_color(gui_info);

                if (config_mgr_get_digit_color(gui_info->config_mgr) != index_sel)
                {
                    config_mgr_set_digit_color(gui_info->config_mgr, index_sel);
                    gui_info->new_alarm_dlg.is_dirty = true;
                }
            }
        }
    }
}

static void alarm_msgbox_callback(lv_obj_t* alarm_box_obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED) {
        GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(alarm_box_obj);
        if (gui_info != NULL)
        {
            const char* active_btn = lv_msgbox_get_active_btn_text(alarm_box_obj);
            printf("Button: %s\n", active_btn);
            ALARM_STATE_RESULT alarm_res;
            if (strcmp(active_btn, g_alarm_buttons[0]) == 0)
            {
                alarm_res = ALARM_STATE_SNOOZE;
                gui_info->notify_cb(gui_info->user_ctx, NOTIFICATION_ALARM_RESULT, &alarm_res);
            }
            else
            {
                alarm_res = ALARM_STATE_STOPPED;
                gui_info->notify_cb(gui_info->user_ctx, NOTIFICATION_ALARM_RESULT, &alarm_res);
            }
        }
        else
        {
            log_error("Failure alarm message box - User data is NULL");
        }
        lv_obj_del_async(lv_obj_get_parent(alarm_box_obj));
    }
}

static void alarm_dlg_btn_cb(lv_obj_t* close_btn, lv_event_t event)
{
    if (event == LV_EVENT_RELEASED) {

        lv_obj_t* alarm_option_win = lv_win_get_from_btn(close_btn);
        GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(close_btn);
        if (gui_info != NULL)
        {
            // Check to see if zipcode has changed
            const char* new_zipcode = lv_textarea_get_text(gui_info->new_alarm_dlg.zipcode_text);
            if (new_zipcode != NULL)
            {
                const char* curr_zipcode = config_mgr_get_zipcode(gui_info->config_mgr);
                if (strcmp(new_zipcode, curr_zipcode) != 0)
                {
                    config_mgr_set_zipcode(gui_info->config_mgr, new_zipcode);
                }
            }
            gui_info->window_mode = WINDOW_MODE_CLOSE_OPTIONS;
        }
        lv_obj_del(alarm_option_win);
    }
}

static void option_btn_callback(lv_obj_t* alarm_dlg_obj, lv_event_t event)
{
    if (event == LV_EVENT_PRESSED)
    {
        GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(alarm_dlg_obj);
        gui_info->window_mode = WINDOW_MODE_SHOW_OPTIONS;
    }
}

static void create_alarm_window(GUI_MGR_INFO* gui_info, lv_obj_t* parent)
{
    size_t alarm_count = alarm_scheduler_get_alarm_count(gui_info->new_alarm_dlg.sched_handle);

    // Create a normal cell style
    //static lv_style_t style_cell1;
    //lv_style_copy(&style_cell1, &lv_style_plain);
    //style_cell1.body.border.width = 1;
    //style_cell1.body.border.color = LV_COLOR_BLACK;

    // Create a header cell style
    //static lv_style_t style_cell2;
    //lv_style_copy(&style_cell2, &lv_style_plain);
    //style_cell2.body.border.width = 1;
    //style_cell2.body.border.color = LV_COLOR_BLACK;
    //style_cell2.body.main_color = LV_COLOR_SILVER;
    //style_cell2.body.grad_color = LV_COLOR_SILVER;

    lv_coord_t win_width = lv_obj_get_width_fit(parent);
    lv_coord_t win_height = lv_obj_get_height_fit(parent);

    gui_info->new_alarm_dlg.table = lv_table_create(parent, NULL);
    lv_table_set_col_cnt(gui_info->new_alarm_dlg.table, 3);
    lv_table_set_row_cnt(gui_info->new_alarm_dlg.table, alarm_count + 1);
    lv_obj_set_pos(gui_info->new_alarm_dlg.table, 5, 5);
    //lv_obj_align(gui_info->new_alarm_dlg.table, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_coord_t tbl_width = lv_obj_get_width(gui_info->new_alarm_dlg.table);

    //lv_obj_set_size(gui_info->new_alarm_dlg.table, win_width, win_height);

    uint16_t col_width = (win_width - (5*10))/3;
    lv_table_set_col_width(gui_info->new_alarm_dlg.table, 0, col_width);
    lv_table_set_col_width(gui_info->new_alarm_dlg.table, 1, col_width);
    lv_table_set_col_width(gui_info->new_alarm_dlg.table, 2, col_width);

    // Make the cells of the first row center aligned
    lv_table_set_cell_align(gui_info->new_alarm_dlg.table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(gui_info->new_alarm_dlg.table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(gui_info->new_alarm_dlg.table, 0, 2, LV_LABEL_ALIGN_CENTER);

    // Make the cells of the first row TYPE = 2 (use `style_cell2`)
    lv_table_set_cell_type(gui_info->new_alarm_dlg.table, 0, 0, 1);
    lv_table_set_cell_type(gui_info->new_alarm_dlg.table, 0, 1, 2);
    lv_table_set_cell_type(gui_info->new_alarm_dlg.table, 0, 2, 2);

    // Fill the header column
    lv_table_set_cell_value(gui_info->new_alarm_dlg.table, 0, 0, "Alarm Name");
    lv_table_set_cell_value(gui_info->new_alarm_dlg.table, 0, 1, "Time");
    lv_table_set_cell_value(gui_info->new_alarm_dlg.table, 0, 2, "Days");

    for (size_t index = 0; index < alarm_count; index++)
    {
        const ALARM_INFO* alarm_info = alarm_scheduler_get_alarm(gui_info->new_alarm_dlg.sched_handle, index);
        if (alarm_info != NULL)
        {
            add_item_to_table(gui_info->new_alarm_dlg.table, index+1, alarm_info->alarm_text, &alarm_info->trigger_time, alarm_info->trigger_days);
        }
    }
    lv_table_set_cell_type(gui_info->new_alarm_dlg.table, 1, 1, 3);
}

static void alarm_text_cb(lv_obj_t* txt_area, lv_event_t event)
{
    GUI_MGR_INFO* gui_info = (GUI_MGR_INFO*)lv_obj_get_user_data(txt_area);
    if (gui_info != NULL)
    {
        if (gui_info->new_alarm_dlg.ta_keybrd == NULL)
        {
            gui_info->new_alarm_dlg.ta_keybrd = lv_keyboard_create(gui_info->new_alarm_dlg.parent, NULL);
            //lv_keyboard_set_cursor_manage(keybrd, true);
        }

        if (event == LV_EVENT_FOCUSED)
        {
            // Set text area with keyboard
            lv_keyboard_set_textarea(gui_info->new_alarm_dlg.ta_keybrd, txt_area);
            lv_textarea_set_cursor_hidden(txt_area, false);
        }
        else if (event == LV_EVENT_DEFOCUSED)
        {
            // Need a way to
            lv_textarea_set_cursor_hidden(txt_area, true);
        }
    }
}

static void create_new_alarm_window(GUI_MGR_INFO* gui_info, lv_obj_t* parent)
{
    lv_coord_t win_width = lv_obj_get_width_fit(parent);
    lv_coord_t win_height = lv_obj_get_height_fit(parent);

    gui_info->new_alarm_dlg.parent = parent;

    lv_coord_t left = 5;
    lv_coord_t top = 5;
    struct tm* curr_time = get_time_value();
    create_time_roller(parent, &left, &top, &gui_info->new_alarm_dlg.alarm_time, curr_time);
    left += 10;
    top += 10;

    lv_obj_t* save_btn = lv_btn_create(parent, NULL);
    lv_obj_align(save_btn, NULL, LV_ALIGN_IN_TOP_RIGHT, -20, 20);
    lv_obj_t* label = lv_label_create(save_btn, NULL);
    lv_label_set_text(label, LV_SYMBOL_SAVE " Save");
    lv_obj_set_event_cb(save_btn, save_option_callback);
    lv_obj_set_user_data(save_btn, gui_info);

    gui_info->new_alarm_dlg.alarm_text = lv_textarea_create(parent, NULL);
    lv_textarea_set_one_line(gui_info->new_alarm_dlg.alarm_text, true);
    lv_textarea_set_placeholder_text(gui_info->new_alarm_dlg.alarm_text, "Enter Alarm Text");
    lv_textarea_set_text(gui_info->new_alarm_dlg.alarm_text, "");
    lv_textarea_set_text_sel(gui_info->new_alarm_dlg.alarm_text, true);
    lv_textarea_set_max_length(gui_info->new_alarm_dlg.alarm_text, 32);
    lv_obj_set_event_cb(gui_info->new_alarm_dlg.alarm_text, alarm_text_cb);
    lv_obj_set_user_data(gui_info->new_alarm_dlg.alarm_text, gui_info);
    lv_obj_set_width(gui_info->new_alarm_dlg.alarm_text, 300);
    lv_obj_set_pos(gui_info->new_alarm_dlg.alarm_text, left, 20);
    //lv_obj_align(gui_info->new_alarm_dlg.alarm_text, NULL, LV_ALIGN_IN_TOP_MID, left, 0);
    // lv_obj_t* keybrd = lv_keyboard_create(parent, NULL);
    // lv_keyboard_set_cursor_manage(keybrd, true);
    // // Set text area with keyboard
    // lv_keyboard_set_textarea(keybrd, gui_info->new_alarm_dlg.alarm_text);

    // Create a container
    lv_obj_t* cont = lv_cont_create(parent, NULL);
    lv_obj_set_pos(cont, 20, top);
    lv_obj_set_size(cont, 400, 100);

    // Days of the Week;
    uint8_t day;
    size_t offset = 5;
    size_t ck_width = 100;
    size_t ck_height_pos = 0;
    size_t multiplier = 0;
    for (size_t index = 0; index < 7; index++, multiplier++)
    {
        day = index+1;
        gui_info->new_alarm_dlg.alert_day_cb[index] = lv_checkbox_create(cont, NULL);
        if (index == 6)
        {
            day = 0;
        }
        else if (index == 3)
        {
            // Add the second row
            ck_height_pos = 50;
            multiplier = 0;
        }
        lv_checkbox_set_text(gui_info->new_alarm_dlg.alert_day_cb[index], get_day_name(day, DAY_NAME_ABBREV));
        lv_obj_align(gui_info->new_alarm_dlg.alert_day_cb[index], NULL, LV_ALIGN_IN_TOP_LEFT, (ck_width*multiplier)+offset, ck_height_pos+offset);
    }

    gui_info->new_alarm_dlg.ringtone_roller = lv_roller_create(parent, NULL);
    lv_roller_set_options(gui_info->new_alarm_dlg.ringtone_roller,
                    "chimes\nheavy Metal", LV_ROLLER_MODE_INIFINITE);
    lv_roller_set_visible_row_count(gui_info->new_alarm_dlg.ringtone_roller, 2);
    lv_obj_set_pos(gui_info->new_alarm_dlg.ringtone_roller, 450, top);
    //lv_obj_align(save_btn, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
}

static void create_options_window(GUI_MGR_INFO* gui_info, lv_obj_t* parent)
{
    lv_obj_t* digit_color = lv_roller_create(parent, NULL);
    lv_roller_set_options(digit_color,
                    "Fire Engine Red\n"
                    "Cool Ocean Blue\n"
                    "Black & White\n"
                    "Moss Green", LV_ROLLER_MODE_INIFINITE);
    lv_roller_set_visible_row_count(digit_color, 2);
    //lv_obj_align(digit_color, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_set_event_cb(digit_color, digit_color_cb);
    lv_obj_set_user_data(digit_color, gui_info);
    lv_obj_set_pos(digit_color, LEFT_MARGIN, TOP_MARGIN);
    uint32_t color_idx = config_mgr_get_digit_color(gui_info->config_mgr);
    lv_roller_set_selected(digit_color, color_idx, LV_ANIM_OFF);

    lv_obj_t* img1 = lv_img_create(parent, NULL);
    lv_img_set_src(img1, &alarm_img);
    lv_obj_align(img1, digit_color, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

    const char* zipcode = config_mgr_get_zipcode(gui_info->config_mgr);

    gui_info->new_alarm_dlg.zipcode_text = lv_textarea_create(parent, NULL);
    lv_textarea_set_one_line(gui_info->new_alarm_dlg.zipcode_text, true);
    lv_textarea_set_text(gui_info->new_alarm_dlg.zipcode_text, zipcode);
    lv_obj_set_pos(gui_info->new_alarm_dlg.zipcode_text, LEFT_MARGIN+400, TOP_MARGIN);
    //lv_obj_align(gui_info->new_alarm_dlg.zipcode_text, NULL, LV_ALIGN_IN_TOP_MID, 0, TOP_MARGIN);

    lv_obj_t* num_keybrd = lv_keyboard_create(parent, NULL);
    lv_keyboard_set_cursor_manage(num_keybrd, true);
    // Set text area with keyboard
    lv_keyboard_set_textarea(num_keybrd, gui_info->new_alarm_dlg.zipcode_text);
    lv_keyboard_set_mode(num_keybrd, LV_KEYBOARD_MODE_NUM);

    /*struct tm shade_time = {0};
    shade_time.tm_hour = 22;
    shade_time.tm_min = 30;
    create_time_roller(parent, LEFT_MARGIN+600, TOP_MARGIN, &gui_info->new_alarm_dlg.start_shade, &shade_time);
    shade_time.tm_hour = 7;
    shade_time.tm_min = 00;
    create_time_roller(parent, LEFT_MARGIN+600, TOP_MARGIN+150, &gui_info->new_alarm_dlg.end_shade, &shade_time);*/
}

static int tick_thread(void * data)
{
    (void)data;
    while(1)
    {
        SDL_Delay(RESOLUTION_TIME);   // Sleep for RESOLUTION_TIME millisecond
        lv_tick_inc(RESOLUTION_TIME); // Tell LittelvGL that RESOLUTION_TIME milliseconds were elapsed
    }
    return 0;
}

static void hal_init(void)
{
    monitor_init();

    // Create a display buffer
    static lv_disp_buf_t disp_buf1;
    static lv_color_t buf1_1[480*10];
    lv_disp_buf_init(&disp_buf1, buf1_1, NULL, 480*10);

    // Create a display
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer = &disp_buf1;
    disp_drv.flush_cb = monitor_flush;    // Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)
    lv_disp_drv_register(&disp_drv);

    // Add the mouse as input device
    // Use the 'mouse' driver which reads the PC's mouse
    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;         // This function will be called periodically (by the library) to get the mouse position and state
    /*lv_indev_t* mouse_indev = */(void)lv_indev_drv_register(&indev_drv);

    // Tick init.7
    // You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about how much time were elapsed
    // Create an SDL thread to do this
    SDL_CreateThread(tick_thread, "tick", NULL);

    // Optional:
    // Create a memory monitor task which prints the memory usage in periodically.
    //lv_task_create(memory_monitor, 3000, LV_TASK_PRIO_MID, NULL);
}

static const lv_img_dsc_t* retrieve_numeral_image(int digit)
{
    const lv_img_dsc_t* result;
    switch (digit)
    {
        case 0:
            result = &zero_img;
            break;
        case 1:
            result = &one_img;
            break;
        case 2:
            result = &two_img;
            break;
        case 3:
            result = &three_img;
            break;
        case 4:
            result = &four_img;
            break;
        case 5:
            result = &five_img;
            break;
        case 6:
            result = &six_img;
            break;
        case 7:
            result = &seven_img;
            break;
        case 8:
            result = &eight_img;
            break;
        case 9:
            result = &nine_img;
            break;
        default:
            result = NULL;
            break;
    }
    return result;
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
        result = NULL;
    }
    else
    {
        memset(result, 0, sizeof(GUI_MGR_INFO));

        lv_init();
        hal_init();

        result->config_mgr = config_mgr;
        result->last_min = 70;
        result->notify_cb = notify_cb;
        result->user_ctx = user_ctx;
        result->window_mode = WINDOW_MODE_CLOCK;
    }
    return result;
}

void gui_mgr_destroy(GUI_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

size_t gui_mgr_get_refresh_resolution(void)
{
    return RESOLUTION_TIME;
}

int gui_mgr_create_win(GUI_MGR_HANDLE handle)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid handle value specified");
        result = __LINE__;
    }
    else
    {
        int16_t x_pos = 0;
        int16_t y_pos = TOP_MARGIN;

        // Create the style for the main window
        setup_win_style(handle);

        /*lv_style_copy(&style1, &lv_style_plain);
        style1.text.font = &arial_20; // Set the base font whcih is concatenated with the others*/

        //handle->main_win_theme = lv_theme_night_init(20, &arial_20);
        //lv_theme_set_current(handle->main_win_theme);

        // Create the style
        /*lv_style_copy(&handle->main_win_style, &lv_style_plain_color);
        handle->main_win_style.body.main_color = LV_COLOR_BLACK;//lv_color_hsv_to_rgb();
        handle->main_win_style.body.grad_color = LV_COLOR_BLACK;
        handle->main_win_style.image.color = LV_COLOR_MAKE(0xFF, 0x33, 0x33);
        handle->main_win_style.image.intense = LV_OPA_80;

        handle->main_win_style.text.color = LV_COLOR_MAKE(0xFF, 0x33, 0x33);
        handle->main_win_style.glass = 0;
        handle->main_win_style.text.font = &lv_font_roboto_28;*/

        //img_style.image.color = lv_color_make(lv_slider_get_value(red_slider), lv_slider_get_value(green_slider), lv_slider_get_value(blue_slider));
        //img_style.image.intense = lv_slider_get_value(intense_slider);

        lv_obj_t* current_scr = lv_disp_get_scr_act(NULL);

        handle->win_bkgrd = lv_obj_create(current_scr, NULL);
        lv_obj_add_style(handle->win_bkgrd, LV_OBJ_PART_MAIN , &handle->main_win_style);
        lv_obj_set_pos(handle->win_bkgrd, 0, 0);
        lv_obj_set_size(handle->win_bkgrd, LV_HOR_RES, LV_VER_RES);

        lv_color_t color = lv_obj_get_style_bg_color(handle->win_bkgrd, LV_OBJ_PART_MAIN);
        //lv_obj_set_opa_scale_enable(win_bkgrd, true); // Enable opacity scaling for the animation */

        // Create an image object
        handle->image_items[IMAGE_HOUR_1] = lv_img_create(handle->win_bkgrd, NULL);
        lv_img_set_src(handle->image_items[IMAGE_HOUR_1], &zero_img);
        lv_obj_set_pos(handle->image_items[IMAGE_HOUR_1], x_pos, y_pos);
        x_pos += CLOCK_IMG_WIDTH + VALUE_BUFFER_SIZE;

        handle->image_items[IMAGE_HOUR_2] = lv_img_create(handle->win_bkgrd, NULL); // Crate an image object
        lv_img_set_src(handle->image_items[IMAGE_HOUR_2], &two_img);
        lv_obj_set_pos(handle->image_items[IMAGE_HOUR_2], x_pos, y_pos);      // Set the positions
        x_pos += CLOCK_IMG_WIDTH + VALUE_BUFFER_SIZE;

        handle->image_items[IMAGE_ELLIPSIS] = lv_img_create(handle->win_bkgrd, NULL); // Crate an image object
        lv_img_set_src(handle->image_items[IMAGE_ELLIPSIS], &ellipse_img);
        lv_obj_set_pos(handle->image_items[IMAGE_ELLIPSIS], x_pos, y_pos);      // Set the positions
        x_pos += ELLIPSE_IMG_WIDTH + VALUE_BUFFER_SIZE;

        int16_t min_x_pos = x_pos;
        handle->image_items[IMAGE_MIN_1] = lv_img_create(handle->win_bkgrd, NULL);  // Crate an image object
        lv_img_set_src(handle->image_items[IMAGE_MIN_1], &three_img);
        lv_obj_set_pos(handle->image_items[IMAGE_MIN_1], x_pos, y_pos);      // Set the positions
        x_pos += CLOCK_IMG_WIDTH + VALUE_BUFFER_SIZE;

        handle->image_items[IMAGE_MIN_2] = lv_img_create(handle->win_bkgrd, NULL); // Crate an image object
        lv_img_set_src(handle->image_items[IMAGE_MIN_2], &four_img);  /*Set the created file as image (a red flower)*/
        lv_obj_set_pos(handle->image_items[IMAGE_MIN_2], x_pos, y_pos);      // Set the positions
        x_pos += CLOCK_IMG_WIDTH + VALUE_BUFFER_SIZE;

        // For Forcast move it a little more on the x
        int16_t temp_x_pos = x_pos + VALUE_BUFFER_SIZE;
        handle->forcast_date_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->forcast_date_label, temp_x_pos, y_pos);
        lv_label_set_text(handle->forcast_date_label, "");
        //lv_label_set_style(handle->forcast_date_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);
        y_pos += (VALUE_BUFFER_SIZE*4);

        handle->image_items[IMAGE_FORCAST] = lv_img_create(handle->win_bkgrd, NULL);
        lv_img_set_src(handle->image_items[IMAGE_FORCAST], &sunny_img);
        lv_obj_set_pos(handle->image_items[IMAGE_FORCAST], temp_x_pos, y_pos);

        handle->curr_temp_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->curr_temp_label, temp_x_pos + FORCAST_IMG_DIMENSIONS + (VALUE_BUFFER_SIZE*2), y_pos + (FORCAST_IMG_DIMENSIONS/2) - VALUE_BUFFER_SIZE);
        lv_label_set_text(handle->curr_temp_label, "00");
        //lv_label_set_style(handle->curr_temp_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);
        y_pos += FORCAST_IMG_DIMENSIONS + (VALUE_BUFFER_SIZE*2);

        handle->forcast_desc_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->forcast_desc_label, temp_x_pos, y_pos);
        lv_label_set_text(handle->forcast_desc_label, "");
        //lv_label_set_style(handle->forcast_desc_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);
        y_pos += VALUE_BUFFER_SIZE*4;

        handle->forcast_temp_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->forcast_temp_label, temp_x_pos, y_pos);
        lv_label_set_text(handle->forcast_temp_label, "0 f/0 f");
        //lv_label_set_style(handle->forcast_temp_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);

        // Reset
        y_pos = CLOCK_IMG_HEIGHT + (VALUE_BUFFER_SIZE*3);
        x_pos = LEFT_MARGIN*2;

        // Create a label and set new text
        handle->date_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->date_label, x_pos, y_pos);
        //lv_label_set_style(handle->date_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);
        x_pos += min_x_pos;

        handle->alarm_label = lv_label_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->alarm_label, x_pos, y_pos);
       // lv_label_set_style(handle->alarm_label, LV_LABEL_STYLE_MAIN, &handle->main_win_style);

        y_pos += (VALUE_BUFFER_SIZE*8);
        x_pos = LEFT_MARGIN*2;

        handle->alarm_dlg_label = lv_btn_create(handle->win_bkgrd, NULL);
        lv_obj_set_pos(handle->alarm_dlg_label, x_pos, y_pos);
        lv_obj_add_style(handle->alarm_dlg_label, LV_BTN_PART_MAIN, &handle->main_win_style);
        lv_obj_set_width(handle->alarm_dlg_label, 200);
        lv_obj_t* label = lv_label_create(handle->alarm_dlg_label, NULL);
        lv_label_set_text(label, "Options");
        lv_obj_set_event_cb(handle->alarm_dlg_label, option_btn_callback);
        lv_obj_set_user_data(handle->alarm_dlg_label, handle);

        /*y_pos += (VALUE_BUFFER_SIZE*3);
        handle->alarm_option_img = lv_img_create(handle->win_bkgrd, NULL);
        lv_img_set_src(handle->alarm_option_img, &alarm_img);
        lv_obj_set_pos(handle->alarm_option_img, x_pos, y_pos+10);
        lv_obj_set_event_cb(handle->alarm_option_img, alarm_dialog_callback);*/

        // handle->today_temp_label = lv_label_create(current_scr, NULL);
        // lv_obj_set_pos(handle->today_temp_label, temp_x_pos, TOP_MARGIN + (CLOCK_IMG_HEIGHT/2) );
        // lv_label_set_style(handle->today_temp_label, LV_LABEL_STYLE_MAIN, &style1);
        // lv_label_set_text(handle->today_temp_label, "Fri 17: 88 F");

        // Create the alarm message box
        // lv_coord_t hres = lv_disp_get_hor_res(NULL);
        // handle->alarm_box = lv_msgbox_create(current_scr, NULL);
        // lv_msgbox_set_text(handle->alarm_box, "Alarm: Alarm Text");
        // lv_obj_set_width(handle->alarm_box, hres - LV_DPI);
        // lv_msgbox_add_btns(handle->alarm_box, g_alarm_buttons);
        // lv_obj_align(handle->alarm_box, current_scr, LV_ALIGN_CENTER, 0, 0);
        // lv_obj_set_event_cb(handle->alarm_box, alarm_msgbox_callback);

        set_image_color(handle);
        result = 0;
    }
    return result;
}

void gui_mgr_set_time_item(GUI_MGR_HANDLE handle, const struct tm* curr_time)
{
    if (handle != NULL && handle->last_min != curr_time->tm_min)
    {
        int digit;
        handle->last_min = curr_time->tm_min;

        uint8_t adjusted_hour = config_mgr_format_hour(handle->config_mgr, curr_time->tm_hour);
        size_t iteration_count = config_mgr_show_seconds(handle->config_mgr) ? SHOW_SEC_VALUE : SHOW_MIN_VALUE;
        for (size_t index = 0; index < iteration_count; index++)
        {
            switch (index)
            {
                case 0:
                    if (adjusted_hour > 19)
                    {
                        lv_img_set_src(handle->image_items[IMAGE_HOUR_1], &two_img);
                    }
                    else if (adjusted_hour > 9)
                    {
                        lv_img_set_src(handle->image_items[IMAGE_HOUR_1], &one_img);
                    }
                    else
                    {
                        lv_img_set_src(handle->image_items[IMAGE_HOUR_1], &blank_img);
                    }
                    break;
                case 1:
                {
                    digit = adjusted_hour - ((adjusted_hour / 10)*10);
                    const lv_img_dsc_t* num_img = retrieve_numeral_image(digit);
                    lv_img_set_src(handle->image_items[IMAGE_HOUR_2], num_img);
                    break;
                }
                case 2:
                    break;
                case 3:
                {
                    digit = curr_time->tm_min / 10;
                    const lv_img_dsc_t* num_img = retrieve_numeral_image(digit);
                    lv_img_set_src(handle->image_items[IMAGE_MIN_1], num_img);
                    break;
                }
                case 4:
                {
                    digit = curr_time->tm_min - ((curr_time->tm_min / 10)*10);
                    const lv_img_dsc_t* num_img = retrieve_numeral_image(digit);
                    lv_img_set_src(handle->image_items[IMAGE_MIN_2], num_img);
                    break;
                }
                case 5:
                case 6:
                case 7:
                    break;
            }
        }
        char date_line[128];
        sprintf(date_line, "%s, %s %d", get_day_name(curr_time->tm_wday, DAY_NAME_FULLNAME), get_month_name(curr_time->tm_mon), curr_time->tm_mday);
        lv_label_set_text(handle->date_label, date_line);
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
        sprintf(forcast_line, "%s %d", get_day_name(weather_cond->forcast_date->tm_wday, DAY_NAME_ABBREV), weather_cond->forcast_date->tm_mday);
        lv_label_set_text(handle->forcast_date_label, forcast_line);

        lv_label_set_text(handle->forcast_desc_label, weather_cond->description);

        sprintf(forcast_line, "%.0f", weather_cond->temperature);
        lv_label_set_text(handle->curr_temp_label, forcast_line);

        sprintf(forcast_line, "Hi: %.0f/Lo: %.0f", weather_cond->hi_temp, weather_cond->lo_temp);
        lv_label_set_text(handle->forcast_temp_label, forcast_line);
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
        if ((trigger_day = alarm_scheduler_get_next_day(next_alarm)) >= 0)
        {
            sprintf(alarm_line, "Alarm set: %s %d:%02d %s", get_day_name(trigger_day, DAY_NAME_ABBREV), config_mgr_format_hour(handle->config_mgr, next_alarm->trigger_time.hour), next_alarm->trigger_time.min, alarm_scheduler_is_morning(&next_alarm->trigger_time) ? "am" : "pm");
            lv_label_set_text(handle->alarm_label, alarm_line);
        }
        else
        {
            lv_label_set_text(handle->alarm_label, "");
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
            lv_obj_del_async(lv_obj_get_parent(handle->alarm_box));
        }
        else
        {
            char alarm_line[128];
            lv_style_t modal_style;

            // Create a full-screen background
            //lv_style_copy(&modal_style, &lv_style_plain_color);

            // Set the background's style
            //modal_style.body.main_color = modal_style.body.grad_color = LV_COLOR_BLACK;
            //modal_style.body.opa = LV_OPA_70;

            // Create a base object for the modal background
            lv_obj_t* model_shade = lv_obj_create(handle->win_bkgrd, NULL);
            //lv_obj_set_style(model_shade, &modal_style);
            lv_obj_set_pos(model_shade, 0, 0);
            lv_obj_set_size(model_shade, LV_HOR_RES, LV_VER_RES);
            //lv_obj_set_opa_scale_enable(model_shade, true); // Enable opacity scaling for the animation

            // Create the message box as a child of the modal background
            sprintf(alarm_line, "Alarm: %s", alarm_triggered->alarm_text);

            handle->alarm_box = lv_msgbox_create(model_shade, NULL);
            lv_msgbox_add_btns(handle->alarm_box, g_alarm_buttons);
            lv_msgbox_set_text(handle->alarm_box, alarm_line);
            lv_obj_align(handle->alarm_box, NULL, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_event_cb(handle->alarm_box, alarm_msgbox_callback);
            lv_obj_set_user_data(handle->alarm_box, handle);

            // Fade the message box in with an animation
            // lv_anim_t anim;
            // lv_anim_init(&anim);
            // lv_anim_set_time(&anim, 500);
            // lv_anim_set_values(&anim, LV_OPA_TRANSP, LV_OPA_COVER);
            // lv_anim_set_exec_cb(&anim, model_shade, (lv_anim_exec_xcb_t)lv_obj_set_opa_scale);
            // lv_anim_create(&anim);

            //lv_label_set_text(info, in_msg_info);
            //lv_obj_align(info, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 5, -5);
        }
    }
    return result;
}

void gui_mgr_process_items(GUI_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        SDL_Event event;
        lv_task_handler();
        mouse_handler(&event);

        // if (handle->alarm_state == ALARM_STATE_TRIGGERED)
        // {
        // }
        // else if (handle->alarm_state == ALARM_STATE_SNOOZE)
        // {
        // }

        GUI_OPTION_DLG_INFO option_info = {0};
        if (handle->window_mode == WINDOW_MODE_SHOW_OPTIONS)
        {
            option_info.dlg_state = OPTION_DLG_OPEN;
            handle->notify_cb(handle->user_ctx, NOTIFICATION_OPTION_DLG, &option_info);
        }
        else if (handle->window_mode == WINDOW_MODE_CLOSE_OPTIONS)
        {
            option_info.dlg_state = OPTION_DLG_CLOSED;
            option_info.is_dirty = handle->new_alarm_dlg.is_dirty;
            handle->notify_cb(handle->user_ctx, NOTIFICATION_OPTION_DLG, &option_info);
            handle->window_mode = WINDOW_MODE_CLOCK;
        }
    }
}

void gui_mgr_show_alarm_dlg(GUI_MGR_HANDLE handle, SCHEDULER_HANDLE sched_handle)
{
    if (handle != NULL && sched_handle != NULL)
    {
        handle->window_mode = WINDOW_MODE_OPTIONS;
        // Show the dialog
        lv_obj_t* parent = handle->win_bkgrd;//lv_obj_get_parent(alarm_dlg_obj);

        memset(&handle->new_alarm_dlg, 0, sizeof(handle->new_alarm_dlg));
        handle->new_alarm_dlg.sched_handle = sched_handle;
        handle->new_alarm_dlg.is_dirty = false;

        lv_obj_t* alarm_opt_win = lv_win_create(parent, NULL);
        lv_obj_t* win_btn = lv_win_add_btn(alarm_opt_win, LV_SYMBOL_CLOSE);
        lv_win_set_title(alarm_opt_win, "Alarm Dialog");
        lv_obj_set_size(alarm_opt_win, lv_disp_get_hor_res(NULL) - VALUE_BUFFER_SIZE, lv_disp_get_ver_res(NULL) - VALUE_BUFFER_SIZE);
        lv_obj_set_pos(alarm_opt_win, 5, 5);
        lv_obj_set_top(alarm_opt_win, true);
        lv_obj_set_event_cb(win_btn, alarm_dlg_btn_cb);
        lv_obj_set_user_data(win_btn, handle);

        lv_obj_t* tabview = lv_tabview_create(alarm_opt_win, NULL);
        lv_obj_t* alarm_tab = lv_tabview_add_tab(tabview, "Alarms");
        lv_obj_t* new_alarm_tab = lv_tabview_add_tab(tabview, "New Alarm");
        lv_obj_t* option_tab = lv_tabview_add_tab(tabview, "Options");

        // Setup alarm tab
        create_alarm_window(handle, alarm_tab);

        // Setup New Alarm tab
        create_new_alarm_window(handle, new_alarm_tab);

        // Setup Option tab
        create_options_window(handle, option_tab);
    }
}