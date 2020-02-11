var flash_time = false;

var weekday = new Array(7);
weekday[0] = "Sunday";
weekday[1] = "Monday";
weekday[2] = "Tuesday";
weekday[3] = "Wednesday";
weekday[4] = "Thursday";
weekday[5] = "Friday";
weekday[6] = "Saturday";

var month = new Array(12);
month[0] = "January";
month[1] = "February";
month[2] = "March";
month[3] = "April";
month[4] = "May";
month[5] = "June";
month[6] = "July";
month[7] = "August";
month[8] = "September";
month[9] = "October";
month[10] = "November";
month[11] = "December";

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

function format_date_value(curr_time)
{
    var format_date = weekday[curr_time.getDay()] + "  " + month[curr_time.getMonth()] + " " + curr_time.getDate() + " " + curr_time.getFullYear();
    return format_date;
}

function construct_clock()
{
    var curr_time = new Date();

    var hour = curr_time.getHours();
    var min = format_time(curr_time.getMinutes());
    var sec = format_time(curr_time.getSeconds());

    //if (clock_face.face_type == this.digital)
    //{
        var period = "AM";
        if (hour > 12)
        {
            hour = hour-12;
            period = "PM"
        }
        var clock = document.getElementById('clock');
        clock.innerText = hour + ":" + min + ":" + sec;

        var period_obj = document.getElementById('period');
        period_obj.innerText = period;

        var clock_date = document.getElementById('clock_date');
        clock_date.innerText = format_date_value(curr_time);
        //}
}
