
#include "stdio.h"
#include "ntp_client.h"

static void ntp_timer_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    bool* connection_complete = (bool*)user_ctx;

    *connection_complete = true;
    if (ntp_result == NTP_OP_RESULT_SUCCESS)
    {
        printf("Time from time server: %s\r\n", ctime(&current_time));
    }
    else
    {
        printf("Failure from time server\r\n");
    }
}

int main()
{
    NTP_CLIENT_HANDLE ntp_handle;
    if ((ntp_handle = ntp_client_create()) == NULL)
    {
        printf("Failure creating ntp client\r\n");
    }
    else
    {
        bool conn_complete = false;
        if (ntp_client_get_time(ntp_handle, "time.nist.gov", 60, ntp_timer_callback, &conn_complete) == 0)
        {
            do
            {
                ntp_client_process(ntp_handle);
            } while (!conn_complete);
        }
        ntp_client_destroy(ntp_handle);
    }

    printf("Press any key to exit\r\n");
    return 0;
}