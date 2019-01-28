#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include <stdint.h>

typedef struct NTP_CLIENT_INFO_TAG* NTP_CLIENT_HANDLE;

typedef void(*NTP_TIME_CALLBACK)(void* user_ctx);

extern NTP_CLIENT_HANDLE ntp_client_create(void);
extern void ntp_client_destroy(NTP_CLIENT_HANDLE handle);

extern int ntp_client_get_time(NTP_CLIENT_HANDLE handle, const char* time_server, uint8_t timeout_sec, NTP_TIME_CALLBACK ntp_callback, void* user_ctx);
extern void ntp_client_process(NTP_CLIENT_HANDLE handle);


#endif // NTP_CLIENT_H
