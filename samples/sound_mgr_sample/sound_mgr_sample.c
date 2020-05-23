
#include "stdio.h"
#include "sound_mgr.h"
#include "time.h"
#include "lib-util-c/thread_mgr.h"

static const char* ALARM_AUDIO_FILENAME = "/home/jebrando/development/repo/personal/smartclock/media/audio/alarm_02.wav";

int main()
{
    SOUND_MGR_HANDLE handle = sound_mgr_create();
    if (handle != NULL)
    {
        float volume = 0.1;

        sound_mgr_play(handle, ALARM_AUDIO_FILENAME, true, false);
        do
        {
            volume = sound_mgr_get_volume(handle);
            if (volume == 1.0)
            {
                volume = 0.0;
            }
            else
            {
                volume += 0.1;
            }
            //sound_mgr_set_volume(handle, volume);
            printf("Setting the Volume to %f", volume);

            thread_mgr_sleep(5*1000);

            /*printf("Press enter to play again.  Press q to stop\n");
            int input = getchar();
            if (input == 'q')
            {
                break;
            }*/
        } while (true);

        sound_mgr_destroy(handle);
    }


    printf("Press any key to exit\r\n");
    return 0;
}