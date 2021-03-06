// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#include <cstdio>
#else
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#endif

static void* my_mem_shim_malloc(size_t size)
{
    return malloc(size);
}

static void my_mem_shim_free(void* ptr)
{
    free(ptr);
}

/**
 * Include the test tools.
 */
#include "ctest.h"
#include "umock_c/umock_c_prod.h"
#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"

#define ENABLE_MOCKS
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/sys_debug_shim.h"
#include "patchcords/patchcord_client.h"
#include "patchcords/cord_socket_client.h"
#undef ENABLE_MOCKS

#include "ntp_client.h"

#define ENABLE_MOCKS
MOCKABLE_FUNCTION(, void, ntp_time_callback, void*, user_ctx, NTP_OPERATION_RESULT, ntp_result, time_t, current_time);
#undef ENABLE_MOCKS

static const char* TEST_NTP_SERVER_ADDRESS = "test_server.org";

#define TEST_cord_socket_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242
#define TEST_IO_HANDLE                            (CORD_HANDLE)0x4243

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
    CTEST_ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

static ON_IO_OPEN_COMPLETE g_on_io_open_complete;
static void* g_on_io_open_complete_context;
static ON_BYTES_RECEIVED g_on_bytes_received;
static void* g_on_bytes_received_context;
static ON_IO_ERROR g_on_io_error;
static void* g_on_io_error_context;
static ON_IO_CLOSE_COMPLETE g_on_io_close_complete;
static void* g_on_io_close_complete_context;
static int g_call_completion = 0;

static int my_socket_open(CORD_HANDLE socket_io, ON_IO_OPEN_COMPLETE on_io_open_complete, void* on_io_open_complete_context)
{
    (void)socket_io;
    g_on_io_open_complete = on_io_open_complete;
    g_on_io_open_complete_context = on_io_open_complete_context;
    return 0;
}

static int my_socket_close(CORD_HANDLE socket_io, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context)
{
    (void)socket_io;
    g_on_io_close_complete = on_io_close_complete;
    g_on_io_close_complete_context = callback_context;
    return 0;
}

static CORD_HANDLE my_socket_create(const void* create_parameters, const PATCHCORD_CALLBACK_INFO* client_cb)
{
    (void)create_parameters;
    g_on_bytes_received = client_cb->on_bytes_received;
    g_on_bytes_received_context = client_cb->on_bytes_received_ctx;
    g_on_io_error = client_cb->on_io_error;
    g_on_io_error_context = client_cb->on_io_error_ctx;
    return my_mem_shim_malloc(1);
}

static void my_socket_destroy(CORD_HANDLE socket_io)
{
    my_mem_shim_free(socket_io);
}

static void my_cord_socket_process_item(CORD_HANDLE xio)
{
    if (g_call_completion == 1)
    {
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        g_call_completion++;
    }
    else if (g_call_completion == 2)
    {
        g_on_bytes_received(g_on_bytes_received_context, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);
        g_call_completion++;
    }
    else if (g_call_completion == 3)
    {
        g_on_io_close_complete(g_on_io_close_complete_context);
    }
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
    STRICT_EXPECTED_CALL(cord_socket_create(IGNORED_ARG, IGNORED_ARG));
    STRICT_EXPECTED_CALL(cord_socket_open(IGNORED_ARG, IGNORED_ARG, handle));
    STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, ntp_timeout));
}

CTEST_BEGIN_TEST_SUITE(ntp_client_ut)

    CTEST_SUITE_INITIALIZE()
    {
        int result;

        (void)umock_c_init(on_umock_c_error);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);
        //REGISTER_TYPE(IO_SEND_RESULT, IO_SEND_RESULT);
        REGISTER_UMOCK_ALIAS_TYPE(CORD_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_OPEN_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_BYTES_RECEIVED, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_ERROR, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_IO_CLOSE_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_SEND_COMPLETE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(NTP_OPERATION_RESULT, int);
        REGISTER_UMOCK_ALIAS_TYPE(time_t, long);
        //REGISTER_TYPE(const cord_socket_CONFIG*, const_cord_socket_CONFIG_ptr);
        //REGISTER_UMOCK_ALIAS_TYPE(cord_socket_CONFIG*, const cord_socket_CONFIG*);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_init, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_init, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_is_expired, false);

        REGISTER_GLOBAL_MOCK_HOOK(cord_socket_create, my_socket_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(cord_socket_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(cord_socket_destroy, my_socket_destroy)
        REGISTER_GLOBAL_MOCK_HOOK(cord_socket_process_item, my_cord_socket_process_item);

        REGISTER_GLOBAL_MOCK_HOOK(cord_socket_open, my_socket_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(cord_socket_open, __LINE__);
        REGISTER_GLOBAL_MOCK_HOOK(cord_socket_close, my_socket_close);
        REGISTER_GLOBAL_MOCK_RETURN(cord_socket_send, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(cord_socket_send, __LINE__);

        //REGISTER_GLOBAL_MOCK_HOOK(ntp_time_callback, my_ntp_time_callback);

        result = umocktypes_charptr_register_types();
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_UMOCK_ALIAS_TYPE(ALARM_TIMER_HANDLE, void*);

        memset(&g_test_recv_packet, 0, sizeof(TEST_NTP_PACKET) );
        g_test_recv_packet.li_vn_mode = 0x1b;
        g_test_recv_packet.txTm_s = 133102800; // Thursday, March 21, 1974 1:00:00 PM
    }

    CTEST_SUITE_CLEANUP()
    {
        umock_c_deinit();
    }

    CTEST_FUNCTION_INITIALIZE()
    {
        umock_c_reset_all_calls();
        g_call_completion = 0;
    }

    CTEST_FUNCTION_CLEANUP()
    {
    }

    CTEST_FUNCTION(ntp_client_create_handle_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG)).SetReturn(__LINE__);

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                NTP_CLIENT_HANDLE handle = ntp_client_create();

                // assert
                CTEST_ASSERT_IS_NULL(handle);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(ntp_client_create_succeed)
    {
        // arrange
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG));

        // act
        NTP_CLIENT_HANDLE handle = ntp_client_create();

        // assert
        CTEST_ASSERT_IS_NOT_NULL(handle);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_destroy_handle_NULL_succeed)
    {
        // arrange

        // act
        ntp_client_destroy(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(ntp_client_destroy_handle_succeed)
    {
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        ntp_client_destroy(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(ntp_client_get_time_handle_NULL_fail)
    {
        size_t ntp_timeout = 20;
        // arrange
        umock_c_reset_all_calls();

        // act
        int result = ntp_client_get_time(NULL, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(ntp_client_get_time_server_address_NULL_fail)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        // act
        int result = ntp_client_get_time(handle, NULL, ntp_timeout, my_ntp_time_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_get_time_fail)
    {
        size_t ntp_timeout = 20;
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

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
            CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "ntp_client_get_time_fail test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        ntp_client_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(ntp_client_get_time_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        umock_c_reset_all_calls();

        setup_ntp_client_get_time_mocks(handle, ntp_timeout);

        // act
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_process_no_timeout_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_send(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_reset(IGNORED_ARG));

        // act
        ntp_client_process(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_process_connected_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_send(IGNORED_ARG, IGNORED_ARG, 48, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_reset(IGNORED_ARG));

        // act
        ntp_client_process(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_process_recv_response_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        g_on_bytes_received(g_on_bytes_received_context, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        //STRICT_EXPECTED_CALL(ntp_time_callback(IGNORED_ARG, NTP_OP_RESULT_SUCCESS, IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_close(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_destroy(IGNORED_ARG));

        // act
        ntp_client_process(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_on_io_bytes_received_recv_context_NULL_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        umock_c_reset_all_calls();

        // act
        g_on_bytes_received(NULL, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_on_io_bytes_received_succeed)
    {
        size_t ntp_timeout = 20;
        // arrange
        NTP_CLIENT_HANDLE handle = ntp_client_create();
        int result = ntp_client_get_time(handle, TEST_NTP_SERVER_ADDRESS, ntp_timeout, my_ntp_time_callback, NULL);
        g_on_io_open_complete(g_on_io_open_complete_context, IO_OPEN_OK);
        ntp_client_process(handle);
        umock_c_reset_all_calls();

        // act
        g_on_bytes_received(g_on_bytes_received_context, (const unsigned char*)&g_test_recv_packet, NTP_TEST_PACKET_SIZE);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        ntp_client_destroy(handle);
    }

    CTEST_FUNCTION(ntp_client_set_time_server_NULL_fail)
    {
        size_t ntp_timeout = 20;

        // arrange

        // act
        int result = ntp_client_set_time(NULL, ntp_timeout);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(ntp_client_set_time_succeed)
    {
        size_t ntp_timeout = 20;
        g_call_completion = 1;

        // arrange
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_create(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_open(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, ntp_timeout));
        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_send(IGNORED_ARG, IGNORED_ARG, 48, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_reset(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_close(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(cord_socket_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        int result = ntp_client_set_time(TEST_NTP_SERVER_ADDRESS, ntp_timeout);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

CTEST_END_TEST_SUITE(ntp_client_ut)
