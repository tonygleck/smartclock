// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "lib-util-c/app_logging.h"

#include "time_mgr.h"


time_t get_time(void)
{
    return time(NULL);
}

struct tm* get_time_value(void)
{
    time_t mark_time = time(NULL);
    //return gmtime(&mark_time);
    return localtime(&mark_time);
}

int set_machine_time(time_t* set_time)
{
    int result;
#if PRODUCTION
        // Set time
    struct timeval time_now;
    time_now.tv_sec = *set_time;
    time_now.tv_usec = 0;
    if ((result = settimeofday(&time_now, NULL)) != 0)
    {
        log_error("Failure setting time of day");
    }
#else
    log_info("Setting Time: %s\r\n", ctime((const time_t*)&set_time) );
    result = 0;
#endif
    return result;
}