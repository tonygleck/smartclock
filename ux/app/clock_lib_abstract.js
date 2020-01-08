// Licensed under the MIT license. See LICENSE file in the project root for full license information.

var ref = require('ref');
var ffi = require('ffi');

var CLOCK_UTIL_LIBRARY = "../../cmake/default/clock_util.so";

// typedef
var WEATHER_CLIENT_OBJECT = ref.types.void;
var WEATHER_CLIENT_HANDLE = ref.refType(WEATHER_CLIENT_OBJECT);

// enumeration
var WEATHER_OPERATION_RESULT = {
    "WEATHER_OP_RESULT_SUCCESS":0,
    "WEATHER_OP_RESULT_COMM_ERR":1,
    "WEATHER_OP_RESULT_INVALID_DATA_ERR":2,
    "WEATHER_OP_RESULT_TIMEOUT":3
};

var TEMPERATURE_UNITS = {
    "TEMP_UNIT_KELVIN":0,
    "TEMP_UNIT_CELSIUS":1,
    "TEMP_UNIT_FAHRENHEIGHT":2
};

// structure
var WEATHER_LOCATION = Struct({
    'latitude': 'double',
    'longitude': 'double'
});
var WEATHER_LOCATION_PTR = ref.refType(WEATHER_LOCATION);

var WEATHER_CONDITIONS = Struct({
    'temperature': 'double',
    'hi_temp': 'double',
    'lo_temp': 'double',
    'description': 'string'
});
var WEATHER_CONDITIONS_PTR = ref.refType(WEATHER_CONDITIONS);

var ntp_obj = ffi.Library(CLOCK_UTIL_LIBRARY, {
    'ntp_client_set_time' : [ 'int', [ 'string', 'size_t' ] ],

    'weather_client_create' : [ WEATHER_CLIENT_HANDLE, [ 'string', TEMPERATURE_UNITS ] ],
    'weather_client_destroy' : [ 'void', [ WEATHER_CLIENT_HANDLE ] ],
    'weather_client_get_by_city' : [ 'int', [ WEATHER_CLIENT_HANDLE, 'WEATHER_LOCATION_PTR', 'size_t', 'pointer', 'pointer' ] ],
    'weather_client_process' : [ 'void', [ WEATHER_CLIENT_HANDLE ] ]
});

var ntp_callback = ffi.Callback('void', ['pointer', 'WEATHER_OPERATION_RESULT', 'WEATHER_CONDITIONS_PTR'],
    function(user_ctx, result, current_cond) {

        console.log("callback envoked");
    });

function set_ntp_time(server, timeout) {
    if (ntp_obj.ntp_client_set_time(server, timeout) == 0) {
        console.log("time has been setup successfully")
    } else {
        console.log("Setting time has failed")
    }
}

function retrieve_city_weather() {

};