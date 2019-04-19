// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

void my_gballoc_free(void* ptr)
{
    free(ptr);
}

#ifdef __cplusplus
#include <cstddef>
#include <ctime>
#else
#include <stddef.h>
#include <time.h>
#endif

/**
 * Include the test tools.
 */
#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
//#include "umock_c_prod.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "macro_utils.h"

#define ENABLE_MOCKS
#include "lib-util-c/alarm_timer.h"
#include "azure_c_shared_utility/gballoc.h"
#include "azure_uhttp_c/uhttp.h"
#undef ENABLE_MOCKS

#include "weather_client.h"

#define ENABLE_MOCKS
MOCKABLE_FUNCTION(, void, ntp_time_callback, void*, user_ctx, NTP_OPERATION_RESULT, ntp_result, time_t, current_time);
#undef ENABLE_MOCKS

static const ALARM_TIMER_HANDLE TEST_TIMER_HANDLE = (ALARM_TIMER_HANDLE)0x1234;
static const char* TEST_WEATHER_SERVER_ADDRESS = "test_weather.org";

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

static TEST_MUTEX_HANDLE g_testByTest;

static ON_IO_OPEN_COMPLETE g_on_io_open_complete;
static void* g_on_io_open_complete_context;
static ON_SEND_COMPLETE g_on_io_send_complete;
static void* g_on_io_send_complete_context;
static ON_BYTES_RECEIVED g_on_bytes_received;
static void* g_on_bytes_received_context;
static ON_IO_ERROR g_on_io_error;
static void* g_on_io_error_context;
static ON_IO_CLOSE_COMPLETE g_on_io_close_complete;
static void* g_on_io_close_complete_context;

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

BEGIN_TEST_SUITE(weather_client_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(time_t, int);
        REGISTER_UMOCK_ALIAS_TYPE(UHTTP_HANDLE, void*);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_create, TEST_TIMER_HANDLE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_create, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_is_expired, false);

        result = umocktypes_charptr_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_UMOCK_ALIAS_TYPE(ALARM_TIMER_HANDLE, void*);
    }

    TEST_SUITE_CLEANUP(suite_cleanup)
    {
        umock_c_deinit();

        TEST_MUTEX_DESTROY(g_testByTest);
    }

    TEST_FUNCTION_INITIALIZE(function_init)
    {
        if (TEST_MUTEX_ACQUIRE(g_testByTest))
        {
            ASSERT_FAIL("Could not acquire test serialization mutex.");
        }

        umock_c_reset_all_calls();
    }

    TEST_FUNCTION_CLEANUP(function_cleanup)
    {
        TEST_MUTEX_RELEASE(g_testByTest);
    }

    TEST_FUNCTION(ntp_client_create_handle_fail)
    {
        // arrange
        STRICT_EXPECTED_CALL(alarm_timer_create()).SetReturn(NULL);

        // act
        NTP_CLIENT_HANDLE handle = ntp_client_create();

        // assert
        ASSERT_IS_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }


END_TEST_SUITE(weather_client_ut)
