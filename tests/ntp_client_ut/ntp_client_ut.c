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

void* my_gballoc_realloc(void* ptr, size_t size)
{
    return realloc(ptr, size);
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
#include "umock_c.h"
#include "umocktypes_charptr.h"
#include "umock_c_negative_tests.h"

#define ENABLE_MOCKS
#include "lib-util-c/alarm_timer.h"
//#include "azure_c_shared_utility/gballoc.h"
#include "azure_c_shared_utility/socketio.h"
#undef ENABLE_MOCKS

#include "ntp_client.h"

static const ALARM_TIMER_HANDLE TEST_TIMER_HANDLE = (ALARM_TIMER_HANDLE)0x1234;
static const char* TEST_NTP_SERVER_ADDRESS = "test_server.org";

#define TEST_SOCKETIO_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242
#define TEST_IO_HANDLE                          (XIO_HANDLE)0x4243

DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
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

static int my_xio_open(XIO_HANDLE xio, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
    (void)xio;
    g_on_io_open_complete = on_io_open_complete;
    g_on_io_open_complete_context = on_io_open_complete_context;
    g_on_bytes_received = on_bytes_received;
    g_on_bytes_received_context = on_bytes_received_context;
    g_on_io_error = on_io_error;
    g_on_io_error_context = on_io_error_context;
    return 0;
}

static int my_xio_close(XIO_HANDLE xio, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
    (void)xio;
    g_on_io_close_complete = on_io_close_complete;
    g_on_io_close_complete_context = callback_context;
    return 0;
}

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

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result)
{

}

BEGIN_TEST_SUITE(ntp_client_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);
        //REGISTER_TYPE(IO_SEND_RESULT, IO_SEND_RESULT);
        REGISTER_UMOCK_ALIAS_TYPE(XIO_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_OPEN_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_BYTES_RECEIVED, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_ERROR, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_CLOSE_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_SEND_COMPLETE, void*);
        //REGISTER_TYPE(const SOCKETIO_CONFIG*, const_SOCKETIO_CONFIG_ptr);
        //REGISTER_UMOCK_ALIAS_TYPE(SOCKETIO_CONFIG*, const SOCKETIO_CONFIG*);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_create, TEST_TIMER_HANDLE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_create, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);

        REGISTER_GLOBAL_MOCK_RETURN(socketio_get_interface_description, TEST_SOCKETIO_INTERFACE_DESCRIPTION);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(socketio_get_interface_description, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(xio_create, TEST_IO_HANDLE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(xio_open, my_xio_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_open, __LINE__);
        REGISTER_GLOBAL_MOCK_HOOK(xio_close, my_xio_close);
        REGISTER_GLOBAL_MOCK_RETURN(xio_send, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_send, __LINE__);

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

    TEST_FUNCTION(ntp_client_create_succeed)
    {
        // arrange
        STRICT_EXPECTED_CALL(alarm_timer_create());

        // act
        NTP_CLIENT_HANDLE handle = ntp_client_create();

        // assert
        ASSERT_IS_NOT_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_destroy_handle_NULL_succeed)
    {
        // arrange

        // act
        ntp_client_destroy(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(ntp_client_get_time_handle_NULL_fail)
    {
        size_t ntp_timeout = 20;
        // arrange
        umock_c_reset_all_calls();

        // act
        int result = ntp_client_get_time(NULL, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_result_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(ntp_client_get_time_server_address_NULL_fail)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        // act
        int result = ntp_client_get_time(handle, NULL, ntp_timeout, ntp_result_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_get_time_fail)
    {
        size_t ntp_timeout = 20;
        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(socketio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SOCKETIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_open(TEST_IO_HANDLE, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle));
        STRICT_EXPECTED_CALL(alarm_timer_start(TEST_TIMER_HANDLE, ntp_timeout));

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_result_callback, NULL);

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result, "ntp_client_get_time_fail test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_get_time_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(socketio_get_interface_description());
        STRICT_EXPECTED_CALL(xio_create(TEST_SOCKETIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_open(TEST_IO_HANDLE, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle));
        STRICT_EXPECTED_CALL(alarm_timer_start(TEST_TIMER_HANDLE, ntp_timeout));

        // act
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_result_callback, NULL);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

END_TEST_SUITE(ntp_client_ut)
