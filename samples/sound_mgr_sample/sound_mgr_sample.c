
#include "stdio.h"
#include "sound_mgr.h"
#include "time.h"
#include "lib-util-c/thread_mgr.h"

static const char* ALARM_AUDIO_FILENAME = "/home/jebrando/development/repo/personal/smartclock/media/audio/alarm_02.wav";

static void sleep_for_now(unsigned int milliseconds)
{
#ifdef WIN32
    Sleep(milliseconds);
#else
    time_t seconds = milliseconds / 1000;
    long nsRemainder = (milliseconds % 1000) * 1000000;
    struct timespec timeToSleep = { seconds, nsRemainder };
    (void)nanosleep(&timeToSleep, NULL);
#endif
}

int main()
{
    SOUND_MGR_HANDLE handle = sound_mgr_create();
    if (handle != NULL)
    {
        do
        {
            //sound_mgr_play(handle, ALARM_AUDIO_FILENAME, true);

            //thread_mgr_sleep(6*1000);

            //printf("Press enter to play again.  Press q to stop\n");
            //int input = getchar();
            //if (input == 'q')
            //{
                break;
            //}
        } while (true);

        sound_mgr_destroy(handle);
    }


    printf("Press any key to exit\r\n");
    return 0;
}