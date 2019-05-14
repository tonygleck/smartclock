// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdint.h>
#endif /* __cplusplus */

#define ICON_MAX_LENGTH         8
typedef struct WEATHER_CLIENT_INFO_TAG* WEATHER_CLIENT_HANDLE;

typedef enum WEATHER_OPERATION_RESULT_TAG
{
    WEATHER_OP_RESULT_SUCCESS,
    WEATHER_OP_RESULT_COMM_ERR,
    WEATHER_OP_RESULT_INVALID_DATA_ERR,
    WEATHER_OP_RESULT_ALLOCATION_ERR,
    WEATHER_OP_RESULT_TIMEOUT
} WEATHER_OPERATION_RESULT;

typedef enum _tag_TEMPERATURE_UNITS
{
    UNIT_KELVIN,
    UNIT_CELSIUS,
    UNIT_FAHRENHEIGHT
} TEMPERATURE_UNITS;

typedef struct WEATHER_CONDITIONS_TAG
{
    double temperature;
    double hi_temp;
    double lo_temp;
    uint8_t humidity;
    uint32_t pressure;
    const char* description;
    char weather_icon[ICON_MAX_LENGTH];
} WEATHER_CONDITIONS;

typedef struct WEATHER_LOCATION_TAG
{
    double latitude;
    double longitude;
} WEATHER_LOCATION;


typedef void(*WEATHER_CONDITIONS_CALLBACK)(void* user_ctx, WEATHER_OPERATION_RESULT result, const WEATHER_CONDITIONS* conditions);

extern WEATHER_CLIENT_HANDLE weather_client_create(const char* api_key, TEMPERATURE_UNITS units);
extern void weather_client_destroy(WEATHER_CLIENT_HANDLE handle);

extern int weather_client_get_by_coordinate(WEATHER_CLIENT_HANDLE handle, const WEATHER_LOCATION* location, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx);
extern int weather_client_get_by_zipcode(WEATHER_CLIENT_HANDLE handle, size_t zipcode, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx);
extern int weather_client_get_by_city(WEATHER_CLIENT_HANDLE handle, const char* city_name, size_t timeout, WEATHER_CONDITIONS_CALLBACK conditions_callback, void* user_ctx);

extern void weather_client_process(WEATHER_CLIENT_HANDLE handle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // WEATHER_CLIENT_H
