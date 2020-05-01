// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstdlib>
#include <cstddef>
#include <ctime>
#else
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#endif

#include "ctest.h"
#include "azure_macro_utils/macro_utils.h"
#include "umock_c/umock_c.h"

#include "umock_c/umock_c_negative_tests.h"
#include "umock_c/umocktypes_charptr.h"

static void* my_mem_shim_malloc(size_t size)
{
    return malloc(size);
}

static void my_mem_shim_free(void* ptr)
{
    free(ptr);
}

#define ENABLE_MOCKS
#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/crt_extensions.h"
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
MOCKABLE_FUNCTION(, JSON_Status, json_serialize_to_file_pretty, const JSON_Value*, value, const char*, filename);
MOCKABLE_FUNCTION(, JSON_Object*, json_array_get_object, const JSON_Array*, array, size_t, index);
MOCKABLE_FUNCTION(, JSON_Status, json_object_set_string, JSON_Object*, object, const char*, name, const char*, string);
MOCKABLE_FUNCTION(, JSON_Status, json_object_set_number, JSON_Object*, object, const char*, name, double,  number);
MOCKABLE_FUNCTION(, JSON_Status, json_object_set_value, JSON_Object*, object, const char*, name, JSON_Value*, value);
MOCKABLE_FUNCTION(, JSON_Value*, json_value_init_object);
MOCKABLE_FUNCTION(, JSON_Status, json_array_append_value, JSON_Array*, array, JSON_Value*, value);

#undef ENABLE_MOCKS

#include "config_mgr.h"

#define ENABLE_MOCKS
//MOCKABLE_FUNCTION(, int, test_alarm_load_cb, void*, context, const CONFIG_ALARM_INFO*, alarm_info);
#undef ENABLE_MOCKS

#ifdef __cplusplus
extern "C"
{
#endif

extern int clone_string_with_format(char** target, const char* format, ...);

int clone_string_with_format(char** target, const char* format, ...)
{
    const char* test_value = "test_value";
    size_t len = strlen(test_value);
    *target = my_mem_shim_malloc(len+1);
    strcpy(*target, test_value);
    return 0;

}

#ifdef __cplusplus
}
#endif

static JSON_Value* TEST_JSON_VALUE = (JSON_Value*)0x11111117;
static JSON_Object* TEST_JSON_OBJECT = (JSON_Object*)0x11111118;
static JSON_Array* TEST_ARRAY_OBJECT = (JSON_Array*)0x11111119;

static const char* TEST_NODE_STRING = "{ node: \"data\" }";
static const char* TEST_CONFIG_PATH = "/some/path/";
static const char* TEST_VALID_TIME_VAL = "11:59:32";
static const char* TEST_INVALID_TIME_VAL = "11:A9:32";
static const char* TEST_ZIPCODE = "12345";
static const char* TEST_NTP_ADDRESS = "127.0.0.1";
static const char* TEST_AUDIO_DIR = "/audio/directory";

static const char* TEST_ALARM_NAME = "alarm text 1";
static uint8_t TEST_SNOOZE_MIN = 15;
static const char* TEST_ALARM_SOUND = "alarm_sound1.mp3";
static uint8_t TEST_ALARM_FREQUENCY = 127;
static TIME_VALUE_STORAGE TEST_ALARM_ARRAY = { 12, 30, 0 };

static int load_alarms_cb(void* context, const CONFIG_ALARM_INFO* cfg_alarm)
{
    return 0;
}

static int my_clone_string(char** target, const char* source)
{
    size_t len = strlen(source);
    *target = my_mem_shim_malloc(len+1);
    strcpy(*target, source);
    return 0;
}

static JSON_Value* my_json_parse_file(const char *filename)
{
    return my_mem_shim_malloc(1);
}

static void my_json_value_free(JSON_Value *value)
{
    my_mem_shim_free(value);
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    CTEST_ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

CTEST_BEGIN_TEST_SUITE(config_mgr_ut)

    CTEST_SUITE_INITIALIZE()
    {
        int result;

        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(time_t, long);

        result = umocktypes_charptr_register_types();
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_HOOK(clone_string, my_clone_string);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(clone_string, __LINE__);

        REGISTER_GLOBAL_MOCK_HOOK(json_parse_file, my_json_parse_file);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_parse_string, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(json_value_free, my_json_value_free);
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
        REGISTER_GLOBAL_MOCK_RETURN(json_serialize_to_file_pretty, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_serialize_to_file_pretty, JSONFailure);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_count, 1);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_object, TEST_JSON_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_object, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_value, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_value, NULL);

        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_object, TEST_JSON_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_object, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_set_string, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_set_string, JSONFailure);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_set_number, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_set_number, JSONFailure);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_set_value, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_set_value, JSONFailure);
        REGISTER_GLOBAL_MOCK_RETURN(json_value_init_object, TEST_JSON_VALUE);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_value_init_object, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_append_value, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_append_value, JSONFailure);
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

    static void setup_config_mgr_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_parse_file(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));
    }

    static void setup_config_mgr_load_alarm_mocks(void)
    {
        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_array_get_count(IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(json_array_get_object(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "name"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "sound"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "time")).SetReturn(TEST_VALID_TIME_VAL);
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "snooze")).CallCannotFail();
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "frequency")).CallCannotFail();
        //STRICT_EXPECTED_CALL(test_alarm_load_cb(IGNORED_ARG, IGNORED_ARG));
    }

    static void setup_config_mgr_store_alarm_mocks(void)
    {
        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_value_init_object());
        STRICT_EXPECTED_CALL(json_value_get_object(IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_number(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_number(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_array_append_value(IGNORED_ARG, IGNORED_ARG));
    }

    CTEST_FUNCTION(config_mgr_create_success)
    {
        // arrange
        setup_config_mgr_create_mocks();

        // act
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(handle);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_create_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_config_mgr_create_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);

                // assert
                CTEST_ASSERT_IS_NULL(handle);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_destroy_handle_NULL_success)
    {
        // arrange

        // act
        config_mgr_destroy(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_destroy_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_value_free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        config_mgr_destroy(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_save_handle_NULL_fail)
    {
        // arrange

        // act
        bool result = config_mgr_save(NULL);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_save_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_serialize_to_file_pretty(IGNORED_ARG, IGNORED_ARG));

        // act
        bool result = config_mgr_save(handle);

        // assert
        CTEST_ASSERT_IS_TRUE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_save_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_serialize_to_file_pretty(IGNORED_ARG, IGNORED_ARG));
        umock_c_negative_tests_snapshot();
        umock_c_negative_tests_fail_call(0);

        // act
        bool result = config_mgr_save(handle);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_load_alarm_handle_NULL_fail)
    {
        // arrange

        // act
        int result = config_mgr_load_alarm(NULL, load_alarms_cb, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_load_alarm_callback_NULL_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_load_alarm(handle, NULL, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_load_alarm_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        setup_config_mgr_load_alarm_mocks();

        // act
        int result = config_mgr_load_alarm(handle, load_alarms_cb, NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_load_alarm_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        setup_config_mgr_load_alarm_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = config_mgr_load_alarm(handle, load_alarms_cb, NULL);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "config_mgr_load_alarm failure %d/%d", (int)index, (int)count);
            }
        }

        // cleanup
        config_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_load_alarm_invalid_time_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_array_get_count(IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(json_array_get_object(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "name"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "sound"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "time")).SetReturn(TEST_INVALID_TIME_VAL);

        // act
        int result = config_mgr_load_alarm(handle, load_alarms_cb, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_store_alarm_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        setup_config_mgr_store_alarm_mocks();

        // act
        int result = config_mgr_store_alarm(handle, TEST_ALARM_NAME, &TEST_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_store_alarm_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        setup_config_mgr_store_alarm_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = config_mgr_store_alarm(handle, TEST_ALARM_NAME, &TEST_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }
        // cleanup
        config_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_get_zipcode_handle_NULL_fail)
    {
        // arrange

        // act
        const char* result = config_mgr_get_zipcode(NULL);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_get_zipcode_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "zipcode")).SetReturn(NULL);

        // act
        const char* result = config_mgr_get_zipcode(handle);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_zipcode_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "zipcode")).SetReturn(TEST_ZIPCODE);

        // act
        const char* result = config_mgr_get_zipcode(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ZIPCODE, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_zipcode_cache_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "zipcode")).SetReturn(TEST_ZIPCODE);
        (void)config_mgr_get_zipcode(handle);
        umock_c_reset_all_calls();

        // act
        const char* result = config_mgr_get_zipcode(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        // Specifically don't check the value here.  It won't be what you expect since the
        // testing framework doesn't return a const char* ptr that is statically allocated
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_ntp_address_handle_NULL_fail)
    {
        // arrange

        // act
        const char* result = config_mgr_get_ntp_address(NULL);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_get_ntp_address_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "ntpAddress")).SetReturn(NULL);

        // act
        const char* result = config_mgr_get_ntp_address(handle);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_ntp_address_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "ntpAddress")).SetReturn(TEST_NTP_ADDRESS);

        // act
        const char* result = config_mgr_get_ntp_address(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_NTP_ADDRESS, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_ntp_address_cache_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "ntpAddress")).SetReturn(TEST_NTP_ADDRESS);
        (void)config_mgr_get_ntp_address(handle);
        umock_c_reset_all_calls();

        // act
        const char* result = config_mgr_get_ntp_address(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        // Specifically don't check the value here.  It won't be what you expect since the
        // testing framework doesn't return a const char* ptr that is statically allocated
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_audio_dir_handle_NULL_fail)
    {
        // arrange

        // act
        const char* result = config_mgr_get_audio_dir(NULL);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_get_audio_dir_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "audioDirectory")).SetReturn(NULL);

        // act
        const char* result = config_mgr_get_audio_dir(handle);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_audio_dir_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "audioDirectory")).SetReturn(TEST_AUDIO_DIR);

        // act
        const char* result = config_mgr_get_audio_dir(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_AUDIO_DIR, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_audio_dir_cache_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "audioDirectory")).SetReturn(TEST_AUDIO_DIR);
        (void)config_mgr_get_audio_dir(handle);
        umock_c_reset_all_calls();

        // act
        const char* result = config_mgr_get_audio_dir(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        // Specifically don't check the value here.  It won't be what you expect since the
        // testing framework doesn't return a const char* ptr that is statically allocated
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

CTEST_END_TEST_SUITE(config_mgr_ut)
