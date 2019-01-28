#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>

#include "ntp_client.h"
#include "azure_c_shared_utility/socketio.h"

#define NTP_PORT_NUM            123
const uint32_t JAN_1ST_1900 = 2415021;

typedef struct NTP_CLIENT_INFO_TAG
{
    NTP_TIME_CALLBACK ntp_callback;
    void* user_ctx;
    XIO_HANDLE socketio;
    uint8_t timeout_sec;
    bool server_connected;
    bool error_encountered;
} NTP_CLIENT_INFO;

// Representation of the NTP timestamp
typedef struct NTP_TIME_PACKET_TAG
{
    uint32_t integer;
    uint32_t fractional;
} NTP_TIME_PACKET;

// NTP basic packet information
typedef struct NTP_BASIC_INFO_TAG
{
    unsigned char li_vn_mode;
    unsigned char stratum;
    char poll;
    char precision;
    uint32_t root_delay;
    uint32_t root_dispersion;
    uint32_t reference_id;
    NTP_TIME_PACKET ntp_ref_timestamp;
    NTP_TIME_PACKET ntp_orig_timestamp;
    NTP_TIME_PACKET ntp_recv_timestamp;
    NTP_TIME_PACKET ntp_transmit_timestamp;
} NTP_BASIC_INFO;

typedef struct NTP_AUTH_INFO_TAG
{
    uint32_t key_algorithm;
    uint32_t message_digest[16];
} NTP_AUTH_INFO;

typedef struct NTP_RESP_PACKET_TAG
{
    NTP_BASIC_INFO ntp_basic_info;
    NTP_AUTH_INFO ntp_auth_info;
} NTP_RESP_PACKET;


/*static uint64_t get_julian_date(const struct tm* gmt)
{
    uint64_t result;
    uint32_t month = 0;
    uint32_t year = 0;
    if (gmt->tm_mon > 2)
    {
        month = gmt->tm_mon - 3;
    }
    else
    {
        month = gmt->tm_mon + 9;
        year = gmt->tm_year - 1;
    }
    uint32_t intermediate = year / 100;
    uint32_t temp_val = year - 100 * intermediate;
    result = (146097L * intermediate) / 4 + (1461L * temp_val) / 4 + (153L * month + 2) / 5 + gmt->tm_mday + 1721119L;
    return result;
}

static int generate_utc_timevalue(NTP_TIME_PACKET* ntp_time)
{
    // Get the UTC Time
    time_t tm_value = time(NULL);
    struct tm time_info;
#ifdef WIN32
        gmtime_s(&time_info, &tm_value);
#else
        struct tm* ptm = gmtime(&tm_value);
        memcpy(&time_info, ptm, sizeof(tm));
#endif

    // Convert the UTC to a NTP object
    uint64_t date_value = get_julian_date(&time_info);
    date_value -= JAN_1ST_1900;

    date_value = (date_value * 24) + time_info.tm_hour;
    date_value = (date_value * 60) + time_info.tm_min;
    date_value = (date_value * 60) + time_info.tm_sec;
    date_value = (date_value << 32) + MS_TO_NTP[time_info.];
}*/

static uint32_t clock_get_time(void)
{
    uint32_t result;
#ifdef WIN32
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  result = (uint32_t)ts.tv_sec * 1000000LL + (uint32_t)ts.tv_nsec / 1000LL;
#endif
  return result;
}

static void on_io_open_complete(void* context, IO_OPEN_RESULT open_result)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        printf("Invalid context specified");
    }
    else
    {
        if (open_result == IO_SEND_OK)
        {
            ntp_client->server_connected = true;
        }
        else
        {
            ntp_client->error_encountered = true;
            printf("open failed");
        }
    }
}

static void on_io_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        printf("Invalid context specified");
    }
    else
    {
        NTP_RESP_PACKET resp_packet;
    }
}

static void on_io_error(void* context)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        printf("Invalid context specified");
    }
    else
    {
        ntp_client->error_encountered = true;
    }
}

static int send_initial_ntp_packet(NTP_CLIENT_INFO* ntp_client)
{
    int result;
    NTP_BASIC_INFO ntp_info;
    size_t ntp_len = sizeof(NTP_BASIC_INFO);
    memset(&ntp_info, 0, ntp_len);

    ntp_info.li_vn_mode = 0x1B; // Last min is 59, version 4, mode reserved
    ntp_info.ntp_orig_timestamp.integer = time(NULL);
    ntp_info.ntp_orig_timestamp.fractional = clock_get_time();
    ntp_info.ntp_transmit_timestamp.integer = ntp_info.ntp_orig_timestamp.integer;
    ntp_info.ntp_transmit_timestamp.fractional = ntp_info.ntp_orig_timestamp.fractional;

    if (xio_send(ntp_client->socketio, &ntp_info, ntp_len, NULL, NULL) != 0)
    {
        printf("Failure sending NTP packet to server");
        result = __FAILURE__;
    }
    else
    {
        result = 0;
    }

}

static int init_connect_to_server(NTP_CLIENT_INFO* ntp_client, const char* time_server)
{
    int result;
    const IO_INTERFACE_DESCRIPTION* socketio_interface = socketio_get_interface_description();
    if (socketio_interface == NULL)
    {
        (void)printf("Error getting socketio interface description.");
        result = __FAILURE__;
    }
    else
    {
        SOCKETIO_CONFIG socketio_config;
        XIO_HANDLE socketio;

        socketio_config.hostname = time_server;
        socketio_config.port = NTP_PORT_NUM;
        if ((socketio = xio_create(socketio_interface, &socketio_config)) == NULL)
        {
            (void)printf("Error connecting to NTP server %s:", time_server, NTP_PORT_NUM);
            result = __FAILURE__;
        }
        else if (xio_open(socketio, on_io_open_complete, ntp_client, on_io_bytes_received, ntp_client, on_io_error, ntp_client) != 0)
        {
            (void)printf("Error opening socket IO.");
            result = __FAILURE__;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

NTP_CLIENT_HANDLE ntp_client_create(void)
{
    NTP_CLIENT_INFO* result;
    if ((result = (NTP_CLIENT_INFO*)malloc(sizeof(NTP_CLIENT_INFO))) == NULL)
    {
        (void)printf("Failure allocating NTP Client Info.");
    }
    else
    {
        memset(result, 0, sizeof(NTP_CLIENT_INFO));
    }

    return result;
}

void ntp_client_destroy(NTP_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        free(handle);
    }
}

int ntp_client_get_time(NTP_CLIENT_HANDLE handle, const char* time_server, uint8_t timeout_sec, NTP_TIME_CALLBACK ntp_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || time_server == NULL || ntp_callback == NULL)
    {
        (void)printf("Invalid parameter specified handle: %p, time_server: %p, ntp_callback: %p.", handle, time_server, ntp_callback);
        result = __LINE__;
    }
    else if (platform_init() != 0)
    {
        (void)printf("Cannot initialize platform.");
        result = __LINE__;
    }
    else if (init_connect_to_server(handle, time_server) != 0)
    {
        (void)printf("Failure initializing connection to ntp server.");
        result = __LINE__;
    }
    else
    {
        handle->timeout_sec = timeout_sec;
    }
    return NULL;
}

void ntp_client_process(NTP_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        xio_dowork(handle->socketio);

        // Check timeout here
        if (handle->server_connected)
        {

        }
    }
}
