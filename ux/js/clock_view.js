var flash_time = false;

function clock_type()
{
    this.digital = 0;
    this.analog = 1;
}

function format_time(digits)
{
    if (digits < 10)
    {
        digits = "0"+digits;
    }
    return digits;
}

function construct_clock()
{
    var curr_time = new Date();

    var hour = curr_time.getHours();
    var min = format_time(curr_time.getMinutes());
    var sec = format_time(curr_time.getSeconds());

    //if (clock_face.face_type == this.digital)
    //{
        var period = " AM";
        if (hour > 12)
        {
            hour = hour-12;
            period = " PM"
        }
        document.getElementById('clock_face').innerHTML = hour + ":" + min + ":" + sec;
        document.getElementById('time_period').innerHTML = period;
    //}
}