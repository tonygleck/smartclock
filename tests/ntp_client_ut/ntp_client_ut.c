// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

static void* my_gballoc_malloc(size_t size)
{
    return malloc(size);
}

static void my_gballoc_free(void* ptr)
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
#include "azure_macro_utils/macro_utils.h"

#define ENABLE_MOCKS
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/sys_debug_shim.h"
#include "patchcords/xio_client.h"
#include "patchcords/xio_socket.h"
#undef ENABLE_MOCKS

#include "ntp_client.h"

#define ENABLE_MOCKS
MOCKABLE_FUNCTION(, void, ntp_time_callback, void*, user_ctx, NTP_OPERATION_RESULT, ntp_result, time_t, current_time);
#undef ENABLE_MOCKS

static const ALARM_TIMER_HANDLE TEST_TIMER_HANDLE = (ALARM_TIMER_HANDLE)0x1234;
static const char* TEST_NTP_SERVER_ADDRESS = "test_server.org";

#define TEST_xio_socket_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242
#define TEST_IO_HANDLE                          (XIO_IMPL_HANDLE)0x4243

#define NTP_TEST_PACKET_SIZE                    48

typedef struct TEST_NTP_PACKET_TAG
{
  uint8_t li_vn_mode;      // Eight bits. li, vn, and mode.
                           // li.   Two bits.   Leap indicator.
                           // vn.   Three bits. Version number of the protocol.
                           // mode. Three bits. Client will pick mode 3 for client.

  uint8_t stratum;         // Eight bits. Stratum level of the local clock.
  uint8_t poll;            // Eight bits. Maximum interval between successive messages.
  uint8_t precision;       // Eight bits. Precision of the local clock.

  uint32_t rootDelay;      // 32 bits. Total round trip delay time.
  uint32_t rootDispersion; // 32 bits. Max error aloud from primary clock source.
  uint32_t refId;          // 32 bits. Reference clock identifier.

  uint32_t refTm_s;        // 32 bits. Reference time-stamp seconds.
  uint32_t refTm_f;        // 32 bits. Reference time-stamp fraction of a second.

  uint32_t origTm_s;       // 32 bits. Originate time-stamp seconds.
  uint32_t origTm_f;       // 32 bits. Originate time-stamp fraction of a second.

  uint32_t rxTm_s;         // 32 bits. Received time-stamp seconds.
  uint32_t rxTm_f;         // 32 bits. Received time-stamp fraction of a second.

  uint32_t txTm_s;         // 32 bits and the most important field the client cares about. Transmit time-stamp seconds.
  uint32_t txTm_f;         // 32 bits. Transmit time-stamp fraction of a second.

} TEST_NTP_PACKET;              // Total: 384 bits or 48 bytes.

// Unix time: 39aea96e.000b3a75 ??
// Actual time: 2000-08-31_18:52:30.735861
static TEST_NTP_PACKET g_test_recv_packet = { 0xd9, 0, 10, 0, 0, 10100, 0, 0, 0, 0, 0, 0xbd5927ee, 0xbc616000, 0xbd5927ee, 0xbc616000 };

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

static int my_socket_open(XIO_IMPL_HANDLE socket_io, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context, ON_BYTES_RECEIVED on_bytes_received, void* on_bytes_received_context, ON_IO_ERROR on_io_error, void* on_io_error_context)
{
    (void)socket_io;
    g_on_io_open_complete = on_io_open_complete;
    g_on_io_open_complete_context = on_io_open_complete_context;
    g_on_bytes_received = on_bytes_received;
    g_on_bytes_received_context = on_bytes_received_context;
    g_on_io_error = on_io_error;
    g_on_io_error_context = on_io_error_context;
    return 0;
}

static int my_socket_close(XIO_IMPL_HANDLE socket_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
    (void)socket_io;
    g_on_io_close_complete = on_io_close_complete;
    g_on_io_close_complete_context = callback_context;
    return 0;
}

static XIO_IMPL_HANDLE my_socket_create(const void* create_parameters)
{
    (void)create_parameters;
    return my_gballoc_malloc(1);
}

static void my_socket_destroy(XIO_IMPL_HANDLE socket_io)
{
    my_gballoc_free(socket_io);
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

static void my_ntp_time_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{

}

static void setup_ntp_client_get_time_mocks(NTP_CLIENT_HANDLE handle, size_t ntp_timeout)
{
    STRICT_EXPECTED_CALL(xio_socket_create(IGNORED_PTR_ARG));
    STRICT_EXPECTED_CALL(xio_socket_open(IGNORED_PTR_ARG, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle, IGNORED_PTR_ARG, handle));
    STRICT_EXPECTED_CALL(alarm_timer_start(TEST_TIMER_HANDLE, ntp_timeout));
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
        REGISTER_UMOCK_ALIAS_TYPE(time_t, int);
        REGISTER_UMOCK_ALIAS_TYPE(XIO_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(XIO_IMPL_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_OPEN_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_BYTES_RECEIVED, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_ERROR, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_CLOSE_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_SEND_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(NTP_OPERATION_RESULT, int);
        //REGISTER_TYPE(const xio_socket_CONFIG*, const_xio_socket_CONFIG_ptr);
        //REGISTER_UMOCK_ALIAS_TYPE(xio_socket_CONFIG*, const xio_socket_CONFIG*);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_create, TEST_TIMER_HANDLE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_create, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_is_expired, false);

        REGISTER_GLOBAL_MOCK_HOOK(xio_socket_create, my_socket_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_socket_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(xio_socket_destroy, my_socket_destroy)

        REGISTER_GLOBAL_MOCK_HOOK(xio_socket_open, my_socket_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_socket_open, __LINE__);
        REGISTER_GLOBAL_MOCK_HOOK(xio_socket_close, my_socket_close);
        REGISTER_GLOBAL_MOCK_RETURN(xio_socket_send, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(xio_socket_send, __LINE__);

        REGISTER_GLOBAL_MOCK_HOOK(ntp_time_callback, my_ntp_time_callback);

        result = umocktypes_charptr_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_UMOCK_ALIAS_TYPE(ALARM_TIMER_HANDLE, void*);

        memset(&g_test_recv_packet, 0, sizeof(TEST_NTP_PACKET) );
        g_test_recv_packet.li_vn_mode = 0x1b;
        g_test_recv_packet.txTm_s = 133102800; // Thursday, March 21, 1974 1:00:00 PM
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
        int result = ntp_client_get_time(NULL, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);

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
        int result = ntp_client_get_time(handle, NULL, ntp_timeout, my_ntp_time_callback, NULL);

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

        setup_ntp_client_get_time_mocks(handle, ntp_timeout);

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result, "ntp_client_get_time_fail test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        ntp_client_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(ntp_client_get_time_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        setup_ntp_client_get_time_mocks(handle, ntp_timeout);

        // act
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_process_no_timeout_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(xio_socket_dowork(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_socket_send(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_NUM_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_reset(TEST_TIMER_HANDLE));

        // act
        ntp_client_process(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_process_connected_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(xio_socket_dowork(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_socket_send(IGNORED_PTR_ARG, IGNORED_PTR_ARG, 48, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_reset(TEST_TIMER_HANDLE));

        // act
        ntp_client_process(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_client_process_recv_response_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        g_on_bytes_received(g_on_bytes_received_context, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(xio_socket_dowork(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(ntp_time_callback(IGNORED_PTR_ARG, NTP_OP_RESULT_SUCCESS, IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(xio_socket_close(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_socket_dowork(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_socket_dowork(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(xio_socket_destroy(IGNORED_PTR_ARG));

        // act
        ntp_client_process(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_on_io_bytes_received_recv_context_NULL_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        umock_c_reset_all_calls();

        // act
        g_on_bytes_received(NULL, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    TEST_FUNCTION(ntp_on_io_bytes_received_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        umock_c_reset_all_calls();

        // act
        g_on_bytes_received(g_on_bytes_received_context, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

END_TEST_SUITE(ntp_client_ut)
