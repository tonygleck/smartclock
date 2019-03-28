// Licensed under the MIT license. See LICENSE file in the project root for full license information.

var ref = require('ref');
var ffi = require('ffi');

// typedef
var NTP_CLIENT_OBJECT = ref.types.void;
var NTP_CLIENT_HANDLE = ref.refType(NTP_CLIENT_OBJECT);

var ntp_obj = ffi.Library('./clock_util', {
    'ntp_client_create' : [ NTP_CLIENT_HANDLE, [ 'void' ] ],
    'ntp_client_destroy' : [ 'void', [ NTP_CLIENT_HANDLE ] ],
    'ntp_client_get_time' : [ 'int', [ NTP_CLIENT_HANDLE, 'string', 'size_t', 'pointer', 'pointer' ] ],
    'ntp_client_process' : [ 'void', [NTP_CLIENT_HANDLE] ]
});

var ntp_callback = ffi.Callback('void', ['pointer', 'result', 'time'],
    function(user_ctx, result, current_time) {

        console.log("callback envoked");
    });