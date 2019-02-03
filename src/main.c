
#include <stdio.h>

#include "ntp_client.h"

static const char* NTP_SERVER_ADDRESS = "0.north-america.pool.ntp.org";

#define NTP_TIMEOUT         5

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result)
{
    if (ntp_result == NTP_OPERATION_RESULT_SUCCESS)
    {

    }
    else
    {
        printf("Failure retrieving NTP time %d", ntp_result);
    }
}

static int check_ntp_time()
{

    NTP_CLIENT_HANDLE ntp_client = ntp_client_create();
    if (ntp_client != NULL)
    {
        int operation_complete = 0;
        if (ntp_client_get_time(ntp_client, NTP_SERVER_ADDRESS, NTP_TIMEOUT, ntp_result_callback, &operation_complete) == 0)
        {
            do
            {
                ntp_client_process(ntp_client);
                // Sleep here
            } while (operation_complete == 0);
        }
    }
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;


    return 0;
}