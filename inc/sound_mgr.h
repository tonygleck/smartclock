// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SOUND_MGR_H
#define SOUND_MGR_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#endif /* __cplusplus */

typedef struct SOUND_MGR_INFO_TAG* SOUND_MGR_HANDLE;

extern SOUND_MGR_HANDLE sound_mgr_create(void);
extern void sound_mgr_destroy(SOUND_MGR_HANDLE handle);

extern int sound_mgr_play(const char* sound_file);
extern int sound_mgr_stop(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // SOUND_MGR_H
