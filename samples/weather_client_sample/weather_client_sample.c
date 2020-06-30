// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "stdio.h"
#include "stdbool.h"

#include "lib-util-c/thread_mgr.h"

#include "weather_client.h"

#define WEATHER_TIMEOUT     30

static void weather_condition_result(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* conditions)
{
    bool* operation_complete = (bool*)user_ctx;

    printf("Weather Result: %s\n", result == WEATHER_OP_RESULT_SUCCESS ? "success" : "failure");
    if (result == WEATHER_OP_RESULT_SUCCESS)
    {
        printf("temp: %.1f\nhumidity: %d\n", conditions->temperature, conditions->humidity);
    }
    *operation_complete = true;
}

int main(int argc, char* argv[])
{
    bool operation_complete = false;

    if (argc <= 1)
    {
        printf("Must have the weather api key\n");
    }
    else
    {
        const char* zipcode[] = { "98077", "41018" };
        WEATHER_CLIENT_HANDLE handle = weather_client_create(argv[1]);
        if (handle == NULL)
        {
            printf("Failure creating weather client\n");
        }
        else
        {
            for (size_t index = 0; index < 2; index++)
            {
                operation_complete = false;
                if (weather_client_get_by_zipcode(handle, zipcode[index], WEATHER_TIMEOUT, weather_condition_result, &operation_complete) != 0)
                {
                    printf("Failure getting weather client by zipcode\n");
                }
                else
                {
                    do
                    {
                        weather_client_process(handle);
                    } while (!operation_complete);
                }
                thread_mgr_sleep(5000);
            }
            weather_client_destroy(handle);
        }
    }
    return 0;
}