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
#else
#include <stddef.h>
#endif

/**
 * Include the test tools.
 */
#include "ctest.h"
#include "umock_c/umock_c.h"
#include "umock_c/umocktypes_bool.h"
#include "umock_c/umocktypes_stdint.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c_prod.h"

#define ENABLE_MOCKS
#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/crt_extensions.h"
#include "http_client/http_client.h"
#include "parson.h"

MOCKABLE_FUNCTION(, JSON_Value*, json_parse_string, const char *, string);
MOCKABLE_FUNCTION(, JSON_Object*, json_value_get_object, const JSON_Value *, value);
MOCKABLE_FUNCTION(, JSON_Array*, json_object_get_array, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, size_t, json_array_get_count, const JSON_Array*, array);
MOCKABLE_FUNCTION(, JSON_Status, json_serialize_to_file, const JSON_Value*, value, const char *, filename);
MOCKABLE_FUNCTION(, JSON_Value*, json_parse_file, const char*, string);
MOCKABLE_FUNCTION(, double, json_object_get_number, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, JSON_Array*, json_array_get_array, const JSON_Array*, array, size_t, index);
MOCKABLE_FUNCTION(, JSON_Value*, json_array_get_value, const JSON_Array*, array, size_t, index);
MOCKABLE_FUNCTION(, JSON_Value*, json_object_get_value, const JSON_Object *, object, const char *, name);
MOCKABLE_FUNCTION(, const char*, json_object_get_string, const JSON_Object*, object, const char *, name);
MOCKABLE_FUNCTION(, const char*, json_value_get_string, const JSON_Value*, value);
MOCKABLE_FUNCTION(, JSON_Object*, json_object_get_object, const JSON_Object*, object, const char*, name);
MOCKABLE_FUNCTION(, void, json_value_free, JSON_Value*, value);
MOCKABLE_FUNCTION(, double, json_value_get_number, const JSON_Value*, value);
#undef ENABLE_MOCKS

#include "weather_client.h"

#define ENABLE_MOCKS
//MOCKABLE_FUNCTION(, void, condition_callback, void*, user_ctx, WEATHER_OPERATION_RESULT, result, const WEATHER_CONDITIONS*, conditions);
#undef ENABLE_MOCKS

static JSON_Value* TEST_JSON_VALUE = (JSON_Value*)0x11111117;
static JSON_Object* TEST_JSON_OBJECT = (JSON_Object*)0x11111118;
static JSON_Array* TEST_ARRAY_OBJECT = (JSON_Array*)0x11111119;

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
static const char* TEST_NODE_STRING = "{ node: \"data\" }";
static const char* TEST_DESCRIPTION = "light intensity drizzle";
static const char* TEST_ICON = "t_icon";

static const char* TEST_ZIPCODE = "98077";

static size_t TEST_WEATHER_CONTENT_LEN = 20;

#define TEST_SOCKETIO_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242
#define TEST_DEFAULT_TIMEOUT_VALUE              10
#define TEST_HTTP_HEADER                        (HTTP_HEADERS_HANDLE)0x4243

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    CTEST_ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

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
static bool g_error_condition_called;

static void condition_callback(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* conditions)
{
    (void)user_ctx;
    if (result == WEATHER_OP_RESULT_SUCCESS)
    {
        g_error_condition_called = false;
        CTEST_ASSERT_ARE_EQUAL(char_ptr, conditions->description, "light intensity drizzle");
    }
    else
    {
        g_error_condition_called = true;
        CTEST_ASSERT_IS_NULL(conditions);
    }
}

static HTTP_CLIENT_HANDLE my_http_client_create(void)
{
    return (HTTP_CLIENT_HANDLE)my_mem_shim_malloc(1);
}

static int my_http_client_open(HTTP_CLIENT_HANDLE handle, const HTTP_ADDRESS* http_address, ON_HTTP_OPEN_COMPLETE_CALLBACK on_open_complete_cb, void* user_ctx, ON_HTTP_ERROR_CALLBACK on_error_cb, void* err_user_ctx)
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

static int my_http_client_close(HTTP_CLIENT_HANDLE handle, ON_HTTP_CLIENT_CLOSE on_close_cb, void* user_ctx)
{
    (void)handle;
    on_close_cb(user_ctx);
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

static int my_clone_string(char** target, const char* source)
{
    size_t len = strlen(source);
    *target = my_mem_shim_malloc(len+1);
    strcpy(*target, source);
    return 0;
}

CTEST_BEGIN_TEST_SUITE(weather_client_ut)

    CTEST_SUITE_INITIALIZE()
    {
        int result;

        (void)umock_c_init(on_umock_c_error);
        (void)umocktypes_bool_register_types();
        (void)umocktypes_stdint_register_types();

        REGISTER_UMOCK_ALIAS_TYPE(time_t, long);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_ERROR_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_OPEN_COMPLETE_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_CLIENT_CLOSE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(HTTP_CLIENT_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(HTTP_HEADERS_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_HTTP_REQUEST_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ALARM_TIMER_HANDLE, void*);

        REGISTER_UMOCK_ALIAS_TYPE(WEATHER_OPERATION_RESULT, int);

        REGISTER_UMOCK_ALIAS_TYPE(HTTP_CLIENT_RESULT, int);
        REGISTER_UMOCK_ALIAS_TYPE(HTTP_CLIENT_REQUEST_TYPE, int);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_init, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_init, __LINE__);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_start, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_timer_start, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_is_expired, false);

        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_create, my_http_client_create);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_destroy, my_http_client_destroy);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_close, my_http_client_close);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_open, my_http_client_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_open, HTTP_CLIENT_ERROR);
        REGISTER_GLOBAL_MOCK_HOOK(http_client_execute_request, my_http_client_execute_request)
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(http_client_execute_request, HTTP_CLIENT_ERROR);

        REGISTER_GLOBAL_MOCK_HOOK(clone_string, my_clone_string);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(clone_string, __LINE__);

        REGISTER_GLOBAL_MOCK_RETURN(json_parse_string, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_parse_string, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_value_get_object, TEST_JSON_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_value_get_object, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_value, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_get_value, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_string, TEST_NODE_STRING);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_get_string, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_value_get_string, TEST_NODE_STRING);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_value_get_string, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_object, TEST_JSON_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_get_object, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_array, TEST_ARRAY_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_get_array, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_value, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_value, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_number, 1);

        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_value, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_value, NULL);

        //REGISTER_GLOBAL_MOCK_HOOK(condition_callback, my_condition_callback);

        CTEST_ASSERT_ARE_EQUAL(int, 0, umocktypes_charptr_register_types());
    }

    CTEST_SUITE_CLEANUP()
    {
        umock_c_deinit();
    }

    CTEST_FUNCTION_INITIALIZE()
    {
        umock_c_reset_all_calls();
    }

    CTEST_FUNCTION_CLEANUP()
    {
    }

    static void setup_weather_client_process_recv_mocks(void)
    {
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_parse_string(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_array_get_count(IGNORED_ARG)).CallCannotFail().SetReturn(1);
        STRICT_EXPECTED_CALL(json_array_get_value(IGNORED_ARG, 0));
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "description")).SetReturn(TEST_DESCRIPTION);
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "icon")).CallCannotFail().SetReturn(TEST_ICON);

        STRICT_EXPECTED_CALL(json_object_get_object(IGNORED_ARG, "main"));
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "pressure")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "humidity")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp_min")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp_max")).CallCannotFail();

        STRICT_EXPECTED_CALL(json_value_free(IGNORED_ARG));

        //STRICT_EXPECTED_CALL(condition_callback(IGNORED_ARG, WEATHER_OP_RESULT_SUCCESS, IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    }

    static void setup_weather_client_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG));
    }

    static void setup_open_connection_mocks(void)
    {
        STRICT_EXPECTED_CALL(http_client_create());
        STRICT_EXPECTED_CALL(http_client_set_trace(IGNORED_ARG, false)).CallCannotFail();
        STRICT_EXPECTED_CALL(http_client_open(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, TEST_DEFAULT_TIMEOUT_VALUE));
        //STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
    }

    static void setup_weather_client_process_send_mocks(void)
    {
        STRICT_EXPECTED_CALL(http_client_execute_request(IGNORED_ARG, HTTP_CLIENT_REQUEST_GET, IGNORED_ARG, NULL, NULL, 0, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, TEST_DEFAULT_TIMEOUT_VALUE));
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));
    }

    static void setup_close_connection(void)
    {
        STRICT_EXPECTED_CALL(http_client_close(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));
        STRICT_EXPECTED_CALL(http_client_destroy(IGNORED_ARG));
    }

    static void setup_weather_desc(void)
    {
        STRICT_EXPECTED_CALL(json_parse_string(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));

        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, "weather"));
        STRICT_EXPECTED_CALL(json_array_get_count(IGNORED_ARG)).CallCannotFail().SetReturn(1);
        STRICT_EXPECTED_CALL(json_array_get_value(IGNORED_ARG, 0));
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "description")).SetReturn(TEST_DESCRIPTION);
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "icon")).SetReturn(TEST_ICON);

        STRICT_EXPECTED_CALL(json_object_get_object(IGNORED_ARG, "main"));
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "pressure")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "humidity")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp_min")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "temp_max")).CallCannotFail();

        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "dt"));

        STRICT_EXPECTED_CALL(json_value_free(IGNORED_ARG));

        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        //STRICT_EXPECTED_CALL(condition_callback(IGNORED_ARG, WEATHER_OP_RESULT_SUCCESS, IGNORED_ARG));
        //STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    }

    CTEST_FUNCTION(weather_client_create_api_key_NULL_fail)
    {
        // arrange

        // act
        WEATHER_CLIENT_HANDLE handle = weather_client_create(NULL);

        // assert
        CTEST_ASSERT_IS_NULL(handle);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_create_handle_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_weather_client_create_mocks();

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                WEATHER_CLIENT_HANDLE handle = weather_client_create(TEST_WEATHER_API_KEY);

                // assert
                CTEST_ASSERT_IS_NULL(handle);
            }
        }

        //cleanup
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(weather_client_create_succeed)
    {
        // arrange
        setup_weather_client_create_mocks();

        // act
        WEATHER_CLIENT_HANDLE handle = weather_client_create(TEST_WEATHER_API_KEY);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(handle);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(handle);
    }

    CTEST_FUNCTION(weather_client_destroy_not_open_succeed)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(http_client_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        weather_client_destroy(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_destroy_open_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        setup_close_connection();
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        weather_client_destroy(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_destroy_handle_NULL_succeed)
    {
        // arrange

        // act
        weather_client_destroy(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_close_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        setup_close_connection();

        // act
        int result = weather_client_close(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_close_handle_NULL_fail)
    {
        // arrange

        // act
        int result = weather_client_close(NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_set_units_handle_NULL_fail)
    {
        // arrange

        // act
        int result = weather_client_set_units(NULL, UNIT_KELVIN);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_set_units_kelvin_succeed)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_set_units(client_handle, UNIT_KELVIN);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_IS_TRUE(UNIT_KELVIN == weather_client_get_units(client_handle));
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_set_units_celsius_succeed)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_set_units(client_handle, UNIT_CELSIUS);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_IS_TRUE(UNIT_CELSIUS == weather_client_get_units(client_handle));
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_units_handle_NULL_succeed)
    {
        // arrange

        // act
        TEMPERATURE_UNITS result = weather_client_get_units(NULL);

        // assert
        CTEST_ASSERT_IS_TRUE(UNIT_FAHRENHEIGHT == result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_get_units_succeed)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        TEMPERATURE_UNITS result = weather_client_get_units(client_handle);

        // assert
        CTEST_ASSERT_IS_TRUE(UNIT_FAHRENHEIGHT == result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_handle_null_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        // act
        int result = weather_client_get_by_coordinate(NULL, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_location_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_coordinate(client_handle, NULL, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_callback_NULL_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        setup_open_connection_mocks();

        // act
        int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_invalid_state_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        int result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        // act
        result = weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_coordinate_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        CTEST_ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

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
            CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_handle_null_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };

        // act
        int result = weather_client_get_by_zipcode(NULL, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_zero_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_zipcode(client_handle, 0, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_callback_NULL_fail)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        setup_open_connection_mocks();
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));

        // act
        int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_2nd_call_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        size_t len = strlen(TEST_ACTUAL_WEATHER);
        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, len, 200, TEST_HTTP_HEADER);
        umock_c_reset_all_calls();

        setup_weather_desc();
        weather_client_process(client_handle);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        setup_open_connection_mocks();
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));

        // act
        result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_invalid_stat_fail)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        // act
        result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        CTEST_ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

        setup_open_connection_mocks();
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));

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

            int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

            // assert
            CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(weather_client_get_by_zipcode_process_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_set_units(client_handle, UNIT_CELSIUS);
        umock_c_reset_all_calls();

        const char* TEST_ZIP_CODE_PATH = "/data/2.5/weather?zip=98077,us&units=metric&appid=test_key";
        setup_open_connection_mocks();
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(http_client_execute_request(IGNORED_ARG, HTTP_CLIENT_REQUEST_GET, TEST_ZIP_CODE_PATH, NULL, NULL, 0, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));

        // act
        int result = weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_city_handle_null_fail)
    {
        // arrange

        // act
        int result = weather_client_get_by_city(NULL, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_get_by_city_name_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_city(client_handle, NULL, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_city_callback_NULL_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, NULL, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_city_succeed)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        setup_open_connection_mocks();

        // act
        int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_city_invalid_state_fail)
    {
        // arrange

        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        // act
        result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_get_by_city_fail)
    {
        // arrange
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        CTEST_ASSERT_ARE_EQUAL(int, 0, umock_c_negative_tests_init());

        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        setup_open_connection_mocks();

        umock_c_negative_tests_snapshot();

        // act
        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                int result = weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in test %lu/%lu", (unsigned long)index, (unsigned long)count);
            }
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(weather_client_process_handle_null_fail)
    {
        // arrange

        // act
        weather_client_process(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(weather_client_process_idle_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        // act
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_send_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        setup_weather_client_process_send_mocks();

        // act
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(on_http_open_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        // act
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(on_http_open_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(http_client_destroy(IGNORED_ARG));

        // act
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_ERROR);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(on_http_open_user_ctx_NULL_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        umock_c_reset_all_calls();

        // act
        g_on_http_open_complete(NULL, HTTP_CLIENT_OK);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(on_http_error_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        setup_close_connection();

        // act
        g_on_io_error(g_on_io_error_context, HTTP_CLIENT_DISCONNECTION);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(on_http_error_user_ctx_NULL_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        umock_c_reset_all_calls();

        // act
        g_on_io_error(NULL, HTTP_CLIENT_DISCONNECTION);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_send_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_weather_client_process_send_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
                g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);

                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                weather_client_process(client_handle);
                g_error_condition_called = false;
                weather_client_process(client_handle);

                // assert
                CTEST_ASSERT_IS_TRUE(g_error_condition_called, "weather_client_process_send failure %zu/%zu", index, count);
            }
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(weather_client_process_sent_timeout_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_city(client_handle, TEST_CITY_NAME, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(alarm_timer_is_expired(IGNORED_ARG)).SetReturn(true);
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));
        //STRICT_EXPECTED_CALL(condition_callback(IGNORED_ARG, WEATHER_OP_RESULT_TIMEOUT, NULL));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        setup_close_connection();

        weather_client_process(client_handle);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_recv_and_parse_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        setup_weather_desc();
        STRICT_EXPECTED_CALL(http_client_process_item(IGNORED_ARG));

        size_t len = strlen(TEST_ACTUAL_WEATHER);
        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, len, 200, TEST_HTTP_HEADER);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_close(client_handle);
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_recv_500_http_status_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        setup_close_connection();

        // act
        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, NULL, 0, 404, TEST_HTTP_HEADER);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_recv_http_error_succeed)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        //STRICT_EXPECTED_CALL(condition_callback(IGNORED_ARG, WEATHER_OP_RESULT_INVALID_DATA_ERR, NULL));
        setup_close_connection();

        // act
        g_on_request_callback(g_on_request_context, HTTP_CLIENT_ERROR, NULL, 0, 200, TEST_HTTP_HEADER);
        weather_client_process(client_handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_request_callback_recv_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        weather_client_process(client_handle);
        umock_c_reset_all_calls();

        // act
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG)).SetReturn(NULL);

        size_t len = strlen(TEST_ACTUAL_WEATHER);
        g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, len, 200, TEST_HTTP_HEADER);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        weather_client_destroy(client_handle);
    }

    CTEST_FUNCTION(weather_client_process_recv_fail)
    {
        // arrange
        WEATHER_LOCATION location = { 1.0, 2.0 };
        WEATHER_CLIENT_HANDLE client_handle = weather_client_create(TEST_WEATHER_API_KEY);
        weather_client_get_by_zipcode(client_handle, TEST_ZIPCODE, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
        g_on_http_open_complete(g_on_http_open_complete_context, HTTP_CLIENT_OK);
        size_t len = strlen(TEST_ACTUAL_WEATHER);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_weather_client_process_recv_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                weather_client_process(client_handle);
                g_on_request_callback(g_on_request_context, HTTP_CLIENT_OK, TEST_ACTUAL_WEATHER, len, 200, TEST_HTTP_HEADER);

                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                weather_client_process(client_handle);

                g_error_condition_called = false;
                weather_client_process(client_handle);

                // assert
                CTEST_ASSERT_IS_TRUE(g_error_condition_called);

                weather_client_get_by_coordinate(client_handle, &location, TEST_DEFAULT_TIMEOUT_VALUE, condition_callback, NULL);
            }
        }

        // cleanup
        weather_client_destroy(client_handle);
        umock_c_negative_tests_deinit();
    }

CTEST_END_TEST_SUITE(weather_client_ut)
