
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/app_logging.h"
#include "lib-util-c/crt_extensions.h"
#include "lib-util-c/file_mgr.h"
#include "sound_mgr.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alut.h>

typedef struct SOUND_MGR_INFO_TAG
{
    SOUND_MGR_STATE sound_state;
    ALCdevice* alc_open_dev;
    ALCcontext* alc_context;
    const unsigned char* wav_data;
    size_t wav_size;

    ALuint source;
    ALuint sound_buf;
} SOUND_MGR_INFO;

#define RIFF_ID     0x46464952 // 'RIFF'
#define FMT_ID      0x20746D66 // 'FMT '
#define DATA_ID     0x61746164 // 'DATA'

#define SWAP_32(value)                 \
        (((((unsigned short)value)<<8) & 0xFF00)   | \
         ((((unsigned short)value)>>8) & 0x00FF))

#define SWAP_16(value)                     \
        (((((unsigned int)value)<<24) & 0xFF000000)  | \
         ((((unsigned int)value)<< 8) & 0x00FF0000)  | \
         ((((unsigned int)value)>> 8) & 0x0000FF00)  | \
         ((((unsigned int)value)>>24) & 0x000000FF))

typedef struct chunk_header_tag
{
    int id;
    int size;
} chunk_header;

// WAVE file format info.  We pass this through to OpenAL so we can support mono/stereo, 8/16/bit, etc.
typedef struct format_info_tag
{
    short format; // PCM = 1, not sure what other values are legal.
    short num_channels;
    int sample_rate;
    int byte_rate;
    short block_align;
    short bits_per_sample;
} format_info;

static bool validate_openal_error(const char* err_text)
{
    bool result;
    ALenum al_error = alGetError();
    if (AL_NO_ERROR != al_error)
    {
        log_error("%s: %s", err_text, alGetString(al_error));
        result = false;
    }
    else
    {
        result = true;
    }
    return result;
}

static int initialize_openai(SOUND_MGR_INFO* sound_info)
{
    int result;
    const char* dev_name = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    if ((sound_info->alc_open_dev = alcOpenDevice(dev_name)) == NULL)
    {
        log_error("Failure opening default device %s", dev_name);
        result = __LINE__;
    }
    else if ((sound_info->alc_context = alcCreateContext(sound_info->alc_open_dev, NULL)) == NULL)
    {
        alcCloseDevice(sound_info->alc_open_dev);
        log_error("Failure creating device context");
        result = __LINE__;
    }
    else if (!alcMakeContextCurrent(sound_info->alc_context))
    {
        alcDestroyContext(sound_info->alc_context);
        alcCloseDevice(sound_info->alc_open_dev);
        log_error("Failure making device context default");
        result = __LINE__;
    }
    else
    {
        // Capture errors
        alGetError();
        result = 0;
    }
    return result;
}

static void deinitialize_openai(SOUND_MGR_INFO* sound_info)
{
    //ALCdevice* device = alcGetContextsDevice(sound_info->alc_context);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(sound_info->alc_context);
    //alcCloseDevice(device);
    alcCloseDevice(sound_info->alc_open_dev);
}

// This utility returns the start of data for a chunk given a range of bytes it might be within.  Pass 1 for
// swapped if the machine is not the same endian as the file.
static const char* find_data_chunk(const unsigned char* file_begin, const unsigned char* file_end, int desired_id, int swapped)
{
    const unsigned char* result = NULL;
    while (file_begin < file_end)
    {
        chunk_header* h = (chunk_header *)file_begin;
        if (h->id == desired_id && !swapped)
        {
            result = file_begin + sizeof(chunk_header);
            break;
        }
        if (h->id == SWAP_32(desired_id) && swapped)
        {
            result = file_begin + sizeof(chunk_header);
            break;
        }
        int chunk_size = swapped ? SWAP_32(h->size) : h->size;
        const unsigned char *next = file_begin + chunk_size + sizeof(chunk_header);
        if (next > file_end || next <= file_begin)
        {
            break;
        }
        file_begin = next;
    }
    return (const char*)result;
}

// Given a chunk, find its end by going back to the header.
static const char* chunk_end(const char* chunk_start, int swapped)
{
    chunk_header* h = (chunk_header*)(chunk_start - sizeof(chunk_header));
    return chunk_start + (swapped ? SWAP_32(h->size) : h->size);
}

static int validate_wav_data(unsigned char* wav_buffer, long wav_size, format_info* fmt_info, const unsigned char** wav_data, int* swapped)
{
    int result;

    // Find the RIFF chunk.  Note that by searching for RIFF both normal
    // and reversed, we can automatically determine the endian swap situation for
    // this file regardless of what machine we are on.
    const unsigned char* wav_end = wav_buffer+wav_size;
    const char* riff = find_data_chunk(wav_buffer, wav_end, RIFF_ID, 0);
    if (riff == NULL)
    {
        riff = find_data_chunk(wav_buffer, wav_end, RIFF_ID, 1);
        if (riff)
        {
            *swapped = 1;
            result = 0;
        }
        else
        {
            log_error("Could not find RIFF chunk in wave file.\n");
            result = __LINE__;
        }
    }
    else
    {
        *swapped = 0;
        result = 0;
    }

    if (result == 0)
    {
        const char* format;

        // The wave chunk isn't really a chunk at all. :-(  It's just a "WAVE" tag
        // followed by more chunks.  This strikes me as totally inconsistent, but
        // anyway, confirm the WAVE ID and move on.
        if (riff[0] != 'W' || riff[1] != 'A' || riff[2] != 'V' || riff[3] != 'E')
        {
            log_error("Could not find WAVE signature in wave file");
            result = __LINE__;
        }
        else if ((format = find_data_chunk((const unsigned char*)riff+4, (const unsigned char*)chunk_end(riff, *swapped), FMT_ID, *swapped)) == NULL)
        {
            log_error("Could not find FMT chunk in wave file");
            result = __LINE__;
        }
        else
        {
            memcpy(fmt_info, format, sizeof(format_info));
            if (*swapped)
            {
                fmt_info->format = SWAP_16(fmt_info->format);
                fmt_info->num_channels = SWAP_16(fmt_info->num_channels);
                fmt_info->sample_rate = SWAP_32(fmt_info->sample_rate);
                fmt_info->byte_rate = SWAP_32(fmt_info->byte_rate);
                fmt_info->block_align = SWAP_16(fmt_info->block_align);
                fmt_info->bits_per_sample = SWAP_16(fmt_info->bits_per_sample);
            }

            // Reject things we don't understand...expand this code to support weirder audio formats.
            if (fmt_info->format != 1)
            {
                log_error("Wave file is not PCM format data");
                result = __LINE__;
            }
            else if (fmt_info->num_channels != 1 && fmt_info->num_channels != 2)
            {
                log_error("Must have mono or stereo sound");
                result = __LINE__;
            }
            else if (fmt_info->bits_per_sample != 8 && fmt_info->bits_per_sample != 16)
            {
                log_error("Must have 8 or 16 bit sounds");
                result = __LINE__;
            }
            else if ((*wav_data = (const unsigned char*)find_data_chunk((const unsigned char*)riff+4, (const unsigned char*)chunk_end(riff, *swapped), DATA_ID, *swapped)) == NULL)
            {
                log_error("Could not find the DATA chunk");
                result = __LINE__;
            }
            else
            {
                result = 0;
            }
        }
    }
    return result;
}

static unsigned char* retrieve_wav_data(const char* filename, long* wav_size)
{
    unsigned char* result;
    FILE_MGR_HANDLE file_mgr;
    if ((file_mgr = file_mgr_open(filename, "rb")) == NULL)
    {
        log_error("Failure loading wav");
        result = NULL;
    }
    else
    {
        if ((*wav_size = file_mgr_get_length(file_mgr)) == 0)
        {
            log_error("Invalid file size found");
            result = NULL;
        }
        else if ((result = (unsigned char*)malloc(*wav_size)) == NULL)
        {
            log_error("Failure allocating wav file");
            result = NULL;
        }
        else if (file_mgr_read(file_mgr, result, *wav_size) != (size_t)*wav_size)
        {
            log_error("Failure reading wav file");
            free(result);
            result = NULL;
        }
        file_mgr_close(file_mgr);
    }
    return result;
}

static int construct_buffer_data(SOUND_MGR_INFO* sound_info, format_info* fmt_info, bool repeat)
{
    int result;

    //ALenum error;
    alGenBuffers(1, &sound_info->sound_buf);
    if (sound_info->sound_buf == 0)
    {
        log_error("Could not generate buffer id");
        result = __LINE__;
    }
    else
    {
        alBufferData(sound_info->sound_buf, fmt_info->bits_per_sample == 16 ?
            (fmt_info->num_channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16) :
            (fmt_info->num_channels == 2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8),
            sound_info->wav_data, sound_info->wav_size, fmt_info->sample_rate);
        if (!validate_openal_error("Failure constructing sound data Buffer") )
        {
            result = __LINE__;
        }
        else
        {
            // Attach the buffers to a source
            alGenSources(1, &sound_info->source);
            if (!validate_openal_error("Failure generating sources") )
            {
                result = __LINE__;
            }
            else
            {
                alSourcei(sound_info->source, AL_BUFFER, sound_info->sound_buf);
                if (!validate_openal_error("Failure setting up source buffer") )
                {
                    result = __LINE__;
                }
                else
                {
                    ALfloat sourcePos[] = { -2.0, 0.0, 0.0};
                    ALfloat sourceVel[] = { 0.0, 0.0, 0.0};

                    ALfloat listenerPos[] = {0.0,0.0,4.0};
                    ALfloat listenerVel[] = {0.0,0.0,0.0};
                    ALfloat listenerOri[] = {0.0,0.0,1.0, 0.0,1.0,0.0};

                    alSourcefv(sound_info->source, AL_POSITION, sourcePos);
                    alSourcefv(sound_info->source, AL_VELOCITY, sourceVel);
                    alSourcef(sound_info->source, AL_PITCH, 1.0f);
                    alSourcef(sound_info->source, AL_GAIN, 1.0f);
                    alSourcei(sound_info->source, AL_LOOPING, repeat ? AL_TRUE : AL_FALSE);
                    //alSourcefv(source, AL_DIRECTION, sourceOri);

                    alListenerfv(AL_POSITION, listenerPos);
                    alListenerfv(AL_VELOCITY, listenerVel);
                    alListenerfv(AL_ORIENTATION, listenerOri);
                    result = 0;
                }
            }
        }
    }
    return result;
}

static void clean_audio_items(SOUND_MGR_INFO* sound_info)
{
    if (sound_info->source != 0)
    {
        alDeleteSources(1, &sound_info->source);
    }
    if (sound_info->sound_buf != 0)
    {
        alDeleteBuffers(1, &sound_info->sound_buf);
    }
}

SOUND_MGR_HANDLE sound_mgr_create(void)
{
    SOUND_MGR_INFO* result;
    if ((result = (SOUND_MGR_INFO*)malloc(sizeof(SOUND_MGR_INFO))) == NULL)
    {
        log_error("Failure unable to allocate alarm scheduler");
    }
    else
    {
        memset(result, 0, sizeof(SOUND_MGR_INFO));

        // Initialize the environment
        if (initialize_openai(result) != 0)
        {
            log_error("Failure initializing audio system");
            free(result);
            result = NULL;
        }
    }
    return result;
}

void sound_mgr_destroy(SOUND_MGR_HANDLE handle)
{
    if (handle != NULL)
    {
        clean_audio_items(handle);

        // Exit everything
        deinitialize_openai(handle);

        free(handle);
    }
}

int sound_mgr_play(SOUND_MGR_HANDLE handle, const char* sound_file, bool set_repeat)
{
    int result;
    if (handle == NULL || sound_file == NULL)
    {
        log_error("Invalid argument specified: handle: %p, sound_file: %p", handle, sound_file);
        result = __LINE__;
    }
    else
    {
        // If we're playing a wav then we need to stop playing it
        // before we play another
        if (handle->sound_state == SOUND_STATE_PLAYING)
        {

        }

        unsigned char* wav_buffer;
        format_info fmt_info = {0};
        int swapped;
        if ((wav_buffer = retrieve_wav_data(sound_file, (long*)&handle->wav_size)) == NULL)
        {
            log_error("Failure opening wav file");
            result = __LINE__;
        }
        else if (validate_wav_data(wav_buffer, handle->wav_size, &fmt_info, &handle->wav_data, &swapped) != 0)
        {
            free(wav_buffer);
            log_error("Failure validating wav data");
            result = __LINE__;
        }
        else
        {
            int sample_size = fmt_info.num_channels * fmt_info.bits_per_sample / 8;
            handle->wav_size = chunk_end((const char*)handle->wav_data, swapped) - (const char*)handle->wav_data;
            int data_samples = handle->wav_size / sample_size;

            // If the file is swapped and we have 16-bit audio, we need to endian-swap the audio too or we'll
            // get something that sounds just astoundingly bad!
            if (fmt_info.bits_per_sample == 16 && swapped)
            {
                short* ptr = (short*)handle->wav_data;
                int words = data_samples * fmt_info.num_channels;
                while(words--)
                {
                    *ptr = SWAP_16(*ptr);
                    ++ptr;
                }
            }

            if (construct_buffer_data(handle, &fmt_info, set_repeat) != 0)
            {
                log_error("Failure construting buffer data");
                result = __LINE__;
            }
            else
            {
                alSourcePlay(handle->source);
                handle->sound_state = SOUND_STATE_PLAYING;
                result = 0;
            }
            free(wav_buffer);
        }
    }
    return result;
}

int sound_mgr_stop(SOUND_MGR_HANDLE handle)
{
    int result;
    if (handle == NULL)
    {
        log_error("Invalid argument specified: handle: %p", handle);
        result = __LINE__;
    }
    else if (handle->sound_state != SOUND_STATE_PLAYING)
    {
        log_error("Sound is no longer playing");
        handle->sound_state = SOUND_STATE_IDLE;
        result = __LINE__;
    }
    else
    {
        //ALenum current_playing_state;
        //alGetSourcei(handle->source, AL_SOURCE_STATE, &current_playing_state);
        //alSourcei(handle->sourceId, AL_BUFFER, AL_NONE);
        alSourceStop(handle->source);
        clean_audio_items(handle);
        handle->sound_state = SOUND_STATE_IDLE;
        result = 0;
    }
    return result;
}

SOUND_MGR_STATE sound_mgr_get_current_state(SOUND_MGR_HANDLE handle)
{
    SOUND_MGR_STATE result;
    if (handle == NULL)
    {
        log_error("Invalid argument specified: handle: %p", handle);
        result = SOUND_STATE_ERROR;
    }
    else
    {
        result = handle->sound_state;
    }
    return result;
}
