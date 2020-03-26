# smartclock

Clock that knows

## installing

### development

May not be necessary
sudo apt-get install libasound2-dev

sudo apt-get install libopenal-dev  libalut-dev

### production

sudo apt-get install libasound2
sudo apt-get install libopenal0a libalut0

### Adjusting the RPI screen

The driver for the screen provides an interface through /sys/. To turn the screen on you can use the command:

```Shell
echo 0 > /sys/class/backlight/rpi_backlight/bl_power
```

and to turn it off:

```Shell
echo 1 > /sys/class/backlight/rpi_backlight/bl_power
```

the brightness can be adjusted using:

```Shell
echo n > /sys/class/backlight/rpi_backlight/brightness
```

where n is some value between 0 and 255.

### how-to-disable-the-blank-screen

https://www.geeks3d.com/hacklab/20160108/how-to-disable-the-blank-screen-on-raspberry-pi-raspbian/