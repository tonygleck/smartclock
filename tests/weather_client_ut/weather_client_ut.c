// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#else
#include <stdlib.h>
#endif

static void* my_mem_shim_malloc(size_t size)
{
    return malloc(size);
}

static void my_mem_shim_free(void* ptr)
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
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umocktypes_stdint.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"

#define ENABLE_MOCKS
#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/crt_extensions.h"
#include "http_client/http_client.h"
#include "patchcords/xio_client.h"
#include "patchcords/xio_socket.h"
#include "parson.h"
#undef ENABLE_MOCKS

#include "weather_client.h"

#define ENABLE_MOCKS
/*MOCKABLE_FUNCTION(, JSON_Value*, json_parse_string, const char *, string);
MOCKABLE_FUNCTION(, JSON_Status, json_serialize_to_file, const JSON_Value*, value, const char *, filename);
MOCKABLE_FUNCTION(, JSON_Value*, json_parse_file, const char*, string);
MOCKABLE_FUNCTION(, JSON_Object*, json_value_get_object, const JSON_Value *, value);
MOCKABLE_FUNCTION(, size_t, json_array_get_count, const JSON_Array*, array);
MOCKABLE_FUNCTION(, JSON_Array*, json_object_get_array, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, double, json_object_get_number, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, JSON_Array*, json_array_get_array, const JSON_Array*, array, size_t, index);
MOCKABLE_FUNCTION(, JSON_Value*, json_array_get_value, const JSON_Array*, array, size_t, index);
MOCKABLE_FUNCTION(, JSON_Value*, json_object_get_value, const JSON_Object *, object, const char *, name);
MOCKABLE_FUNCTION(, const char*, json_object_get_string, const JSON_Object*, object, const char *, name);
MOCKABLE_FUNCTION(, const char*, json_value_get_string, const JSON_Value*, value);
MOCKABLE_FUNCTION(, JSON_Object*, json_object_get_object, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, void, json_value_free, JSON_Value*, value);
MOCKABLE_FUNCTION(, double, json_value_get_number, const JSON_Value*, value);*/
MOCKABLE_FUNCTION(, void, condition_callback, void*, user_ctx, WEATHER_OPERATION_RESULT, result, const WEATHER_CONDITIONS*, conditions);

#undef ENABLE_MOCKS

static const ALARM_TIMER_HANDLE TEST_TIMER_HANDLE = (ALARM_TIMER_HANDLE)0x1234;
static const char* TEST_WEATHER_SERVER_ADDRESS = "test_weather.org";
static const char* TEST_WEATHER_API_KEY = "test_key";
static const char* TEST_CITY_NAME = "test_city";
static const char* TEST_ACTUAL_WEATHER = "{\"coord\": {\"lon\": -0.13,\"lat\": 51.51 \
},\"weather\": [ {\"id\": 300,\"main\": \"Drizzle\", \"description\": \"light intensity drizzle\", \
\"icon\": \"09d\"}],\"base\": \"stations\",\"main\": {\"temp\": 280.32,\"pressure\": 1012,\"humidity\": 81, \
\"temp_min\": 279.15,\"temp_max\": 281.15},\"visibility\": 10000,\"wind\": {\"speed\": 4.1,\"deg\": 80}, \
\"clouds\": {\"all\": 90},\"dt\": 1485789600,\"sys\": {\"type\": 1,\"id\": 5091,\"message\": 0.0103,\"country\": \
\"GB\",\"sunrise\": 1485762037,\"sunset\": 1485794875},\"id\": 2643743,\"name\": \"London\",\"cod\": 200}";

static size_t TEST_WEATHER_CONTENT_LEN = 20;

#define TEST_SOCKETIO_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242
#define TEST_IO_HANDLE                          (XIO_HANDLE)0x4243
#define TEST_DEFAULT_TIMEOUT_VALUE              10
#define TEST_HTTP_HEADER                        (HTTP_HEADERS_HANDLE)0x4243

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

TEST_DEFINE_ENUM_TYPE(HTTP_CLIENT_RESULT, HTTP_CLIENT_RESULT_VALUE);
IMPLEMENT_UMOCK_C_ENUM_TYPE(HTTP_CLIENT_RESULT, HTTP_CLIENT_RESULT_VALUE);

// TEST_DEFINE_ENUM_TYPE(HTTP_CLIENT_RESULT, HTTP_CALLBACK_REASON_VALUES);
// IMPLEMENT_UMOCK_C_ENUM_TYPE(HTTP_CALLBACK_REASON, HTTP_CALLBACK_REASON_VALUES);

TEST_DEFINE_ENUM_TYPE(HTTP_CLIENT_REQUEST_TYPE, HTTP_CLIENT_REQUEST_TYPE_VALUES);
IMPLEMENT_UMOCK_C_ENUM_TYPE(HTTP_CLIENT_REQUEST_TYPE, HTTP_CLIENT_REQUEST_TYPE_VALUES);

static TEST_MUTEX_HANDLE g_testByTest;

static ON_HTTP_OPEN_COMPLETE_CALLBACK g_on_http_open_complete;
static void* g_on_http_open_complete_context;
// static ON_SEND_COMPLETE g_on_io_send_complete;
// static void* g_on_io_send_complete_context;

static ON_HTTP_REQUEST_CALLBACK g_on_request_callback;
static void* g_on_request_context;
static ON_HTTP_ERROR_CALLBACK g_on_io_error;
static void* g_on_io_error_context;

static ON_HTTP_CLIENT_CLOSE g_on_io_close_complete;
static void* g_on_io_close_complete_context;

static void my_condition_callback(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* conditions)
{
    (void)user_ctx;
    if (result == WEATHER_OP_RESULT_SUCCESS)
    {
        ASSERT_ARE_EQUAL(char_ptr, conditions->description, "light intensity drizzle");
    }
    else
    {
        ASSERT_IS_NULL(conditions);
    }
}

static HTTP_CLIENT_HANDLE my_http_client_create(void)
{
    return (HTTP_CLIENT_HANDLE)my_mem_shim_malloc(1);
}

static int my_http_client_open(HTTP_CLIENT_HANDLE handle, XIO_INSTANCE_HANDLE xio_handle, ON_HTTP_OPEN_COMPLETE_CALLBACK on_open_complete_cb, void* user_ctx, ON_HTTP_ERROR_CALLBACK on_error_cb, void* err_user_ctx)
{
    (void)handle;
    g_on_io_error = on_error_cb;
    g_on_io_error_context = err_user_ctx;
    g_on_http_open_complete = on_open_complete_cb;
    g_on_http_open_complete_context = user_ctx;
    return 0;
}

static void my_http_client_destroy(HTTP_CLIENT_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static int my_http_client_execute_request(HTTP_CLIENT_HANDLE handle, HTTP_CLIENT_REQUEST_TYPE request_type, const char* relative_path,
    HTTP_HEADERS_HANDLE http_header, const unsigned char* content, size_t content_length, ON_HTTP_REQUEST_CALLBACK on_request_callback, void* callback_ctx)
{
    (void)handle;
    (void)request_type;
    (void)relative_path;
    (void)http_header;
    (void)content;
    (void)content_length;
    g_on_request_callback = on_request_callback;
    g_on_request_context = callback_ctx;
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

BEGIN_TEST_SUITE(weather_client_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);
        (void)umocktypes_bool_register_types();
        (void)umocktypes_stdint_register_types();

        REGISTER_UMOCK_ALIAS_TYPE(time_t, int);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_ERROR_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_OPEN_COMPLETE_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_CLIENT_CLOSE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(HTTP_CLIENT_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(HTTP_HEADERS_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_REQUEST_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(XIO_INSTANCE_HANDLE, void*);

        REGISTER_UMOCK_ALIAS_TYPE(WEATHER_OPERATION_RESULT, int);

        REGISTER_TYPE(HTTP_CLIENT_RESULT, HTTP_CLIENT_RESULT);
        REGISTER_TYPE(HTTP_CLIENT_REQUEST_TYPE, HTTP_CLIENT_REQUEST_TYPE);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_create, TEST_TIMER_HANDLE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_create, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_is_expired, false);

        REGISTER_GLOBAL_MOCK_RETURN(xio_socket_get_interface, TEST_SOCKETIO_INTERFACE_DESCRIPTION);

        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_create, my_http_client_create);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_destroy, my_http_client_destroy);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_open, my_http_client_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_open, HTTP_CLIENT_ERROR);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_execute_request, my_http_client_execute_request)
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_execute_request, HTTP_CLIENT_ERROR);

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

    static void setup_open_connection_mocks(void)
    {
        STRICT_EXPECTED_CALL(http_client_create());
        STRICT_EXPECTED_CALL(xio_socket_get_interface()).CallCannotFail();
        STRICT_EXPECTED_CALL(xio_client_create(TEST_SOCKETIO_INTERFACE_DESCRIPTION, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(http_client_set_trace(IGNORED_PTR_ARG, true)).CallCannotFail();
        STRICT_EXPECTED_CALL(http_client_open(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_PTR_ARG, TEST_DEFAULT_TIMEOUT_VALUE));
    }

    static void setup_close_connection(void)
    {
        STRICT_EXPECTED_CALL(http_client_close(IGNORED_PTR_ARG, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        for (size_t index = 0; index < 10; index++)
        {
            STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_PTR_ARG));
        }
        STRICT_EXPECTED_CALL(http_client_destroy(IGNORED_PTR_ARG));
    }

    TEST_FUNCTION(weather_client_create_api_key_NULL_fail)
    {
        // arrange

        // act
        WEATHER_CLIENT_HANDLE handle = weather_client_create(NULL, UNIT_CELSIUS);

        // assert
        ASSERT_IS_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_create_handle_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_create());

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (!umock_c_negative_tests_can_call_fail(index))
            {
                continue;
            }

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            WEATHER_CLIENT_HANDLE handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);

            // assert
            ASSERT_IS_NULL(handle);
        }

        //cleanup
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(weather_client_create_succeed)
    {
        // arrange
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_create());

        // act
        WEATHER_CLIENT_HANDLE handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);

        // assert
        ASSERT_IS_NOT_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(handle);
    }

    TEST_FUNCTION(weather_client_destroy_not_open_succeed)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(http_client_destroy(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_destroy(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

        // act
        weather_client_destroy(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_destroy_open_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        setup_close_connection();
        STRICT_EXPECTED_CALL(alarm_timer_destroy(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

        // act
        weather_client_destroy(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_destroy_handle_NULL_succeed)
    {
        // arrange

        // act
        weather_client_destroy(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_get_by_coordinate_handle_null_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        // act
        int result = weather_client_get_by_coordinate(NULL, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_get_by_coordinate_location_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_coordinate(client_handle, NULL, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_coordinate_callback_NULL_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_coordinate_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        setup_open_connection_mocks();

        // act
        int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_coordinate_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

        setup_open_connection_mocks();

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (!umock_c_negative_tests_can_call_fail(index))
            {
                continue;
            }

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(weather_client_get_by_zipcode_handle_null_fail)
    {
        size_t zipcode = 98077;

        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        // act
        int result = weather_client_get_by_zipcode(NULL, zipcode, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_get_by_zipcode_zero_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_zipcode(client_handle, 0, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_zipcode_callback_NULL_fail)
    {
        // arrange
        size_t zipcode = 98077;

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_zipcode(client_handle, zipcode, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_zipcode_succeed)
    {
        // arrange
        size_t zipcode = 98077;

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        setup_open_connection_mocks();

        // act
        int result = weather_client_get_by_zipcode(client_handle, zipcode, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_zipcode_fail)
    {
        // arrange
        size_t zipcode = 98077;

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

        setup_open_connection_mocks();

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (!umock_c_negative_tests_can_call_fail(index))
            {
                continue;
            }

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            int result = weather_client_get_by_zipcode(client_handle, zipcode, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(weather_client_get_by_city_handle_null_fail)
    {
        // arrange

        // act
        int result = weather_client_get_by_city(NULL, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_get_by_city_name_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_city(client_handle, NULL, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_city_callback_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_city_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        setup_open_connection_mocks();

        // act
        int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_get_by_city_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        setup_open_connection_mocks();

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (!umock_c_negative_tests_can_call_fail(index))
            {
                continue;
            }

            umock_c_negative_tests_reset();
            umock_c_negative_tests_fail_call(index);

            int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

            // assert
            ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(weather_client_process_handle_null_fail)
    {
        // arrange

        // act
        weather_client_process(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(weather_client_process_idle_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_PTR_ARG));
        // setup_open_connection_mocks();
        // STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_PTR_ARG, TEST_DEFAULT_TIMEOUT_VALUE));

        // act
        weather_client_process(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_process_send_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(http_client_execute_request(IGNORED_PTR_ARG, HTTP_CLIENT_REQUEST_GET, IGNORED_PTR_ARG, NULL, NULL, 0, IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_PTR_ARG, TEST_DEFAULT_TIMEOUT_VALUE));

        // act
        weather_client_process(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_process_sent_timeout_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_is_expired(IGNORED_PTR_ARG)).SetReturn(true);
        STRICT_EXPECTED_CALL(condition_callback(IGNORED_PTR_ARG, WEATHER_OP_RESULT_TIMEOUT, NULL));
        setup_close_connection();

        weather_client_process(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_process_recv_and_parse_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(condition_callback(IGNORED_PTR_ARG, WEATHER_OP_RESULT_SUCCESS, IGNORED_PTR_ARG));

        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, strlen(TEST_ACTUAL_WEATHER), 200, TEST_HTTP_HEADER);
        weather_client_process(client_handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    TEST_FUNCTION(weather_client_process_recv_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY, UNIT_CELSIUS);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG)).SetReturn(NULL);

        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, strlen(TEST_ACTUAL_WEATHER), 200, TEST_HTTP_HEADER);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

END_TEST_SUITE(weather_client_ut)