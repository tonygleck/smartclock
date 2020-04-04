// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

typedef struct CONFIG_ALARM_INFO_TAG
{
    const char* name;
    uint8_t time_array[3];
    const char* sound_file;
    uint32_t frequency;
    uint8_t snooze;
} CONFIG_ALARM_INFO;

typedef struct CONFIG_MGR_INFO_TAG* CONFIG_MGR_HANDLE;

typedef int(*ON_ALARM_LOAD_CALLBACK)(void* context, const CONFIG_ALARM_INFO* alarm_info);

MOCKABLE_FUNCTION(, CONFIG_MGR_HANDLE, config_mgr_create, const char*, config_path);
MOCKABLE_FUNCTION(, void, config_mgr_destroy,  CONFIG_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, bool, config_mgr_save, CONFIG_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, const char*, config_mgr_get_ntp_address, CONFIG_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, const char*, config_mgr_get_zipcode, CONFIG_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, const char*, config_mgr_get_audio_dir, CONFIG_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, int, config_mgr_load_alarm, CONFIG_MGR_HANDLE, handle, ON_ALARM_LOAD_CALLBACK, alarm_cb, void*, user_ctx);
MOCKABLE_FUNCTION(, int, config_mgr_store_alarm, CONFIG_MGR_HANDLE, handle, const char*, name, const uint8_t, time_array[3], const char*, sound_file, uint32_t, frequency, uint8_t, snooze);


MOCKABLE_FUNCTION(, uint8_t, config_mgr_format_hour, CONFIG_MGR_HANDLE, handle, int, hour);
MOCKABLE_FUNCTION(, bool, config_mgr_show_seconds, CONFIG_MGR_HANDLE, handle);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // CONFIG_MGR_H
