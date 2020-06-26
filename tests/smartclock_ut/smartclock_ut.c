// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstddef>
#include <cstdlib>
#else
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#endif

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
#include "lib-util-c/file_mgr.h"
#include "lib-util-c/app_logging.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/thread_mgr.h"
#include "lib-util-c/crt_extensions.h"

#include "ntp_client.h"
#include "weather_client.h"
#include "config_mgr.h"
#include "alarm_scheduler.h"
#include "sound_mgr.h"
#include "gui_mgr.h"
#include "time_mgr.h"

#undef ENABLE_MOCKS

/**
 * Include the test tools.
 */
#include "ctest.h"
#include "umock_c/umock_c.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"

#include "smartclock.h"

static GUI_MGR_NOTIFICATION_CB g_gui_notification;
static void* g_gui_notification_ctx;
static ON_ALARM_LOAD_CALLBACK g_alarm_load_cb;
static void* g_alarm_cb_ctx;
static size_t g_iteration;
static size_t g_close_iteration;
static struct tm g_time_value = {0};

#ifdef __cplusplus
extern "C"
{
#endif

    int clone_string_with_size_format(char** target, const char* source, size_t source_len, const char* format, ...);

    int clone_string_with_size_format(char** target, const char* source, size_t source_len, const char* format, ...)
    {
        size_t len = strlen(source);
        *target = my_mem_shim_malloc(len+1);
        strncpy(*target, source, source_len);
        return 0;
    }

#ifdef __cplusplus
}
#endif

static CONFIG_MGR_HANDLE my_config_mgr_create(const char* config_path)
{
    (void)config_path;
    return (CONFIG_MGR_HANDLE)my_mem_shim_malloc(1);
}

static void my_config_mgr_destroy(CONFIG_MGR_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static int my_config_mgr_load_alarm(CONFIG_MGR_HANDLE handle, ON_ALARM_LOAD_CALLBACK alarm_cb, void* user_ctx)
{
    g_alarm_load_cb = alarm_cb;
    g_alarm_cb_ctx = user_ctx;
    return 0;
}

static SCHEDULER_HANDLE my_alarm_scheduler_create(void)
{
    return (SCHEDULER_HANDLE)my_mem_shim_malloc(1);
}

static void my_alarm_scheduler_destroy(SCHEDULER_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static SOUND_MGR_HANDLE my_sound_mgr_create(void)
{
    return (SOUND_MGR_HANDLE)my_mem_shim_malloc(1);
}

static void my_sound_mgr_destroy(SOUND_MGR_HANDLE handle)
{
    my_mem_shim_free(handle);
}

// static SOUND_MGR_HANDLE my_sound_mgr_create(void)
// {
//     return (SOUND_MGR_HANDLE)my_mem_shim_malloc(1);
// }

// static void my_sound_mgr_destroy(SOUND_MGR_HANDLE handle)
// {
//     my_mem_shim_free(handle);
// }

static GUI_MGR_HANDLE my_gui_mgr_create(CONFIG_MGR_HANDLE config_mgr, GUI_MGR_NOTIFICATION_CB notify_cb, void* user_ctx)
{
    g_gui_notification = notify_cb;
    g_gui_notification_ctx = user_ctx;
    return (GUI_MGR_HANDLE)my_mem_shim_malloc(1);
}

static void my_gui_mgr_destroy(GUI_MGR_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static void my_gui_mgr_process_items(GUI_MGR_HANDLE handle)
{
    (void)handle;
}

static NTP_CLIENT_HANDLE my_ntp_client_create(void)
{
    return (NTP_CLIENT_HANDLE)my_mem_shim_malloc(1);
}

static void my_ntp_client_destroy(NTP_CLIENT_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static WEATHER_CLIENT_HANDLE my_weather_client_create(const char* api_key, TEMPERATURE_UNITS units)
{
    return (WEATHER_CLIENT_HANDLE)my_mem_shim_malloc(1);
}

static void my_weather_client_destroy(WEATHER_CLIENT_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static void my_thread_mgr_sleep(size_t milliseconds)
{
    (void)milliseconds;
    g_iteration++;
    if (g_close_iteration >= g_iteration)
    {
        g_gui_notification(g_gui_notification_ctx, NOTIFICATION_APPLICATION_RESULT, NULL);
    }
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    CTEST_ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

CTEST_BEGIN_TEST_SUITE(smartclock_ut)

    CTEST_SUITE_INITIALIZE()
    {
        int result;
        (void)umock_c_init(on_umock_c_error);

        REGISTER_UMOCK_ALIAS_TYPE(CONFIG_MGR_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(SCHEDULER_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(SOUND_MGR_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(NTP_CLIENT_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(WEATHER_CLIENT_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ALARM_TIMER_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(GUI_MGR_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(GUI_MGR_NOTIFICATION_CB, void*);
        REGISTER_UMOCK_ALIAS_TYPE(ON_ALARM_LOAD_CALLBACK, void*);
        REGISTER_UMOCK_ALIAS_TYPE(TEMPERATURE_UNITS, int);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_HOOK(config_mgr_create, my_config_mgr_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(config_mgr_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(config_mgr_destroy, my_config_mgr_destroy);
        REGISTER_GLOBAL_MOCK_HOOK(config_mgr_load_alarm, my_config_mgr_load_alarm);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(config_mgr_load_alarm, __LINE__);

        REGISTER_GLOBAL_MOCK_HOOK(alarm_scheduler_create, my_alarm_scheduler_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alarm_scheduler_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(alarm_scheduler_destroy, my_alarm_scheduler_destroy);

        REGISTER_GLOBAL_MOCK_RETURN(alarm_timer_init, 0);
        REGISTER_GLOBAL_MOCK_HOOK(thread_mgr_sleep, my_thread_mgr_sleep);
        REGISTER_GLOBAL_MOCK_RETURN(get_time_value, &g_time_value);

        REGISTER_GLOBAL_MOCK_HOOK(sound_mgr_create, my_sound_mgr_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(sound_mgr_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(sound_mgr_destroy, my_sound_mgr_destroy);

        REGISTER_GLOBAL_MOCK_HOOK(gui_mgr_create, my_gui_mgr_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(gui_mgr_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(gui_mgr_destroy, my_gui_mgr_destroy);
        REGISTER_GLOBAL_MOCK_RETURN(gui_mgr_create_win, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(gui_mgr_create_win, __LINE__);
        REGISTER_GLOBAL_MOCK_HOOK(gui_mgr_process_items, my_gui_mgr_process_items);

        REGISTER_GLOBAL_MOCK_HOOK(ntp_client_create, my_ntp_client_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(ntp_client_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(ntp_client_destroy, my_ntp_client_destroy);

        REGISTER_GLOBAL_MOCK_HOOK(weather_client_create, my_weather_client_create);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(weather_client_create, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(weather_client_destroy, my_weather_client_destroy);

        result = umocktypes_charptr_register_types();
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
    }

    CTEST_SUITE_CLEANUP()
    {
        umock_c_deinit();
    }

    CTEST_FUNCTION_INITIALIZE()
    {
        umock_c_reset_all_calls();
        g_gui_notification = NULL;
        g_gui_notification_ctx = NULL;
        g_close_iteration = g_iteration = 0;
        g_time_value.tm_hour = 11;
        g_time_value.tm_min = 30;
        g_time_value.tm_year = 109;
    }

    CTEST_FUNCTION_CLEANUP()
    {
    }

    static void setup_initialize_mocks(void)
    {
        STRICT_EXPECTED_CALL(alarm_scheduler_create());
        STRICT_EXPECTED_CALL(sound_mgr_create());
        STRICT_EXPECTED_CALL(gui_mgr_create(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(ntp_client_create());
        STRICT_EXPECTED_CALL(weather_client_create(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(alarm_timer_init(IGNORED_ARG)).CallCannotFail();
    }

    static void setup_check_ntp_operation_mocks(void)
    {
        STRICT_EXPECTED_CALL(alarm_timer_is_expired(IGNORED_ARG));
    }

    static void setup_check_weather_operation_mocks(void)
    {
        STRICT_EXPECTED_CALL(alarm_timer_is_expired(IGNORED_ARG));
    }

    static void setup_check_alarm_operation_mocks(const ALARM_INFO* triggered)
    {
        STRICT_EXPECTED_CALL(get_time_value());
        STRICT_EXPECTED_CALL(alarm_scheduler_is_triggered(IGNORED_ARG, IGNORED_ARG)).SetReturn(triggered);
        if (triggered != NULL)
        {
            STRICT_EXPECTED_CALL(gui_mgr_set_alarm_triggered(IGNORED_ARG, IGNORED_ARG));
            STRICT_EXPECTED_CALL(config_mgr_get_audio_dir(IGNORED_ARG));
            STRICT_EXPECTED_CALL(sound_mgr_play(IGNORED_ARG, IGNORED_ARG, true, true));
            STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, IGNORED_ARG));

            STRICT_EXPECTED_CALL(alarm_timer_is_expired(IGNORED_ARG));
            STRICT_EXPECTED_CALL(gui_mgr_set_alarm_triggered(IGNORED_ARG, IGNORED_ARG));
            STRICT_EXPECTED_CALL(alarm_scheduler_get_next_alarm(IGNORED_ARG));
            STRICT_EXPECTED_CALL(gui_mgr_set_next_alarm(IGNORED_ARG, IGNORED_ARG));
        }
    }

    static void setup_cleanup_mocks(void)
    {
        STRICT_EXPECTED_CALL(ntp_client_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(sound_mgr_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_scheduler_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(config_mgr_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(weather_client_destroy(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
    }

    CTEST_FUNCTION(run_application_1_loop_succeed)
    {
        // arrange
        int argc = 3;
        char* argv[] = {
            "/usr/bin/smartclock_exe",
            "--weather_appid",
            "1a2b3c4d5e6f7g8h9i0j"
            };

        STRICT_EXPECTED_CALL(config_mgr_create(IGNORED_ARG));
        setup_initialize_mocks();
        STRICT_EXPECTED_CALL(config_mgr_load_alarm(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_create_win(IGNORED_ARG));
        STRICT_EXPECTED_CALL(get_time_value());
        STRICT_EXPECTED_CALL(config_mgr_get_shade_times(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(config_mgr_is_demo_mode(IGNORED_ARG));
        setup_check_ntp_operation_mocks();
        setup_check_weather_operation_mocks();
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_timer_start(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alarm_scheduler_get_next_alarm(IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_set_next_alarm(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_get_refresh_resolution());
        STRICT_EXPECTED_CALL(get_time_value());
        setup_check_ntp_operation_mocks();
        setup_check_weather_operation_mocks();
        setup_check_alarm_operation_mocks(NULL);
        STRICT_EXPECTED_CALL(gui_mgr_set_time_item(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_process_items(IGNORED_ARG));
        STRICT_EXPECTED_CALL(thread_mgr_sleep(IGNORED_ARG));
        setup_cleanup_mocks();

        // act
        g_close_iteration = 1;
        int result = run_application(argc, argv);

        // assert
        CTEST_ASSERT_ARE_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(run_application_no_arg_fail)
    {
        // arrange
        int argc = 1;
        char* argv[] = {
            "/usr/bin/smartclock_exe"
            };

        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        int result = run_application(argc, argv);

        // assert
        CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result);
        CTEST_ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    CTEST_FUNCTION(run_application_fail)
    {
        // arrange
        int argc = 3;
        char* argv[] = {
            "/usr/bin/smartclock_exe",
            "--weather_appid",
            "1a2b3c4d5e6f7g8h9i0j"
            };

        int negativeTestsInitResult = umock_c_negative_tests_init();
        CTEST_ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        STRICT_EXPECTED_CALL(config_mgr_create(IGNORED_ARG));
        setup_initialize_mocks();
        STRICT_EXPECTED_CALL(config_mgr_load_alarm(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(gui_mgr_create_win(IGNORED_ARG));

        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                g_close_iteration = 1;
                int result = run_application(argc, argv);

                // assert
                CTEST_ASSERT_ARE_NOT_EQUAL(int, 0, result, "Failure in item %d", (int)index);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

CTEST_END_TEST_SUITE(smartclock_ut)
