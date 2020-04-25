// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifdef __cplusplus
#include <cstddef>
#include <cstdlib>
#else
#include <stddef.h>
#include <stdlib.h>
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

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        //REGISTER_GLOBAL_MOCK_RETURN(file_mgr_get_length, sizeof(TEST_WAV_FILE)/sizeof(TEST_WAV_FILE[0]));
        //REGISTER_GLOBAL_MOCK_HOOK(file_mgr_read, my_file_mgr_read);
        //REGISTER_GLOBAL_MOCK_FAIL_RETURN(file_mgr_read, __LINE__);

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
    }

    CTEST_FUNCTION_CLEANUP()
    {
    }


    CTEST_FUNCTION(sound_mgr_create_succeed)
    {
        // arrange

        // act

        // assert

        // cleanup
    }


CTEST_END_TEST_SUITE(smartclock_ut)
