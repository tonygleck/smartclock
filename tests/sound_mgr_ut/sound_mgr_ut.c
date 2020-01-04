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

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>

MOCKABLE_FUNCTION(, const ALCchar*, alcGetString, ALCdevice*, device, ALCenum, param);
MOCKABLE_FUNCTION(, const ALchar*, alGetString, ALenum, param);

MOCKABLE_FUNCTION(, ALCdevice*, alcOpenDevice, const ALCchar*, devicename);
MOCKABLE_FUNCTION(, ALCboolean, alcCloseDevice, ALCdevice*, device);

MOCKABLE_FUNCTION(, ALCcontext*, alcCreateContext, ALCdevice*, device, const ALCint*, attrlist);
MOCKABLE_FUNCTION(, ALCboolean, alcMakeContextCurrent, ALCcontext*, context);
MOCKABLE_FUNCTION(, void, alcDestroyContext, ALCcontext*, context);

MOCKABLE_FUNCTION(, void, alGenBuffers, ALsizei, n, ALuint*, buffers);
MOCKABLE_FUNCTION(, void, alDeleteBuffers, ALsizei, n, const ALuint*, buffers);

MOCKABLE_FUNCTION(, void, alBufferData, ALuint, buffer, ALenum, format, const ALvoid*, data, ALsizei, size, ALsizei, freq);

MOCKABLE_FUNCTION(, void, alGenSources, ALsizei, n, ALuint*, sources);
MOCKABLE_FUNCTION(, void, alDeleteSources, ALsizei, n, const ALuint*, sources);

MOCKABLE_FUNCTION(, void, alSourcef, ALuint, source, ALenum, param, ALfloat, value);
MOCKABLE_FUNCTION(, void, alSourcefv, ALuint, source, ALenum, param, const ALfloat*, values);
MOCKABLE_FUNCTION(, void, alSourcei, ALuint, source, ALenum, param, ALint, value);

MOCKABLE_FUNCTION(, void, alListenerfv, ALenum, param, const ALfloat*, values);

MOCKABLE_FUNCTION(, void, alSourcePlay, ALuint, source);
MOCKABLE_FUNCTION(, void, alSourceStop, ALuint, source);

MOCKABLE_FUNCTION(, ALenum, alGetError);
#undef ENABLE_MOCKS

/**
 * Include the test tools.
 */
#include "testrunnerswitcher.h"
#include "umock_c/umock_c.h"
//#include "umock_c_prod.h"

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"

#include "sound_mgr.h"

#define ENABLE_MOCKS
#undef ENABLE_MOCKS

static const char* TEST_DEVICE_NAME = "test_device_name";

#define TEST_xio_socket_INTERFACE_DESCRIPTION     (const IO_INTERFACE_DESCRIPTION*)0x4242

static ALCdevice* my_alcOpenDevice(const ALCchar *devicename)
{
    return (ALCdevice*)my_mem_shim_malloc(1);
}

static ALCboolean my_alcCloseDevice(ALCdevice* device)
{
    my_mem_shim_free(device);
    return 1;
}

static ALCcontext* my_alcCreateContext(ALCdevice *device, const ALCint* attrlist)
{
    return (ALCcontext*)my_mem_shim_malloc(1);
}

static void my_alcDestroyContext(ALCcontext *context)
{
    my_mem_shim_free(context);
}

MU_DEFINE_ENUM_STRINGS(UMOCK_C_ERROR_CODE, UMOCK_C_ERROR_CODE_VALUES)
static void on_umock_c_error(UMOCK_C_ERROR_CODE error_code)
{
    ASSERT_FAIL("umock_c reported error :%s", MU_ENUM_TO_STRING(UMOCK_C_ERROR_CODE, error_code));
}

static TEST_MUTEX_HANDLE g_testByTest;

BEGIN_TEST_SUITE(sound_mgr_ut)

    TEST_SUITE_INITIALIZE(suite_init)
    {
        int result;
        g_testByTest = TEST_MUTEX_CREATE();
        ASSERT_IS_NOT_NULL(g_testByTest);

        (void)umock_c_init(on_umock_c_error);

        //REGISTER_TYPE(IO_OPEN_RESULT, IO_OPEN_RESULT);
        REGISTER_UMOCK_ALIAS_TYPE(ALCenum, int);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_RETURN(alcGetString, TEST_DEVICE_NAME);
        REGISTER_GLOBAL_MOCK_HOOK(alcOpenDevice, my_alcOpenDevice);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcOpenDevice, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(alcCloseDevice, my_alcCloseDevice);
        REGISTER_GLOBAL_MOCK_HOOK(alcCreateContext, my_alcCreateContext);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcCreateContext, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(alcDestroyContext, my_alcDestroyContext);
        REGISTER_GLOBAL_MOCK_RETURN(alcMakeContextCurrent, 1);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcMakeContextCurrent, 0);

        result = umocktypes_charptr_register_types();
        ASSERT_ARE_EQUAL(int, 0, result);
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

    static void setup_sound_mgr_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_NUM_ARG));
        STRICT_EXPECTED_CALL(alcGetString(IGNORED_PTR_ARG, IGNORED_NUM_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(alcOpenDevice(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alcCreateContext(IGNORED_PTR_ARG, NULL));
        STRICT_EXPECTED_CALL(alcMakeContextCurrent(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alGetError()).CallCannotFail();
    }

    TEST_FUNCTION(sound_mgr_create_succeed)
    {
        // arrange
        setup_sound_mgr_create_mocks();

        // act
        SOUND_MGR_HANDLE handle = sound_mgr_create();

        // assert
        ASSERT_IS_NOT_NULL(handle);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_create_fail)
    {
        // arrange
        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_sound_mgr_create_mocks();
        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                SOUND_MGR_HANDLE handle = sound_mgr_create();

                // assert
                ASSERT_IS_NULL(handle, "sound_mgr_create failure %zu/%zu", index, count);
            }
        }
        // cleanup
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(sound_mgr_destroy_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        //STRICT_EXPECTED_CALL(alDeleteSources(IGNORED_NUM_ARG, IGNORED_PTR_ARG));
        //STRICT_EXPECTED_CALL(alDeleteBuffers(IGNORED_NUM_ARG, IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alcMakeContextCurrent(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alcDestroyContext(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(alcCloseDevice(IGNORED_PTR_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_PTR_ARG));

        // act
        sound_mgr_destroy(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

END_TEST_SUITE(sound_mgr_ut)
