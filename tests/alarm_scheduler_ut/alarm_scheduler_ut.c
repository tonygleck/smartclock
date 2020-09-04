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
#include "lib-util-c/item_list.h"
#include "lib-util-c/crt_extensions.h"
#include "time_mgr.h"
#undef ENABLE_MOCKS

#include "alarm_scheduler.h"

static ALARM_INFO g_alarm_info;
static size_t insert_item_index = 0;
static const void* g_insert_item[3];

static ITEM_LIST_DESTROY_ITEM g_list_destroy_item;
static void* g_destroy_user_ctx;
static const char* TEST_ALARM_TEXT = "test alarm text";
static const char* TEST_TEST_SOUND_FILE = "test sound text";
static const char* TEST_ALARM_1_TEXT = "alarm_1_text";
static const char* TEST_ALARM_2_TEXT = "alarm_2_text";
static const char* TEST_ALARM_3_TEXT = "alarm_3_text";
static const uint8_t TEST_SNOOZE_VALUE = 1;

static void set_tm_struct(struct tm* curr_time)
{
    curr_time->tm_hour = 10;
    curr_time->tm_min = 10;
    curr_time->tm_year = 74;
    curr_time->tm_mday = 21;
    curr_time->tm_wday = 4;

    curr_time->tm_mon = 3;
    curr_time->tm_yday = 77;
    curr_time->tm_isdst = 0;
    curr_time->tm_sec = 0;
}

static void setup_alarm_time_info(ALARM_INFO* alarm_info, struct tm* curr_time)
{
    switch (curr_time->tm_wday)
    {
        case 0:
            alarm_info->trigger_days = Sunday;
            break;
        case 1:
            alarm_info->trigger_days = Monday;
            break;
        case 2:
            alarm_info->trigger_days = Tuesday;
            break;
        case 3:
            alarm_info->trigger_days = Wednesday;
            break;
        case 4:
            alarm_info->trigger_days = Thursday;
            break;
        case 5:
            alarm_info->trigger_days = Friday;
            break;
        case 6:
            alarm_info->trigger_days = Saturday;
            break;
        default:
            alarm_info->trigger_days = NoDay;
            break;
    }
    alarm_info->trigger_time.hour = curr_time->tm_hour;
    alarm_info->trigger_time.min = curr_time->tm_min;
    alarm_info->alarm_text = (char*)TEST_ALARM_TEXT;
    alarm_info->sound_file = (char*)TEST_TEST_SOUND_FILE;
    alarm_info->snooze_min = 1;
    alarm_info->alarm_id = 0;
}

static ITEM_LIST_HANDLE my_item_list_create(ITEM_LIST_DESTROY_ITEM destroy_cb, void* user_ctx)
{
    g_list_destroy_item = destroy_cb;
    g_destroy_user_ctx = user_ctx;
    return my_mem_shim_malloc(1);
}

static void my_item_list_destroy(ITEM_LIST_HANDLE handle)
{
    if (g_insert_item[0] != NULL)
    {
        g_list_destroy_item(g_destroy_user_ctx, (void*)g_insert_item[0]);
        g_insert_item[0] = NULL;
        insert_item_index--;
    }
    if (g_insert_item[1] != NULL)
    {
        g_list_destroy_item(g_destroy_user_ctx, (void*)g_insert_item[1]);
        g_insert_item[1] = NULL;
        insert_item_index--;
    }
    if (g_insert_item[2] != NULL)
    {
        g_list_destroy_item(g_destroy_user_ctx, (void*)g_insert_item[2]);
        g_insert_item[2] = NULL;
        insert_item_index--;
    }
    my_mem_shim_free(handle);
}

static int my_item_list_add_item(ITEM_LIST_HANDLE handle, const void* item)
{
    (void)handle;
    if (g_insert_item[0] == NULL)
    {
        g_insert_item[0] = item;
        insert_item_index++;
    }
    else if (g_insert_item[1] == NULL)
    {
        g_insert_item[1] = item;
        insert_item_index++;
    }
    else
    {
        g_insert_item[2] = item;
        insert_item_index++;
    }
    return 0;
}

static const void* my_item_list_get_item(ITEM_LIST_HANDLE handle, size_t item_index)
{
    const void* result;
    if (item_index >= insert_item_index++)
    {
        result = NULL;
    }
    else
    {
        result = g_insert_item[item_index];
    }
    return result;
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
    CTEST_ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

CTEST_BEGIN_TEST_SUITE(alarm_scheduler_ut)

    CTEST_SUITE_INITIALIZE()
    {
        int result;

        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(time_t, long);
        REGISTER_UMOCK_ALIAS_TYPE(ITEM_LIST_DESTROY_ITEM, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ITEM_LIST_HANDLE, void*);

        result = umocktypes_charptr_register_types();
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);

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
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(item_list_get_item, NULL);

        REGISTER_GLOBAL_MOCK_HOOK(clone_string, my_clone_string);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(clone_string, __LINE__);
    }

    CTEST_SUITE_CLEANUP()
    {
        umock_c_deinit();
    }

    CTEST_FUNCTION_INITIALIZE()
    {
        umock_c_reset_all_calls();
        g_list_destroy_item = NULL;
        g_destroy_user_ctx = NULL;

        insert_item_index = 0;
        g_insert_item[0] = NULL;
        g_insert_item[1] = NULL;
    }

    CTEST_FUNCTION_CLEANUP()
    {
    }

    static void setup_alarm_scheduler_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_create(IGNORED_ARG, IGNORED_ARG));
    }

    static void setup_alarm_scheduler_add_alarm_info_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(clone_string(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_add_item(IGNORED_ARG, IGNORED_ARG));
    }

    CTEST_FUNCTION(alarm_scheduler_create_success)
    {
        // arrange
        setup_alarm_scheduler_create_mocks();

        // act
        SCHEDULER_HANDLE handle = alarm_scheduler_create();

        // assert
        CTEST_ASSERT_IS_NOT_NULL(handle);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_create_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

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
                CTEST_ASSERT_IS_NULL(handle);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(alarm_scheduler_destroy_success)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        alarm_scheduler_destroy(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_destroy_handle_NULL_success)
    {
        // arrange

        // act
        alarm_scheduler_destroy(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_info_handle_NULL_fail)
    {
        // arrange

        // act
        int result = alarm_scheduler_add_alarm_info(NULL, &g_alarm_info);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_info_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        umock_c_reset_all_calls();

        setup_alarm_scheduler_add_alarm_info_mocks();

        // act
        int result = alarm_scheduler_add_alarm_info(handle, &g_alarm_info);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_info_invalid_fail)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        // act
        ALARM_INFO alarm_info1 = {0};
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 55;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        int result = alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_info_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

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
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
            }
        }

        // cleanup
        alarm_scheduler_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_handle_NULL_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        umock_c_reset_all_calls();

        // act
        TIME_INFO tm_info = { 4, 5 };
        int result = alarm_scheduler_add_alarm(NULL, TEST_ALARM_TEXT, &tm_info, Monday|Tuesday, TEST_TEST_SOUND_FILE, TEST_SNOOZE_VALUE, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_time_info_NULL_fail)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        // act
        TIME_INFO tm_info = { 4, 5 };
        int result = alarm_scheduler_add_alarm(handle, TEST_ALARM_TEXT, NULL, Monday|Tuesday, TEST_TEST_SOUND_FILE, TEST_SNOOZE_VALUE, NULL);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        umock_c_reset_all_calls();

        setup_alarm_scheduler_add_alarm_info_mocks();

        // act
        TIME_INFO tm_info = { 4, 5 };
        uint8_t alarm_id;
        int result = alarm_scheduler_add_alarm(handle, TEST_ALARM_TEXT, &tm_info, Monday|Tuesday, TEST_TEST_SOUND_FILE, TEST_SNOOZE_VALUE, &alarm_id);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_add_alarm_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

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
                int result = alarm_scheduler_add_alarm(handle, TEST_ALARM_TEXT, &tm_info, Monday|Tuesday, TEST_TEST_SOUND_FILE, TEST_SNOOZE_VALUE, NULL);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "alarm_scheduler_add_alarm failure %d/%d", (int)index, (int)count);
            }
        }

        // cleanup
        alarm_scheduler_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_handle_NULL_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        umock_c_reset_all_calls();

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(NULL, &test_tm);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_current_time_NULL_fail)
    {
        // arrange
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle, NULL);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_alert_1_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        (void)alarm_scheduler_add_alarm_info(handle, &g_alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_iterator(IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle, &test_tm);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_no_alert_success)
    {
        // arrange
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};

        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        test_tm.tm_hour = 7;
        test_tm.tm_min = 8;
        test_tm.tm_wday = 0;

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = NoDay;
        alarm_info2.trigger_time.hour = 7;
        alarm_info2.trigger_time.min = 8;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle, &test_tm);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_wrong_day_success)
    {
        // arrange
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};

        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        test_tm.tm_hour = 4;
        test_tm.tm_min = 50;
        test_tm.tm_wday = 6;

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle, &test_tm);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_is_triggered_alert_3_success)
    {
        // arrange
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};

        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        test_tm.tm_hour = 4;
        test_tm.tm_min = 50;
        test_tm.tm_wday = 1;

        SCHEDULER_HANDLE handle = alarm_scheduler_create();

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = NoDay;
        alarm_info2.trigger_time.hour = 7;
        alarm_info2.trigger_time.min = 8;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_iterator(IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_is_triggered(handle, &test_tm);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_remove_alarm_handle_NULL_fail)
    {
        // arrange

        // act
        int result = alarm_scheduler_remove_alarm(NULL, 0);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, result, 0);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_remove_alarm_no_items_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        (void)alarm_scheduler_add_alarm_info(handle, &g_alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(0);

        // act
        int result = alarm_scheduler_remove_alarm(handle, 2);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, result, 0);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_remove_alarm_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        (void)alarm_scheduler_add_alarm_info(handle, &g_alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_remove_item(IGNORED_ARG, IGNORED_ARG));

        // act
        int result = alarm_scheduler_remove_alarm(handle, 0);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, result, 0);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_remove_alarm_remove_fail)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        (void)alarm_scheduler_add_alarm_info(handle, &g_alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_remove_item(IGNORED_ARG, IGNORED_ARG)).SetReturn(__LINE__);

        // act
        int result = alarm_scheduler_remove_alarm(handle, 0);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, result, 0);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_delete_alarm_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info;
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info, &test_tm);
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_remove_item(IGNORED_ARG, IGNORED_ARG));

        // act
        int result = alarm_scheduler_delete_alarm(handle, alarm_info.alarm_id);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, result, 0);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_handle_NULL_fail)
    {
        // arrange

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(NULL);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_1_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 15;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 3;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Wednesday;
        alarm_info1.trigger_time.hour = 12;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Thursday;
        alarm_info2.trigger_time.hour = 13;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_2_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_2_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_wday = 6;
        curr_time.tm_hour = 15;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 2;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};

        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Wednesday;
        alarm_info1.trigger_time.hour = 12;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Wednesday;
        alarm_info2.trigger_time.hour = 13;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_3_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_wday = 5;
        curr_time.tm_hour = 15;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 0;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};

        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Friday;
        alarm_info1.trigger_time.hour = 12;
        alarm_info1.trigger_time.min = 0;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Thursday;
        alarm_info2.trigger_time.hour = 13;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_2_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_4_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_wday = 5;
        curr_time.tm_hour = 15;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 3;  // Wed
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};

        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Everyday;
        alarm_info1.trigger_time.hour = 12;
        alarm_info1.trigger_time.min = 0;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Thursday;
        alarm_info2.trigger_time.hour = 13;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_5_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 11;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 3;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Wednesday;
        alarm_info1.trigger_time.hour = 12;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Tuesday;
        alarm_info2.trigger_time.hour = 13;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_6_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 11;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 3;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Everyday;
        alarm_info1.trigger_time.hour = 9;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 10;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_7_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 10;
        curr_time.tm_min = 15;
        curr_time.tm_wday = 6;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        ALARM_INFO alarm_info3 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 8;
        alarm_info2.trigger_time.min = 49;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        setup_alarm_time_info(&alarm_info3, &test_tm);
        alarm_info3.trigger_days = Everyday;
        alarm_info3.trigger_time.hour = 7;
        alarm_info3.trigger_time.min = 18;
        alarm_info3.alarm_text = (char*)TEST_ALARM_3_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info3);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(3);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_3_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_8_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 21;
        curr_time.tm_min = 28;
        curr_time.tm_wday = 6;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        ALARM_INFO alarm_info3 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday|Friday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info2.trigger_time.min = 12;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 4;
        alarm_info2.trigger_time.min = 49;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        setup_alarm_time_info(&alarm_info3, &test_tm);
        alarm_info3.trigger_days = Everyday;
        alarm_info3.trigger_time.hour = 21;
        alarm_info3.trigger_time.min = 30;
        alarm_info3.alarm_text = (char*)TEST_ALARM_3_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info3);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(3);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_3_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

static const ALARM_INFO test_alarm_list1[] =
{
    { {4, 50, 0}, 21, 0, "text_alarm_1", "", 0}
};

static const struct
{
    struct tm curr_time;
    bool should_be_null;
    const char* success_text;
    size_t alarm_cnt;
    const ALARM_INFO* alarm_info_list;
} test_alarm_value[] =
{
    {{0, 1, 6, 1, 0, 1900, 5, 0, 0}, false, "text_alarm_1", 1, test_alarm_list1}
};

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_array_success)
    {
        // arrange
        size_t num_elements = sizeof(test_alarm_value)/sizeof(test_alarm_value[0]);
        for (size_t index = 0; index < num_elements; index++)
        {
            SCHEDULER_HANDLE handle = alarm_scheduler_create();

            size_t alarm_cnt = test_alarm_value[index].alarm_cnt;
            for (size_t inner = 0; inner < alarm_cnt; inner++)
            {
                ALARM_INFO alarm_info = test_alarm_value[index].alarm_info_list[inner];
                (void)alarm_scheduler_add_alarm_info(handle, &alarm_info);
            }
            umock_c_reset_all_calls();

            struct tm time_value = test_alarm_value[index].curr_time;
            STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&time_value);
            STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(alarm_cnt);
            for (size_t inner = 0; inner < alarm_cnt; inner++)
            {
               STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
            }

            // act
            const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

            // assert
            if (test_alarm_value[index].should_be_null)
            {
                CTEST_ASSERT_IS_NULL(alarm_info, "Alarm Info should be NULL test %d/%d", (int)index, (int)num_elements);
            }
            else
            {
                CTEST_ASSERT_IS_NOT_NULL(alarm_info, "Alarm Info should not be NULL test %d/%d", (int)index, (int)num_elements);
                CTEST_ASSERT_ARE_EQUAL(char_ptr, test_alarm_value[index].success_text, alarm_info->alarm_text, "success text invalid test %d/%d", (int)index, (int)num_elements);
            }
            CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

            // cleanup
            alarm_scheduler_destroy(handle);
        }
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_cmp_min_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 11;
        curr_time.tm_min = 30;
        curr_time.tm_wday = 3;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Everyday;
        alarm_info1.trigger_time.hour = 9;
        alarm_info1.trigger_time.min = 10;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 9;
        alarm_info1.trigger_time.min = 20;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(2);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }


    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_no_day_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 10;
        curr_time.tm_min = 15;
        curr_time.tm_wday = 6;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        ALARM_INFO alarm_info3 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = NoDay;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_alarm_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 17;
        curr_time.tm_min = 8;
        curr_time.tm_wday = 5;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        ALARM_INFO alarm_info3 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 17;
        alarm_info2.trigger_time.hour = 10;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        setup_alarm_time_info(&alarm_info3, &test_tm);
        alarm_info3.trigger_days = Everyday;
        alarm_info3.trigger_time.hour = 21;
        alarm_info3.trigger_time.min = 45;
        alarm_info3.alarm_text = (char*)TEST_ALARM_3_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info3);

        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&curr_time);
        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(3);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* alarm_info = alarm_scheduler_get_next_alarm(handle);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(alarm_info);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_3_TEXT, alarm_info->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }


    CTEST_FUNCTION(alarm_scheduler_get_next_day_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        set_tm_struct(&test_tm);
        test_tm.tm_wday = 2;

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Tuesday|Thursday;
        alarm_info1.trigger_time.hour = 9;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&test_tm);

        // act
        int result = alarm_scheduler_get_next_day(&alarm_info1);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 4, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_day_2_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        set_tm_struct(&test_tm);
        test_tm.tm_wday = 5;
        test_tm.tm_hour = 10;

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Tuesday;
        alarm_info1.trigger_time.hour = 9;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&test_tm);

        // act
        int result = alarm_scheduler_get_next_day(&alarm_info1);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 2, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_day_3_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        set_tm_struct(&test_tm);
        test_tm.tm_hour = 21;
        test_tm.tm_min = 28;
        test_tm.tm_wday = 4;

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Everyday;
        alarm_info1.trigger_time.hour = 21;
        alarm_info1.trigger_time.min = 30;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&test_tm);

        // act
        int result = alarm_scheduler_get_next_day(&alarm_info1);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 4, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_day_4_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        set_tm_struct(&test_tm);
        test_tm.tm_hour = 5;
        test_tm.tm_min = 50;
        test_tm.tm_wday = 5;

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = 21;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;

        STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&test_tm);

        // act
        int result = alarm_scheduler_get_next_day(&alarm_info1);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 1, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_next_day_everyday_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        set_tm_struct(&test_tm);
        test_tm.tm_hour = 8;

        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Everyday;
        alarm_info1.trigger_time.hour = 9;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;;

        for (size_t index = 0; index < 7; index++)
        {
            test_tm.tm_wday = index;

            STRICT_EXPECTED_CALL(get_time_value()).SetReturn(&test_tm);

            // act
            int result = alarm_scheduler_get_next_day(&alarm_info1);

            // assert
            CTEST_ASSERT_ARE_EQUAL(int, index, result);

            umock_c_reset_all_calls();
        }

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_count_handle_NULL_fail)
    {
        // arrange

        // act
        size_t result = alarm_scheduler_get_alarm_count(NULL);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_count_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);

        // act
        size_t result = alarm_scheduler_get_alarm_count(handle);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 1, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        ALARM_INFO alarm_info1 = {0};

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = NoDay;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* result = alarm_scheduler_get_alarm(handle, 0);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, result->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_by_id_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = NoDay;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* result = alarm_scheduler_get_alarm_by_id(handle, 100);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_1_TEXT, result->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_by_id_handle_NULL_fail)
    {
        // arrange

        // act
        const ALARM_INFO* result = alarm_scheduler_get_alarm_by_id(NULL, 100);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_by_id_3_items_success)
    {
        // arrange
        struct tm curr_time = {0};
        curr_time.tm_hour = 17;
        curr_time.tm_min = 8;
        curr_time.tm_wday = 5;
        ALARM_INFO alarm_info1 = {0};
        ALARM_INFO alarm_info2 = {0};
        ALARM_INFO alarm_info3 = {0};
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = Monday|Wednesday;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.trigger_time.min = 50;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);

        setup_alarm_time_info(&alarm_info2, &test_tm);
        alarm_info2.trigger_days = Everyday;
        alarm_info2.trigger_time.hour = 17;
        alarm_info2.trigger_time.hour = 10;
        alarm_info2.alarm_text = (char*)TEST_ALARM_2_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info2);

        setup_alarm_time_info(&alarm_info3, &test_tm);
        alarm_info3.trigger_days = Everyday;
        alarm_info3.trigger_time.hour = 21;
        alarm_info3.trigger_time.min = 45;
        alarm_info3.alarm_text = (char*)TEST_ALARM_3_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info3);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(3);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* result = alarm_scheduler_get_alarm_by_id(handle, 101);

        // assert
        CTEST_ASSERT_IS_NOT_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, TEST_ALARM_2_TEXT, result->alarm_text);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_get_alarm_by_id_not_found_success)
    {
        // arrange
        struct tm test_tm = {0};
        ALARM_INFO alarm_info1 = {0};

        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&alarm_info1, &test_tm);
        alarm_info1.trigger_days = NoDay;
        alarm_info1.trigger_time.hour = 4;
        alarm_info1.alarm_text = (char*)TEST_ALARM_1_TEXT;
        (void)alarm_scheduler_add_alarm_info(handle, &alarm_info1);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(item_list_item_count(IGNORED_ARG)).SetReturn(1);
        STRICT_EXPECTED_CALL(item_list_get_item(IGNORED_ARG, IGNORED_ARG));

        // act
        const ALARM_INFO* result = alarm_scheduler_get_alarm_by_id(handle, 90);

        // assert
        CTEST_ASSERT_IS_NULL(result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_snooze_alarm_success)
    {
        // arrange
        struct tm test_tm = {0};
        set_tm_struct(&test_tm);
        SCHEDULER_HANDLE handle = alarm_scheduler_create();
        setup_alarm_time_info(&g_alarm_info, &test_tm);
        umock_c_reset_all_calls();

        setup_alarm_scheduler_add_alarm_info_mocks();

        // act
        int result = alarm_scheduler_snooze_alarm(handle, &g_alarm_info);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        alarm_scheduler_destroy(handle);
    }

    CTEST_FUNCTION(alarm_scheduler_is_morning_success)
    {
        // arrange
        TIME_INFO test_time = {0};
        test_time.hour = 12;

        // act
        bool result = alarm_scheduler_is_morning(&test_time);
        CTEST_ASSERT_IS_FALSE(result);

        test_time.hour = 13;
        result = alarm_scheduler_is_morning(&test_time);
        CTEST_ASSERT_IS_FALSE(result);

        test_time.hour = 11;
        result = alarm_scheduler_is_morning(&test_time);
        CTEST_ASSERT_IS_TRUE(result);

        // assert
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

CTEST_END_TEST_SUITE(alarm_scheduler_ut)
