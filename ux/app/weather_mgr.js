// Licensed under the MIT license. See LICENSE file in the project root for full license information.

var ref = require('ref');
var ffi = require('ffi');

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

var weather_mgr = ffi.Library('./clock_util', {
    'weather_client_create' : [ WEATHER_CLIENT_HANDLE, [ 'string' ] ],
    'weather_client_destroy' : [ 'void', [ WEATHER_CLIENT_HANDLE ] ],
    'weather_client_get_conditions' : [ 'int', [ WEATHER_CLIENT_HANDLE, 'WEATHER_LOCATION_PTR', 'size_t', 'pointer', 'pointer' ] ],
    'ntp_client_process' : [ 'void', [WEATHER_CLIENT_HANDLE] ]
});

var ntp_callback = ffi.Callback('void', ['pointer', 'WEATHER_OPERATION_RESULT', 'WEATHER_CONDITIONS_PTR'],
    function(user_ctx, result, current_cond) {

        console.log("callback envoked");
    });