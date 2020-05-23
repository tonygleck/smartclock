// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SOUND_MGR_H
#define SOUND_MGR_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

#include "umock_c/umock_c_prod.h"

typedef enum SOUND_MGR_STATE_TAG
{
    SOUND_STATE_IDLE,
    SOUND_STATE_PLAYING,
    SOUND_STATE_PAUSED,
    SOUND_STATE_ERROR
} SOUND_MGR_STATE;

typedef struct SOUND_MGR_INFO_TAG* SOUND_MGR_HANDLE;

MOCKABLE_FUNCTION(, SOUND_MGR_HANDLE, sound_mgr_create);
MOCKABLE_FUNCTION(, void, sound_mgr_destroy, SOUND_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, int, sound_mgr_play, SOUND_MGR_HANDLE, handle, const char*, sound_file, bool, set_repeat, bool, crescendo);
MOCKABLE_FUNCTION(, int, sound_mgr_stop, SOUND_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, SOUND_MGR_STATE, sound_mgr_get_current_state, SOUND_MGR_HANDLE, handle);

MOCKABLE_FUNCTION(, float, sound_mgr_get_volume, SOUND_MGR_HANDLE, handle);
MOCKABLE_FUNCTION(, int, sound_mgr_set_volume, SOUND_MGR_HANDLE, handle, float, volume);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // SOUND_MGR_H
