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

#include "testrunnerswitcher.h"
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
#include "lib-util-c/item_list.h"
#include "lib-util-c/crt_extensions.h"
#undef ENABLE_MOCKS

#include "alarm_scheduler.h"

static ALARM_INFO g_alarm_info;
static const void* g_insert_item;

static ITEM_LIST_DESTROY_ITEM g_list_destroy_item;
static void* g_destroy_user_ctx;
static const char* TEST_ALARM_TEXT = "test alarm text";
static const char* TEST_TEST_SOUND_FILE = "test sound text";

static void setup_alarm_time_info(void)
{
    time_t current_time = time(NULL);
    struct tm* curr_time = gmtime(&current_time);

    switch (curr_time->tm_wday)
    {
        case 0:
            g_alarm_info.trigger_days = Sunday;
            break;
        case 1:
            g_alarm_info.trigger_days = Monday;
            break;
        case 2:
            g_alarm_info.trigger_days = Tuesday;
            break;
        case 3:
            g_alarm_info.trigger_days = Wednesday;
            break;
        case 4:
            g_alarm_info.trigger_days = Thursday;
            break;
        case 5:
            g_alarm_info.trigger_days = Friday;
            break;
        case 6:
            g_alarm_info.trigger_days = Saturday;
            break;
        default:
            g_alarm_info.trigger_days = NoDay;
            break;
    }
    g_alarm_info.trigger_time.hour = curr_time->tm_hour;
    g_alarm_info.trigger_time.min = curr_time->tm_min;
    g_alarm_info.alarm_text = (char*)TEST_ALARM_TEXT;
    g_alarm_info.sound_file = (char*)TEST_TEST_SOUND_FILE;
}

static ITEM_LIST_HANDLE my_item_list_create(ITEM_LIST_DESTROY_ITEM destroy_cb, void* user_ctx)
{
    g_list_destroy_item = destroy_cb;
    g_destroy_user_ctx = user_ctx;
    return my_mem_shim_malloc(1);
}

static void my_item_list_destroy(ITEM_LIST_HANDLE handle)
{
    if (g_insert_item != NULL)
    {
        g_list_destroy_item(g_destroy_user_ctx, (void*)g_insert_item);
        g_insert_item = NULL;
    }
    my_mem_shim_free(handle);
}

static int my_item_list_add_item(ITEM_LIST_HANDLE handle, const void* item)
{
    (void)handle;
    g_insert_item = item;
    return 0;
}

static const void* my_item_list_get_item(ITEM_LIST_HANDLE handle, size_t item_index)
{
    return g_insert_item;
}

static int my_clone_string(char** target, const char* source)
{
    size_t len = strlen(source);
    *target = my_mem_shim_malloc(len+1);
    strcpy(*target, source);
    return 0;
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

static TEST_MUTEX_HANDLE g_testByTest;

BEGIN_TEST_SUITE(alarm_scheduler_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(time_t, int);
        REGISTER_UMOCK_ALIAS_TYPE(ITEM_LIST_DESTROY_ITEM, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ITEM_LIST_HANDLE, void*);

        result = umocktypes_charptr_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_HOOK(item_list_create, my_item_list_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(item_list_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(item_list_destroy, my_item_list_destroy);
        REGISTER_GLOBAL_MOCK_HOOK(item_list_add_item, my_item_list_add_item);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(item_list_add_item, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(item_list_item_count, 0);
        REGISTER_GLOBAL_MOCK_HOOK(item_list_get_item, my_item_list_get_item);
        //REGISTER_GLOBAL_MOCK_RETURN(item_list_get_item, g_insert_item);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(item_list_get_item, NULL);

        REGISTER_GLOBAL_MOCK_HOOK(clone_string, my_clone_string);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(clone_string, __LINE__);
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
        g_list_destroy_item = NULL;
        g_destroy_user_ctx = NULL;
        g_insert_item = NULL;
    }

    TEST_FUNCTION_CLEANUP(function_cleanup)
    {
        TEST_MUTEX_RELEASE(g_testByTest);
    }

    static void setup_alarm_scheduler_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(item_list_create(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    }

    static void setup_alarm_scheduler_add_alarm_info_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(clone_string(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(clone_string(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(item_list_add_item(IGNORED_PTR_ARG, IGNORED_PTR_ARG));
    }

    TEST_FUNCTION(alarm_scheduler_create_success)
    {
        // arrange
        setup_alarm_scheduler_create_mocks();

        // act
        SCHEDULER_HANDLE handle = alarm_scheduler_create();

        // assert
        ASSERT_IS_NOT_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    TEST_FUNCTION(alarm_scheduler_create_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_alarm_scheduler_create_mocks();

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                SCHEDULER_HANDLE handle = alarm_scheduler_create();

                // assert
                ASSERT_IS_NULL(handle);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(alarm_scheduler_destroy_success)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_destroy(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

        // act
        alarm_scheduler_destroy(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(alarm_scheduler_destroy_handle_NULL_success)
    {
        // arrange

        // act
        alarm_scheduler_destroy(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(alarm_scheduler_add_alarm_info_handle_NULL_fail)
    {
        // arrange

        // act
        int result = alarm_scheduler_add_alarm_info(NULL, &g_alarm_info);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(alarm_scheduler_add_alarm_info_success)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info();
        umock_c_reset_all_calls();

        setup_alarm_scheduler_add_alarm_info_mocks();

        // act
        int result = alarm_scheduler_add_alarm_info(handle, &g_alarm_info);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    TEST_FUNCTION(alarm_scheduler_add_alarm_info_fail)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info();
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_alarm_scheduler_add_alarm_info_mocks();
        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = alarm_scheduler_add_alarm_info(handle, &g_alarm_info);

                // assert
                ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }

        // cleanup
        alarm_scheduler_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(alarm_scheduler_add_alarm_success)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info();
        umock_c_reset_all_calls();

        setup_alarm_scheduler_add_alarm_info_mocks();

        // act
        TIME_INFO tm_info = { 4, 5 };
        int result = alarm_scheduler_add_alarm(handle, TEST_ALARM_TEXT, &tm_info, Monday|Tuesday, TEST_TEST_SOUND_FILE);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    TEST_FUNCTION(alarm_scheduler_add_alarm_fail)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info();
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_alarm_scheduler_add_alarm_info_mocks();
        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                TIME_INFO tm_info = { 4, 5 };
                int result = alarm_scheduler_add_alarm(handle, TEST_ALARM_TEXT, &tm_info, Monday|Tuesday, TEST_TEST_SOUND_FILE);

                // assert
                ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }

        // cleanup
        alarm_scheduler_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(alarm_scheduler_is_triggered_alert_success)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info();
        (void)alarm_scheduler_add_alarm_info(handle, &g_alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_PTR_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_PTR_ARG, IGNORED_NUM_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle);

        // assert
        ASSERT_IS_NOT_NULL(alarm_info);
        ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_TEXT, alarm_info->alarm_text);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

END_TEST_SUITE(alarm_scheduler_ut)
