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
#include "umock_c/umock_c_prod.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "macro_utils.h"

#define ENABLE_MOCKS
//#include "lib-util-c/alarm_timer.h"
#include "azure_c_shared_utility/gballoc.h"
#undef ENABLE_MOCKS

#include "alarm_scheduler.h"

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

static TEST_MUTEX_HANDLE g_testByTest;


static void setup_mocks(void)
{
}

BEGIN_TEST_SUITE(alarm_scheduler_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(time_t, int);

        result = umocktypes_charptr_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);

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

    TEST_FUNCTION(create_handle_fail)
    {
        // arrange
        //STRICT_EXPECTED_CALL(alarm_timer_create()).SetReturn(NULL);

        // act
        //NTP_CLIENT_HANDLE handle = ntp_client_create();

        // assert
        ASSERT_IS_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

END_TEST_SUITE(alarm_scheduler_ut)
