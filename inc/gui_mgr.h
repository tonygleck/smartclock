// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef GUI_MGR_H
#define GUI_MGR_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

#include "alarm_scheduler.h"
#include "weather_client.h"
#include "config_mgr.h"

typedef enum FORCAST_TIME_TAG
{
    FORCAST_TODAY,
    FORCAST_TOMORROW,
    FORCAST_NEXT_DAY
} FORCAST_TIME;

typedef enum GUI_NOTIFICATION_TYPE_TAG
{
    NOTIFICATION_ALARM_RESULT,
    NOTIFICATION_APPLICATION_RESULT,
    NOTIFICATION_OPTION_DLG
} GUI_NOTIFICATION_TYPE;

typedef enum GUI_OPTION_DLG_STATE_TAG
{
    OPTION_DLG_OPEN,
    OPTION_DLG_CLOSED
} GUI_OPTION_DLG_STATE;

typedef struct GUI_OPTION_DLG_INFO_TAG
{
    GUI_OPTION_DLG_STATE dlg_state;
    bool is_dirty;
} GUI_OPTION_DLG_INFO;

typedef struct GUI_MGR_INFO_TAG* GUI_MGR_HANDLE;

//typedef void(*ALARM_RESULT_CB)(void* user_ctx, ALARM_STATE_RESULT alarm_result);
typedef void(*GUI_MGR_NOTIFICATION_CB)(void* user_ctx, GUI_NOTIFICATION_TYPE type, void* res_value);

MOCKABLE_FUNCTION(, GUI_MGR_HANDLE, gui_mgr_create, CONFIG_MGR_HANDLE, config_mgr, GUI_MGR_NOTIFICATION_CB, notify_cb, void*, user_ctx);
MOCKABLE_FUNCTION(, void, gui_mgr_destroy, GUI_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, size_t, gui_mgr_get_refresh_resolution);
MOCKABLE_FUNCTION(, int, gui_mgr_create_win, GUI_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, void, gui_mgr_process_items, GUI_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, void, gui_mgr_set_time_item, GUI_MGR_HANDLE, handle, const struct tm*, curr_time);
MOCKABLE_FUNCTION(, void, gui_mgr_set_forcast, GUI_MGR_HANDLE, handle, FORCAST_TIME, timeframe, const WEATHER_CONDITIONS*, weather_cond);

MOCKABLE_FUNCTION(, int, gui_mgr_set_alarm_triggered, GUI_MGR_HANDLE, handle, const ALARM_INFO*, alarm_triggered);
MOCKABLE_FUNCTION(, void, gui_mgr_set_next_alarm, GUI_MGR_HANDLE, handle, const ALARM_INFO*, next_alarm);

MOCKABLE_FUNCTION(, void, gui_mgr_show_alarm_dlg, GUI_MGR_HANDLE, handle, SCHEDULER_HANDLE, sched_handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // GUI_MGR_H
