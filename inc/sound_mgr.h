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

//extern int ntp_client_get_time(NTP_CLIENT_HANDLE handle, const char* time_server, size_t timeout_sec, NTP_TIME_CALLBACK ntp_callback, void* user_ctx);
//extern void ntp_client_process(NTP_CLIENT_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // SOUND_MGR_H
