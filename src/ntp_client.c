// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef WIN32
    #include <winsock.h>
#else
    #include <arpa/inet.h>
    #include <sys/timeb.h>
#endif

#include "patchcords/xio_client.h"
#include "patchcords/xio_socket.h"

#include "ntp_client.h"
#include "lib-util-c/sys_debug_shim.h"
#include "lib-util-c/alarm_timer.h"
#include "lib-util-c/app_logging.h"

#define NTP_PORT_NUM            123
#define MAX_CLOSE_RETRIES       2
#define NTP_PACKET_SIZE         48

#define OPERATION_SUCCESSFUL    1
#define OPERATION_FAILURE       2

static const unsigned long long NTP_TIMESTAMP_DELTA = 2208988800ull;
static const uint32_t JAN_1ST_1900 = 2415021;

typedef struct SET_TIME_INFO_TAG
{
    int operation_complete;
    time_t curr_time;
} SET_TIME_INFO;

typedef enum NTP_CLIENT_STATE_TAG
{
    NTP_CLIENT_STATE_IDLE,
    NTP_CLIENT_STATE_CONNECTED,
    NTP_CLIENT_STATE_SENT,
    NTP_CLIENT_STATE_RECV,
    NTP_CLIENT_STATE_COMPLETE,
    NTP_CLIENT_STATE_ERROR
} NTP_CLIENT_STATE;

// Representation of the NTP timestamp
typedef struct NTP_TIME_PACKET_TAG
{
    uint32_t integer;
    uint32_t fractional;
} NTP_TIME_PACKET;

// NTP basic packet information
typedef struct NTP_BASIC_INFO_TAG
{
    uint8_t li_vn_mode;
    uint8_t stratum;
    uint8_t poll;
    uint8_t precision;

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

typedef struct NTP_CLIENT_INFO_TAG
{
    NTP_TIME_CALLBACK ntp_callback;
    void* user_ctx;
    XIO_IMPL_HANDLE socket_impl;
    size_t timeout_sec;
    bool server_connected;

    NTP_TIME_PACKET recv_packet;
    NTP_CLIENT_STATE ntp_state;
    NTP_OPERATION_RESULT ntp_op_result;
    ALARM_TIMER_HANDLE timer_handle;

    unsigned char collection_buff[48];
    size_t collection_size;
} NTP_CLIENT_INFO;

#ifdef WIN32
LARGE_INTEGER getFILETIMEoffset()
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
    return (t);
}
#endif

static uint32_t clock_get_time(void)
{
    uint32_t result;
#ifdef WIN32
    struct timeval ts;
    LARGE_INTEGER t;
    FILETIME f;
    double microseconds;
    static LARGE_INTEGER offset;
    static double frequencyToMicroseconds;
    static int initialized = 0;
    static BOOL usePerformanceCounter = 0;

    if (!initialized)
    {
        LARGE_INTEGER performanceFrequency;
        initialized = 1;
        usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
        if (usePerformanceCounter)
        {
            QueryPerformanceCounter(&offset);
            frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
        }
        else
        {
            offset = getFILETIMEoffset();
            frequencyToMicroseconds = 10.;
        }
    }
    if (usePerformanceCounter)
    {
        QueryPerformanceCounter(&t);
    }
    else
    {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    microseconds = (double)t.QuadPart / frequencyToMicroseconds;
    t.QuadPart = (LONGLONG)microseconds;
    ts.tv_sec = (long)(t.QuadPart / 1000000);
    ts.tv_usec = (long)(t.QuadPart % 1000000);
    result = (uint32_t)ts.tv_sec * 1000000LL + (uint32_t)ts.tv_usec / 1000LL;
#else
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  result = (uint32_t)ts.tv_sec * 1000000LL + (uint32_t)ts.tv_nsec / 1000LL;
#endif
  return result;
}

static void on_socket_open_complete(void* context, IO_OPEN_RESULT open_result)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        log_error("Invalid context specified");
    }
    else
    {
        if (open_result == IO_OPEN_OK)
        {
            ntp_client->server_connected = true;
            ntp_client->ntp_state = NTP_CLIENT_STATE_CONNECTED;
        }
        else
        {
            ntp_client->ntp_state = NTP_CLIENT_STATE_ERROR;
            ntp_client->ntp_op_result = NTP_OP_RESULT_COMM_ERR;
            log_error("socket open failed");
        }
    }
}

static void on_socket_bytes_received(void* context, const unsigned char* buffer, size_t size)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        log_error("Invalid context specified");
    }
    else
    {
        NTP_RESP_PACKET resp_packet;
        if (ntp_client->collection_size+size > NTP_PACKET_SIZE)
        {
            log_error("Recieving packet size too large");
            ntp_client->ntp_state = NTP_CLIENT_STATE_ERROR;
            ntp_client->collection_size = 0;
        }
        else
        {
            memcpy(ntp_client->collection_buff, buffer, size);
            ntp_client->collection_size += size;
        }

        if (ntp_client->collection_size == sizeof(NTP_BASIC_INFO) )
        {
            NTP_BASIC_INFO* ntp_info = (NTP_BASIC_INFO*)buffer;

            log_debug("Leap Indicator: %d", ntp_info->li_vn_mode&0x11000000);

            // These two fields contain the time-stamp seconds as the packet left the NTP server.
            // The number of seconds correspond to the seconds passed since 1900.
            // ntohl() converts the bit/byte order from the network's to host's "endianness".
            ntp_client->recv_packet.integer = ntohl(ntp_info->ntp_transmit_timestamp.integer);
            ntp_client->recv_packet.fractional = ntohl(ntp_info->ntp_transmit_timestamp.fractional);
            ntp_client->ntp_state = NTP_CLIENT_STATE_RECV;

            log_debug("NTP values: integer value: %u, fraction value: %u", ntp_client->recv_packet.integer, ntp_client->recv_packet.fractional);
        }
    }
}

static void on_socket_error(void* context, IO_ERROR_RESULT error_result)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        log_error("Invalid context specified");
    }
    else
    {
        ntp_client->ntp_state = NTP_CLIENT_STATE_ERROR;
        ntp_client->ntp_op_result = NTP_OP_RESULT_COMM_ERR;
    }
}

static void on_connection_closed(void* context)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        log_error("Invalid context specified");
    }
    else
    {
    }
}

static int send_initial_ntp_packet(NTP_CLIENT_INFO* ntp_client)
{
    int result;
    NTP_BASIC_INFO ntp_info;
    size_t ntp_len = sizeof(NTP_BASIC_INFO);
    memset(&ntp_info, 0, ntp_len);

    ntp_info.li_vn_mode = 0x1B; // Last min is 59, version 4, mode reserved
    ntp_info.ntp_orig_timestamp.integer = (uint32_t)time(NULL);
    ntp_info.ntp_orig_timestamp.fractional = clock_get_time();
    //ntp_info.ntp_transmit_timestamp.integer = ntp_info.ntp_orig_timestamp.integer;
    //ntp_info.ntp_transmit_timestamp.fractional = ntp_info.ntp_orig_timestamp.fractional;

    if (xio_socket_send(ntp_client->socket_impl, &ntp_info, ntp_len, NULL, NULL) != 0)
    {
        log_error("Failure sending NTP packet to server");
        result = MU_FAILURE;
    }
    else
    {
        result = 0;
    }
    return result;
}

static int init_connect_to_server(NTP_CLIENT_INFO* ntp_client, const char* time_server)
{
    int result;
    SOCKETIO_CONFIG socket_config;

    socket_config.hostname = time_server;
    socket_config.port = NTP_PORT_NUM;
    socket_config.address_type = ADDRESS_TYPE_UDP;
    if ((ntp_client->socket_impl = xio_socket_create(&socket_config)) == NULL)
    {
        log_error("Error connecting to NTP server %s:%d", time_server, NTP_PORT_NUM);
        result = MU_FAILURE;
    }
    else
    {
        if (xio_socket_open(ntp_client->socket_impl, on_socket_open_complete, ntp_client, on_socket_bytes_received, ntp_client, on_socket_error, ntp_client) != 0)
        {
            log_error("Error opening socket IO.");
            xio_socket_destroy(ntp_client->socket_impl);
            ntp_client->socket_impl = NULL;
            result = MU_FAILURE;
        }
        else
        {
            result = 0;
        }
    }
    return result;
}

static void close_ntp_connection(NTP_CLIENT_INFO* ntp_client)
{
    if (ntp_client->server_connected)
    {
        if (xio_socket_close(ntp_client->socket_impl, on_connection_closed, ntp_client) == 0)
        {
            size_t counter = 0;
            do
            {
                xio_socket_process_item(ntp_client->socket_impl);
                counter++;
                //ThreadAPI_Sleep(2);
            } while (ntp_client->server_connected && counter < MAX_CLOSE_RETRIES);
            ntp_client->server_connected = false;
        }
    }
    // Close client
    if (ntp_client->socket_impl)
    {
        xio_socket_destroy(ntp_client->socket_impl);
    }
    ntp_client->socket_impl = NULL;
}

static bool is_timed_out(NTP_CLIENT_INFO* ntp_client)
{
    bool result = false;
    if (ntp_client->timeout_sec > 0)
    {
        result = alarm_timer_is_expired(ntp_client->timer_handle);
    }
    return result;
}

NTP_CLIENT_HANDLE ntp_client_create(void)
{
    NTP_CLIENT_INFO* result;
    if ((result = (NTP_CLIENT_INFO*)malloc(sizeof(NTP_CLIENT_INFO))) == NULL)
    {
        log_error("Failure allocating NTP Client Info.");
    }
    else
    {
        memset(result, 0, sizeof(NTP_CLIENT_INFO));
         if ((result->timer_handle = alarm_timer_create()) == NULL)
        {
            free(result);
            result = NULL;
        }
        else
        {
            result->ntp_state = NTP_CLIENT_STATE_IDLE;
        }
    }
    return result;
}

void ntp_client_destroy(NTP_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        close_ntp_connection(handle);
        alarm_timer_destroy(handle->timer_handle);
        free(handle);
    }
}

int ntp_client_get_time(NTP_CLIENT_HANDLE handle, const char* time_server, size_t timeout_sec, NTP_TIME_CALLBACK ntp_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || time_server == NULL || ntp_callback == NULL)
    {
        log_error("Invalid parameter specified handle: %p, time_server: %p, ntp_callback: %p.", handle, time_server, ntp_callback);
        result = __LINE__;
    }
    else if (init_connect_to_server(handle, time_server) != 0)
    {
        log_error("Failure initializing connection to ntp server.");
        result = __LINE__;
    }
    else if (alarm_timer_start(handle->timer_handle, timeout_sec) != 0)
    {
        log_error("Failure starting timer alarm.");
        close_ntp_connection(handle);
        result = __LINE__;
    }
    else
    {
        handle->timeout_sec = timeout_sec;
        handle->ntp_callback = ntp_callback;
        handle->user_ctx = user_ctx;
        result = 0;
    }
    return result;
}

void ntp_client_process(NTP_CLIENT_HANDLE handle)
{
    if (handle != NULL)
    {
        xio_socket_process_item(handle->socket_impl);

        // Check timeout here
        if (handle->server_connected)
        {
            switch (handle->ntp_state)
            {
                case NTP_CLIENT_STATE_CONNECTED:
                    log_debug("NTP Client state connected");
                    // Send the NTP packet
                    if (send_initial_ntp_packet(handle) != 0)
                    {
                        handle->ntp_state = NTP_CLIENT_STATE_ERROR;
                    }
                    else
                    {
                        log_debug("NTP Client: Packet sent");
                        handle->ntp_state = NTP_CLIENT_STATE_SENT;
                        alarm_timer_reset(handle->timer_handle);
                    }
                    break;
                case NTP_CLIENT_STATE_RECV:
                case NTP_CLIENT_STATE_ERROR:
                {
                    time_t recv_time = (time_t)(handle->recv_packet.integer - NTP_TIMESTAMP_DELTA);
                    handle->ntp_callback(handle->user_ctx, handle->ntp_op_result, recv_time);
                    close_ntp_connection(handle);
                    handle->ntp_state = NTP_CLIENT_STATE_COMPLETE;
                    break;
                }
                case NTP_CLIENT_STATE_SENT:
                case NTP_CLIENT_STATE_IDLE:
                case NTP_CLIENT_STATE_COMPLETE:
                default:
                    break;
            }
        }
        else
        {
            // test if the connection has timed out
            if (handle->ntp_state == NTP_CLIENT_STATE_IDLE && is_timed_out(handle) )
            {
                handle->ntp_state = NTP_CLIENT_STATE_ERROR;
                handle->ntp_op_result = NTP_OP_RESULT_TIMEOUT;
                handle->ntp_callback(handle->user_ctx, handle->ntp_op_result, (time_t)0);
            }
        }
    }
}

static void ntp_result_callback(void* user_ctx, NTP_OPERATION_RESULT ntp_result, time_t current_time)
{
    SET_TIME_INFO* set_time_info = (SET_TIME_INFO*)user_ctx;
    if (ntp_result == NTP_OP_RESULT_SUCCESS)
    {
        set_time_info->curr_time = current_time;
        set_time_info->operation_complete = OPERATION_SUCCESSFUL;

        log_debug("Time: %s\r\n", ctime((const time_t*)&current_time) );
    }
    else
    {
        set_time_info->operation_complete = OPERATION_FAILURE;
        log_error("Failure retrieving NTP time %d\r\n", ntp_result);
    }
}

bool ntp_client_set_time(const char* time_server, size_t timeout_sec)
{
    NTP_CLIENT_HANDLE ntp_client = ntp_client_create();
    if (ntp_client != NULL)
    {
        SET_TIME_INFO set_time_info = { 0 };
        if (ntp_client_get_time(ntp_client, time_server, timeout_sec, ntp_result_callback, &set_time_info) == 0)
        {
            do
            {
                ntp_client_process(ntp_client);
                // Sleep here
            } while (set_time_info.operation_complete == 0);
        }
        if (set_time_info.operation_complete == OPERATION_SUCCESSFUL)
        {
            // Set the system time
            //stime(set_time_info.curr_time);
        }
        ntp_client_destroy(ntp_client);
    }
    return 0;

}
