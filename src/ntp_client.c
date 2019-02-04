#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <sys/timeb.h>

#ifdef WIN32
    #include <winsock.h>
#else
    #include <arpa/inet.h>
#endif

#include "ntp_client.h"
#include "azure_c_shared_utility/socketio.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "lib-util-c/alarm_timer.h"

#define NTP_PORT_NUM            123
#define MAX_CLOSE_RETRIES       2

static const unsigned long long NTP_TIMESTAMP_DELTA = 2208988800ull;
static const uint32_t JAN_1ST_1900 = 2415021;

typedef enum NTP_CLIENT_STATE_TAG
{
    NTP_CLIENT_STATE_IDLE,
    NTP_CLIENT_STATE_CONNECTED,
    NTP_CLIENT_STATE_SENT,
    NTP_CLIENT_STATE_RECV,
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
    CONCRETE_IO_HANDLE socketio;
    size_t timeout_sec;
    bool server_connected;

    NTP_TIME_PACKET recv_packet;
    NTP_CLIENT_STATE ntp_state;
    NTP_OPERATION_RESULT ntp_op_result;
    ALARM_TIMER_HANDLE timer_handle;
} NTP_CLIENT_INFO;

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
            ntp_client->ntp_state = NTP_CLIENT_STATE_CONNECTED;
        }
        else
        {
            ntp_client->ntp_state = NTP_CLIENT_STATE_ERROR;
            ntp_client->ntp_op_result = NTP_OP_RESULT_COMM_ERR;
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
        if (size != sizeof(NTP_BASIC_INFO))
        {
            // TODO: Store the bits
        }
        else
        {
            NTP_BASIC_INFO* ntp_info = (NTP_BASIC_INFO*)buffer;

            // These two fields contain the time-stamp seconds as the packet left the NTP server.
            // The number of seconds correspond to the seconds passed since 1900.
            // ntohl() converts the bit/byte order from the network's to host's "endianness".
            ntp_client->recv_packet.integer = ntohl(ntp_info->ntp_transmit_timestamp.integer);
            ntp_client->recv_packet.fractional = ntohl(ntp_info->ntp_transmit_timestamp.fractional);
            ntp_client->ntp_state = NTP_CLIENT_STATE_RECV;
        }
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
        ntp_client->ntp_state = NTP_CLIENT_STATE_ERROR;
        ntp_client->ntp_op_result = NTP_OP_RESULT_COMM_ERR;
    }
}

static void on_connection_closed(void* context)
{
    NTP_CLIENT_INFO* ntp_client = (NTP_CLIENT_INFO*)context;
    if (ntp_client == NULL)
    {
        printf("Invalid context specified");
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
    ntp_info.ntp_orig_timestamp.integer = time(NULL);
    ntp_info.ntp_orig_timestamp.fractional = clock_get_time();
    ntp_info.ntp_transmit_timestamp.integer = ntp_info.ntp_orig_timestamp.integer;
    ntp_info.ntp_transmit_timestamp.fractional = ntp_info.ntp_orig_timestamp.fractional;

    if (socketio_send(ntp_client->socketio, &ntp_info, ntp_len, NULL, NULL) != 0)
    {
        printf("Failure sending NTP packet to server");
        result = __FAILURE__;
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
    SOCKETIO_CONFIG socketio_config;

    socketio_config.hostname = time_server;
    socketio_config.port = NTP_PORT_NUM;
    if ((ntp_client->socketio = socketio_create(&socketio_config)) == NULL)
    {
        (void)printf("Error connecting to NTP server %s:%d", time_server, NTP_PORT_NUM);
        result = __FAILURE__;
    }
    else
    {
        if (socketio_setoption(ntp_client->socketio, OPTION_ADDRESS_TYPE, OPTION_ADDRESS_TYPE_UDP_SOCKET) != 0)
        {
            (void)printf("Error opening socket IO.");
            socketio_destroy(ntp_client->socketio);
            ntp_client->socketio = NULL;
            result = __FAILURE__;
        }
        else if (socketio_open(ntp_client->socketio, on_io_open_complete, ntp_client, on_io_bytes_received, ntp_client, on_io_error, ntp_client) != 0)
        {
            (void)printf("Error opening socket IO.");
            socketio_destroy(ntp_client->socketio);
            ntp_client->socketio = NULL;
            result = __FAILURE__;
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
    if (socketio_close(ntp_client->socketio, on_connection_closed, ntp_client) == 0)
    {
        size_t counter = 0;
        do
        {
            socketio_dowork(ntp_client->socketio);
            counter++;
            //ThreadAPI_Sleep(2);
        } while (ntp_client->server_connected && counter < MAX_CLOSE_RETRIES);
        ntp_client->server_connected = false;
    }
    // Close client
    socketio_destroy(ntp_client->socketio);
    ntp_client->socketio = NULL;
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
        (void)printf("Failure allocating NTP Client Info.");
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
        if (handle->server_connected)
        {
            close_ntp_connection(handle);
        }
        alarm_timer_destroy(handle->timer_handle);
        free(handle);
    }
}

int ntp_client_get_time(NTP_CLIENT_HANDLE handle, const char* time_server, size_t timeout_sec, NTP_TIME_CALLBACK ntp_callback, void* user_ctx)
{
    int result;
    if (handle == NULL || time_server == NULL || ntp_callback == NULL)
    {
        (void)printf("Invalid parameter specified handle: %p, time_server: %p, ntp_callback: %p.\r\n", handle, time_server, ntp_callback);
        result = __LINE__;
    }
    else if (init_connect_to_server(handle, time_server) != 0)
    {
        (void)printf("Failure initializing connection to ntp server.\r\n");
        result = __LINE__;
    }
    else if (alarm_timer_start(handle->timer_handle, timeout_sec) != 0)
    {
        (void)printf("Failure starting timer alarm.\r\n");
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
        socketio_dowork(handle->socketio);

        // Check timeout here
        if (handle->server_connected)
        {
            switch (handle->ntp_state)
            {
                case NTP_CLIENT_STATE_CONNECTED:
                    // Send the NTP packet
                    if (send_initial_ntp_packet(handle) != 0)
                    {
                        handle->ntp_state = NTP_CLIENT_STATE_ERROR;
                    }
                    else
                    {
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
                    handle->ntp_state = NTP_CLIENT_STATE_IDLE;
                    break;
                }
                case NTP_CLIENT_STATE_SENT:
                case NTP_CLIENT_STATE_IDLE:
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
