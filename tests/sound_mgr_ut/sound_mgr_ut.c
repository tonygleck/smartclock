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

#include "umock_c/umocktypes_charptr.h"
#include "umock_c/umock_c_negative_tests.h"
#include "azure_macro_utils/macro_utils.h"

#include "sound_mgr.h"

#define ENABLE_MOCKS
#undef ENABLE_MOCKS

static const char* TEST_DEVICE_NAME = "test_device_name";
static const char* TEST_SOUND_FILE = "test_sound_file";

/* chunk size 2084*/
static const unsigned char TEST_WAV_FILE[] = {
    0x52, 0x49, 0x46, 0x46, 0x24, 0x08, 0x00, 0x00,
    0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x22, 0x56, 0x00, 0x00, 0x88, 0x58, 0x01, 0x00,
    0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x24, 0x17, 0x1e, 0xf3, 0x3c, 0x13, 0x3c, 0x14,
    0x16, 0xf9, 0x18, 0xf9, 0x34, 0xe7, 0x23, 0xa6,
    0x3c, 0xf2, 0x24, 0xf2, 0x11, 0xce, 0x1a, 0x0d
};
static size_t g_buffer_values = 0;
static size_t g_source_values = 0;

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

static void my_alGenBuffers(ALsizei n, ALuint *buffers)
{
    g_buffer_values++;
    *buffers = g_buffer_values;
    //ALuint* temp = (ALuint*)my_mem_shim_malloc(1);
    //*temp = 1;
    //buffers = temp;
}

static void my_alDeleteBuffers(ALsizei n, const ALuint *buffers)
{
    //my_mem_shim_free((void*)buffers);
    g_source_values--;
}

static void my_alGenSources(ALsizei n, ALuint *sources)
{
    g_source_values++;
    *sources = g_source_values;
}

static void my_alDeleteSources(ALsizei n, const ALuint *sources)
{
    //my_mem_shim_free((void*)sources);
    g_source_values--;
}

static FILE_MGR_HANDLE my_file_mgr_open(const char* filename, const char* param)
{
    return (FILE_MGR_HANDLE)my_mem_shim_malloc(1);
}

static void my_file_mgr_close(FILE_MGR_HANDLE handle)
{
    my_mem_shim_free(handle);
}

static size_t my_file_mgr_read(FILE_MGR_HANDLE handle, unsigned char* buffer, size_t read_len)
{
    size_t test_wav_file_len = sizeof(TEST_WAV_FILE)/sizeof(TEST_WAV_FILE[0]);
    size_t total_size = read_len < test_wav_file_len ? read_len : test_wav_file_len;
    memcpy(buffer, TEST_WAV_FILE, total_size);
    return total_size;
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
        REGISTER_UMOCK_ALIAS_TYPE(ALenum, int);
        REGISTER_UMOCK_ALIAS_TYPE(ALsizei, int);
        REGISTER_UMOCK_ALIAS_TYPE(ALuint, int);
        REGISTER_UMOCK_ALIAS_TYPE(ALint, int);
        REGISTER_UMOCK_ALIAS_TYPE(ALfloat, float);
        REGISTER_UMOCK_ALIAS_TYPE(FILE_MGR_HANDLE, void*);
        REGISTER_UMOCK_ALIAS_TYPE(SOUND_MGR_STATE, int);

        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_malloc, my_mem_shim_malloc);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(mem_shim_malloc, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(mem_shim_free, my_mem_shim_free);

        REGISTER_GLOBAL_MOCK_RETURN(alcGetString, TEST_DEVICE_NAME);
        REGISTER_GLOBAL_MOCK_HOOK(alcOpenDevice, my_alcOpenDevice);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcOpenDevice, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(alcCloseDevice, my_alcCloseDevice);
        REGISTER_GLOBAL_MOCK_HOOK(alcCreateContext, my_alcCreateContext);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcCreateContext, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(alDeleteSources, my_alDeleteSources);
        REGISTER_GLOBAL_MOCK_HOOK(alcDestroyContext, my_alcDestroyContext);
        REGISTER_GLOBAL_MOCK_HOOK(alGenSources, my_alGenSources);
        REGISTER_GLOBAL_MOCK_RETURN(alGetError, 0);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alGetError, __LINE__);
        REGISTER_GLOBAL_MOCK_RETURN(alGetString, "AL error String Text");
        REGISTER_GLOBAL_MOCK_HOOK(alGenBuffers, my_alGenBuffers);

        REGISTER_GLOBAL_MOCK_HOOK(alDeleteBuffers, my_alDeleteBuffers);

        REGISTER_GLOBAL_MOCK_RETURN(alcMakeContextCurrent, 1);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(alcMakeContextCurrent, 0);

        REGISTER_GLOBAL_MOCK_RETURN(file_mgr_get_length, sizeof(TEST_WAV_FILE)/sizeof(TEST_WAV_FILE[0]));
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(file_mgr_get_length, 0);
        REGISTER_GLOBAL_MOCK_HOOK(file_mgr_open, my_file_mgr_open);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(file_mgr_open, NULL);
        REGISTER_GLOBAL_MOCK_HOOK(file_mgr_close, my_file_mgr_close);
        REGISTER_GLOBAL_MOCK_HOOK(file_mgr_read, my_file_mgr_read);
        REGISTER_GLOBAL_MOCK_FAIL_RETURN(file_mgr_read, __LINE__);

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
        g_buffer_values = 0;
        g_source_values = 0;
    }

    TEST_FUNCTION_CLEANUP(function_cleanup)
    {
        TEST_MUTEX_RELEASE(g_testByTest);
    }

    static void setup_validate_al_error_mocks(bool failure)
    {
        STRICT_EXPECTED_CALL(alGetError());
        if (failure)
        {
            STRICT_EXPECTED_CALL(alGetString(IGNORED_ARG));
        }
    }

    static void setup_sound_mgr_create_mocks(void)
    {
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alcGetString(IGNORED_ARG, IGNORED_ARG)).CallCannotFail();
        STRICT_EXPECTED_CALL(alcOpenDevice(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alcCreateContext(IGNORED_ARG, NULL));
        STRICT_EXPECTED_CALL(alcMakeContextCurrent(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alGetError()).CallCannotFail();
    }

    static void setup_construct_data_buffer_mocks(bool failure)
    {
        STRICT_EXPECTED_CALL(alGenBuffers(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alBufferData(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        setup_validate_al_error_mocks(failure);
        STRICT_EXPECTED_CALL(alGenSources(IGNORED_ARG, IGNORED_ARG));
        setup_validate_al_error_mocks(failure);
        STRICT_EXPECTED_CALL(alSourcei(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        setup_validate_al_error_mocks(failure);
        STRICT_EXPECTED_CALL(alSourcefv(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alSourcefv(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alSourcef(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alSourcef(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alSourcei(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alListenerfv(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alListenerfv(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alListenerfv(IGNORED_ARG, IGNORED_ARG));
    }

    static void setup_play_mocks(bool failure)
    {
        STRICT_EXPECTED_CALL(file_mgr_open(TEST_SOUND_FILE, "rb"));
        STRICT_EXPECTED_CALL(file_mgr_get_length(IGNORED_ARG));
        STRICT_EXPECTED_CALL(malloc(IGNORED_ARG));
        STRICT_EXPECTED_CALL(file_mgr_read(IGNORED_ARG, IGNORED_ARG, IGNORED_ARG)).
            CopyOutArgumentBuffer_buffer(TEST_WAV_FILE, sizeof(unsigned char**));
        STRICT_EXPECTED_CALL(file_mgr_close(IGNORED_ARG));

        setup_construct_data_buffer_mocks(failure);

        // Construct Data buffer
        STRICT_EXPECTED_CALL(alSourcePlay(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));
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

    TEST_FUNCTION(sound_mgr_destroy_handle_NULL_succeed)
    {
        // arrange

        // act
        sound_mgr_destroy(NULL);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(sound_mgr_destroy_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        //STRICT_EXPECTED_CALL(alDeleteSources(IGNORED_ARG, IGNORED_ARG));
        //STRICT_EXPECTED_CALL(alDeleteBuffers(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alcMakeContextCurrent(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alcDestroyContext(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alcCloseDevice(IGNORED_ARG));
        STRICT_EXPECTED_CALL(free(IGNORED_ARG));

        // act
        sound_mgr_destroy(handle);

        // assert
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(sound_mgr_play_handle_NULL_fail)
    {
        // arrange

        // act
        int result = sound_mgr_play(NULL, TEST_SOUND_FILE, true);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(sound_mgr_play_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        setup_play_mocks(false);

        // act
        int result = sound_mgr_play(handle, TEST_SOUND_FILE, true);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_stop(handle);
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_play_fail)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        int negativeTestsInitResult = umock_c_negative_tests_init();
        ASSERT_ARE_EQUAL(int, 0, negativeTestsInitResult);

        setup_play_mocks(false);
        umock_c_negative_tests_snapshot();

        size_t count = umock_c_negative_tests_call_count();
        for (size_t index = 0; index < count; index++)
        {
            if (umock_c_negative_tests_can_call_fail(index))
            {
                umock_c_negative_tests_reset();
                umock_c_negative_tests_fail_call(index);

                // act
                int result = sound_mgr_play(handle, TEST_SOUND_FILE, true);

                // assert
                ASSERT_ARE_NOT_EQUAL(int, 0, result, "sound_mgr_play failure %d/%d", (int)index, (int)count);
            }
        }
        // cleanup
        sound_mgr_destroy(handle);
        umock_c_negative_tests_deinit();
    }

    TEST_FUNCTION(sound_mgr_stop_handle_NULL_fail)
    {
        // arrange

        // act
        int result = sound_mgr_stop(NULL);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
    }

    TEST_FUNCTION(sound_mgr_stop_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        int result = sound_mgr_play(handle, TEST_SOUND_FILE, true);
        umock_c_reset_all_calls();

        STRICT_EXPECTED_CALL(alSourceStop(IGNORED_ARG));
        STRICT_EXPECTED_CALL(alDeleteSources(IGNORED_ARG, IGNORED_ARG));
        STRICT_EXPECTED_CALL(alDeleteBuffers(IGNORED_ARG, IGNORED_ARG));

        // act
        result = sound_mgr_stop(handle);

        // assert
        ASSERT_ARE_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_stop_not_playing_fail)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        // act
        int result = sound_mgr_stop(handle);

        // assert
        ASSERT_ARE_NOT_EQUAL(int, 0, result);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_get_current_state_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        int result = sound_mgr_play(handle, TEST_SOUND_FILE, true);
        umock_c_reset_all_calls();

        // act
        SOUND_MGR_STATE state = sound_mgr_get_current_state(handle);

        // assert
        ASSERT_ARE_EQUAL(int, SOUND_STATE_PLAYING, state);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_stop(handle);
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_get_current_state_not_playing_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        umock_c_reset_all_calls();

        // act
        SOUND_MGR_STATE state = sound_mgr_get_current_state(handle);

        // assert
        ASSERT_ARE_EQUAL(int, SOUND_STATE_IDLE, state);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_destroy(handle);
    }

    TEST_FUNCTION(sound_mgr_get_current_state_stopped_succeed)
    {
        // arrange
        SOUND_MGR_HANDLE handle = sound_mgr_create();
        int result = sound_mgr_play(handle, TEST_SOUND_FILE, true);
        sound_mgr_stop(handle);
        umock_c_reset_all_calls();

        // act
        SOUND_MGR_STATE state = sound_mgr_get_current_state(handle);

        // assert
        ASSERT_ARE_EQUAL(int, SOUND_STATE_IDLE, state);
        ASSERT_ARE_EQUAL(char_ptr, umock_c_get_expected_calls(), umock_c_get_actual_calls());

        // cleanup
        sound_mgr_destroy(handle);
    }

END_TEST_SUITE(sound_mgr_ut)
