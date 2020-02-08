#ifndef ALARM_SCHEDULER_H
#define ALARM_SCHEDULER_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#include <stdbool.h>
#endif /* __cplusplus */

typedef struct TIME_INFO_TAG
{
    uint8_t hour;
    uint8_t min;
} TIME_INFO;

enum DayOfTheWeek
{
    NoDay = 0x0,
    Monday = 0x1,
    Tuesday = 0x2,
    Wednesday = 0x4,
    Thursday = 0x8,
    Friday = 0x10,
    Saturday = 0x20,
    Sunday = 0x40,
};

typedef struct ALARM_INFO_TAG
{
    TIME_INFO trigger_time;
    uint32_t trigger_days;
    char* alarm_text;
    char* sound_file;
} ALARM_INFO;

typedef struct ALARM_SCHEDULER_TAG* SCHEDULER_HANDLE;

extern SCHEDULER_HANDLE alarm_scheduler_create(void);
extern void alarm_scheduler_destroy(SCHEDULER_HANDLE handle);

extern const ALARM_INFO* alarm_scheduler_is_triggered(SCHEDULER_HANDLE handle);
extern int alarm_scheduler_add_alarm(SCHEDULER_HANDLE handle, const char* alarm_text, const TIME_INFO* time, uint32_t trigger_days, const char* sound_file);
extern int alarm_scheduler_add_alarm_info(SCHEDULER_HANDLE handle, const ALARM_INFO* alarm_info);
extern int alarm_scheduler_remove_alarm(SCHEDULER_HANDLE handle, const char* alarm_text);
extern const ALARM_INFO* alarm_scheduler_get_next_alarm(SCHEDULER_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // ALARM_SCHEDULER
