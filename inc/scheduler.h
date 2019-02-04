#ifndef ALARM_SCHEDULER_H
#define ALARM_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#endif /* __cplusplus */

typedef struct SCHEDULER_TAG* SCHEDULER_HANDLE;

extern SCHEDULER_HANDLE alarm_scheduler_create();
extern void alarm_scheduler_destroy(SCHEDULER_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // ALARM_SCHEDULER
