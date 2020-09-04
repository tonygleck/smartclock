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
MOCKABLE_FUNCTION(, int, json_object_get_boolean, const JSON_Object*, object, const char *, name);
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
static const char* TEST_VALID_TIME_VAL2 = "9:17:00";
static const char* TEST_INVALID_TIME_VAL = "11:A9:32";

static const char* TEST_DIGITCOLOR_NODE = "digitColor";
static const char* TEST_ZIPCODE_NODE = "zipcode";
static const char* TEST_ZIPCODE = "12345";
static const char* TEST_NEW_ZIPCODE = "67890";
static const char* TEST_NTP_ADDRESS = "127.0.0.1";
static const char* TEST_AUDIO_DIR = "/audio/directory";
static const char* TEST_SHADE_START = "shadeStart";
static const char* TEST_SHADE_END = "shadeEnd";
static const char* TEST_DEMO_MODE = "demo_mode";

static const char* TEST_ALARM_NAME = "alarm text 1";
static uint8_t TEST_SNOOZE_MIN = 15;
static uint8_t TEST_ALARM_ID = 234;
static const char* TEST_ALARM_SOUND = "alarm_sound1.mp3";
static uint8_t TEST_ALARM_FREQUENCY = 127;
static TIME_VALUE_STORAGE TEST_ALARM_ARRAY = { 12, 30, 0 };
static TIME_VALUE_STORAGE TEST_INVALID_ALARM_ARRAY = { 25, 30, 0 };
static uint32_t TEST_DIGIT_COLOR = 3;
static uint32_t TEST_DEFAULT_DIGIT_COLOR = 0;

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
        REGISTER_UMOCK_ALIAS_TYPE(JSON_Status, int);

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
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_number, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_object_get_number, JSONError);
        REGISTER_GLOBAL_MOCK_RETURN(json_serialize_to_file_pretty, JSONSuccess);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_serialize_to_file_pretty, JSONFailure);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_count, 1);
        REGISTER_GLOBAL_MOCK_RETURN(json_array_get_object, TEST_JSON_OBJECT);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(json_array_get_object, NULL);
        REGISTER_GLOBAL_MOCK_RETURN(json_object_get_boolean, 1);

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
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "option")).CallCannotFail();
    }

    static void setup_config_mgr_load_alarm_mocks(void)
    {
        STRICT_EXPECTED_CALL(json_object_get_array(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_array_get_count(IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(json_array_get_object(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "name"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "sound"));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, "time")).SetReturn(TEST_VALID_TIME_VAL);
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, "id")).CallCannotFail();
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

    CTEST_FUNCTION(config_mgr_store_alarm_handle_NULL_fail)
    {
        // arrange

        // act
        int result = config_mgr_store_alarm(NULL, TEST_ALARM_NAME, &TEST_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN, TEST_ALARM_ID);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_store_alarm_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        setup_config_mgr_store_alarm_mocks();

        // act
        int result = config_mgr_store_alarm(handle, TEST_ALARM_NAME, &TEST_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN, TEST_ALARM_ID);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_store_alarm_invalid_time_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_store_alarm(handle, TEST_ALARM_NAME, &TEST_INVALID_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN, TEST_ALARM_ID);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
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
                int result = config_mgr_store_alarm(handle, TEST_ALARM_NAME, &TEST_ALARM_ARRAY, TEST_ALARM_SOUND, TEST_ALARM_FREQUENCY, TEST_SNOOZE_MIN, TEST_ALARM_ID);

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

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_ZIPCODE_NODE)).SetReturn(NULL);

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

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_ZIPCODE_NODE)).SetReturn(TEST_ZIPCODE);

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
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_ZIPCODE_NODE)).SetReturn(TEST_ZIPCODE);
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

    CTEST_FUNCTION(config_mgr_set_zipcode_handle_NULL_fail)
    {
        // arrange

        // act
        int result = config_mgr_set_zipcode(NULL, TEST_NEW_ZIPCODE);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_set_zipcode_zipcode_NULL_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_zipcode(handle, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_zipcode_zipcode_empty_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_zipcode(handle, "");

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_zipcode_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_ZIPCODE_NODE, TEST_NEW_ZIPCODE));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_ZIPCODE_NODE)).SetReturn(TEST_NEW_ZIPCODE);

        // act
        int result = config_mgr_set_zipcode(handle, TEST_NEW_ZIPCODE);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_zipcode_set_json_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_ZIPCODE_NODE, TEST_NEW_ZIPCODE)).SetReturn(JSONFailure);

        // act
        int result = config_mgr_set_zipcode(handle, TEST_NEW_ZIPCODE);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_zipcode_get_string_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_ZIPCODE_NODE, TEST_NEW_ZIPCODE));
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_ZIPCODE_NODE)).SetReturn(NULL);

        // act
        int result = config_mgr_set_zipcode(handle, TEST_NEW_ZIPCODE);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_digit_color_handle_NULL_fail)
    {
        // arrange

        // act
        uint32_t result = config_mgr_get_digit_color(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(uint32_t, TEST_DEFAULT_DIGIT_COLOR, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_get_digit_color_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, TEST_DIGITCOLOR_NODE)).SetReturn(TEST_DIGIT_COLOR);

        // act
        uint32_t result = config_mgr_get_digit_color(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(uint32_t, TEST_DIGIT_COLOR, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_digit_color_fail)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, TEST_DIGITCOLOR_NODE)).SetReturn(JSONError);

        // act
        uint32_t result = config_mgr_get_digit_color(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(uint32_t, TEST_DEFAULT_DIGIT_COLOR, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_digit_color_cache_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        STRICT_EXPECTED_CALL(json_object_get_number(IGNORED_ARG, TEST_DIGITCOLOR_NODE)).SetReturn(TEST_DIGIT_COLOR);
        (void)config_mgr_get_digit_color(handle);
        umock_c_reset_all_calls();

        // act
        uint32_t result = config_mgr_get_digit_color(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(uint32_t, TEST_DIGIT_COLOR, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_digit_color_success)
    {
        // arrange
        uint32_t new_color = 10;

        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_number(IGNORED_ARG, TEST_DIGITCOLOR_NODE, new_color));

        // act
        int result = config_mgr_set_digit_color(handle, new_color);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_digit_color_fail)
    {
        // arrange
        uint32_t new_color = 10;

        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_number(IGNORED_ARG, TEST_DIGITCOLOR_NODE, new_color)).SetReturn(JSONFailure);

        // act
        int result = config_mgr_set_digit_color(handle, new_color);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_digit_color_handle_NULL_fail)
    {
        // arrange
        uint32_t new_color = 10;

        // act
        int result = config_mgr_set_digit_color(NULL, new_color);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
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

    CTEST_FUNCTION(config_mgr_format_hour_handle_NULL_success)
    {
        // arrange

        // act
        uint8_t result = config_mgr_format_hour(NULL, 15);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_format_hour_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        uint8_t result = config_mgr_format_hour(handle, 15);
        CTEST_ASSERT_ARE_EQUAL(int, 3, result);
        result = config_mgr_format_hour(handle, 3);
        CTEST_ASSERT_ARE_EQUAL(int, 3, result);

        // assert

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_show_seconds_handle_NULL_fail)
    {
        // arrange

        // act
        bool result = config_mgr_show_seconds(NULL);

        // assert
        CTEST_ASSERT_IS_FALSE(result);

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_show_seconds_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        bool result = config_mgr_show_seconds(handle);

        // assert
        CTEST_ASSERT_IS_FALSE(result);

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_shade_times_handle_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time;
        TIME_VALUE_STORAGE end_time;
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_get_shade_times(NULL, &start_time, &end_time);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_get_shade_times_start_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE end_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_get_shade_times(handle, NULL, &end_time);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_shade_times_end_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_get_shade_times(handle, &start_time, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_shade_times_success)
    {
        // arrange
        TIME_VALUE_STORAGE start_time;
        TIME_VALUE_STORAGE end_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_SHADE_START)).SetReturn(TEST_VALID_TIME_VAL);
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_SHADE_END)).SetReturn(TEST_VALID_TIME_VAL2);

        // act
        int result = config_mgr_get_shade_times(handle, &start_time, &end_time);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(int, 11, start_time.hours);
        CTEST_ASSERT_ARE_EQUAL(int, 59, start_time.minutes);
        CTEST_ASSERT_ARE_EQUAL(int, 32, start_time.seconds);
        CTEST_ASSERT_ARE_EQUAL(int, 9, end_time.hours);
        CTEST_ASSERT_ARE_EQUAL(int, 17, end_time.minutes);
        CTEST_ASSERT_ARE_EQUAL(int, 0, end_time.seconds);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_get_shade_times_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time;
        TIME_VALUE_STORAGE end_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_SHADE_START)).SetReturn(TEST_VALID_TIME_VAL);
        STRICT_EXPECTED_CALL(json_object_get_string(IGNORED_ARG, TEST_SHADE_END)).SetReturn(TEST_VALID_TIME_VAL2);

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = config_mgr_get_shade_times(handle, &start_time, &end_time);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }

        // cleanup
        config_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_set_shade_times_handle_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time = { 16, 29, 22 };
        TIME_VALUE_STORAGE end_time = { 12, 3, 0 };
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_shade_times(NULL, &start_time, &end_time);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_set_shade_times_start_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE end_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_shade_times(handle, NULL, &end_time);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_shade_times_end_NULL_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time;
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_shade_times(handle, &start_time, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_shade_times_success)
    {
        // arrange
        TIME_VALUE_STORAGE start_time = { 16, 29, 22 };
        TIME_VALUE_STORAGE end_time = { 12, 3, 0 };
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_SHADE_START, IGNORED_ARG));
        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_SHADE_END, IGNORED_ARG));

        // act
        int result = config_mgr_set_shade_times(handle, &start_time, &end_time);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_shade_times_fail)
    {
        // arrange
        TIME_VALUE_STORAGE start_time = { 16, 29, 22 };
        TIME_VALUE_STORAGE end_time = { 12, 3, 0 };
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_SHADE_START, "16:29:22"));
        STRICT_EXPECTED_CALL(json_object_set_string(IGNORED_ARG, TEST_SHADE_END, "12:03:00"));

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = config_mgr_set_shade_times(handle, &start_time, &end_time);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }

        // cleanup
        config_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(config_mgr_is_demo_mode_handle_NULL_fail)
    {
        // arrange

        // act
        bool result = config_mgr_is_demo_mode(NULL);

        // assert
        CTEST_ASSERT_IS_TRUE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_is_demo_mode_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_boolean(IGNORED_ARG, TEST_DEMO_MODE));

        // act
        bool result = config_mgr_is_demo_mode(handle);

        // assert
        CTEST_ASSERT_IS_TRUE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_is_demo_mode_is_false_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(json_object_get_boolean(IGNORED_ARG, TEST_DEMO_MODE)).SetReturn(0);

        // act
        bool result = config_mgr_is_demo_mode(handle);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_24h_clock_handle_NULL_fail)
    {
        // arrange

        // act
        int result = config_mgr_set_24h_clock(NULL, true);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_set_24h_clock_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_24h_clock(handle, true);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_IS_TRUE(config_mgr_is_24h_clock(handle));
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_is_24h_clock_handle_NULL_fail)
    {
        // arrange

        // act
        bool result = config_mgr_is_24h_clock(NULL);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_is_24h_clock_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        bool result = config_mgr_is_24h_clock(handle);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_set_celsius_handle_NULL_fail)
    {
        // arrange

        // act
        int result = config_mgr_set_celsius(NULL, true);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_set_celsius_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        int result = config_mgr_set_celsius(handle, true);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_IS_TRUE(config_mgr_is_celsius(handle));
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

    CTEST_FUNCTION(config_mgr_is_celsius_handle_NULL_fail)
    {
        // arrange

        // act
        bool result = config_mgr_is_celsius(NULL);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(config_mgr_is_celsius_success)
    {
        // arrange
        CONFIG_MGR_HANDLE handle = config_mgr_create(TEST_CONFIG_PATH);
        umock_c_reset_all_calls();

        // act
        bool result = config_mgr_is_celsius(handle);

        // assert
        CTEST_ASSERT_IS_FALSE(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        config_mgr_destroy(handle);
    }

CTEST_END_TEST_SUITE(config_mgr_ut)
