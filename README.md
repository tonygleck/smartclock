# smartclock

Clock that knows

## installing

### Installing SDL2

#### Easy way

sudo apt-get install libsdl2-2.0
sudo apt-get install libsdl2-dev

hg clone https://hg.libsdl.org/SDL SDL
cd SDL
mkdir build
cd build
../configure
make
sudo make install

### development

May not be necessary

sudo apt-get install libasound2-dev

sudo apt-get install libopenal-dev libalut-dev libncurses-dev

### production

sudo apt-get install libasound2
sudo apt-get install libopenal0a libalut0 libncurses-dev

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

### smartclock size

may 23 - 93,464 bytes
Jun 15 - 93,504 bytes

### Auto starting application on RPI

Eddit the following file:

```Shell
sudo nano /etc/xdg/lxsession/LXDE-pi/autostart
```

Add the following line to the file

```Shell
@/home/pi/smartclock/development/repo/smartclock/cmake/smartclock
```
