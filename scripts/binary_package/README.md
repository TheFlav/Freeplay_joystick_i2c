## To install with RetroPie on a new SD card
Download the latest Freeplay Joystick binary package from https://github.com/TheFlav/Freeplay_joystick_i2c/releases

Start "Raspberry Pi Imager" (download from https://www.raspberrypi.com/software/)

In "Raspberry Pi Imager"

- Select "CHOOSE OS" and select "Use custom" at the bottom of the list.
- Select the RetroPie SD img.gz file (download from https://retropie.org.uk/download/)
- Select your SD card
- Click the GEAR icon to select settings
	- Check the "Enable SSH" box and choose "Use password authentication" (unless you prefer to do some key auth)
	- Check the "Set locale settings" box and choose your Time Zone and Keyboard Layout
	- Uncheck "Eject media when finished"
	- It may force you to check "Set username and password" so use "pi" and "raspberry" which are the defaults for RetroPie
	- Click "SAVE" at the bottom
- Click the "WRITE" button
	- When it's done writing, you will want to open the "boot" drive on your computer which is the /boot partition of the newly created SD card.
	- In this "boot" drive, open the config.txt file and add the "Freeplay Edits" to the end of the file, and save the changes.
		- If you are using a shell intended for a GBA, then it's recommended to delete the # from the overscan lines below.  This will allow you to use a GBA glass/plastic lens with a smaller viewable area then the full LCD.  You can tweak these lines to your liking.
	- Also in the boot drive, copy in the fpjoy_whatever.zip file and unzip it.  It should create a fpjoy directory in the boot drive.
	- In the boot drive, edit the firstrun.sh file with a text editor.
		- In firstrun.sh, you should find a line that says something like "rm -f /boot/firstrun.sh" near the end of the file.  
		- Add a new line BEFORE this line that says.
		  /boot/fpjoy/fpjoy_firstrun.sh
		- Save the changed file.
	- Eject the boot drive (SD card).
	- Pop the SD card into your Freeplay.
	- Boot it up!


```
##### Freeplay Edits #####

framebuffer_width=640
framebuffer_height=480

dtparam=i2c_arm=on


[EDID=N/A-]
gpio=0-9,12-17,20-25=a2
###dtoverlay=dpi18
enable_dpi_lcd=1
display_default_lcd=1
extra_transpose_buffer=2
dpi_group=2
dpi_mode=87
dpi_output_format=0x6f006
#dpi_output_format=0x6fc06 #disable HSYNC and VSYNC
dpi_timings=640 0 20 10 10 480 0 10 5 5 0 0 0 60 0 60000000 1
#overscan_left=32
#overscan_right=32
#overscan_top=15
#overscan_bottom=84

[edid=*]
#gpio=0-9,12-13,16-17,20-25=ip
gpio=0-9,12-17,20-25=ip
enable_dpi_lcd=0
display_default_lcd=0

#uart0=on
#dtoverlay=uart1,txd0_pin=14,rxd0_pin=15
#dtoverlay=uart0,txd0_pin=14,rxd0_pin=15
#enable_uart=1

[ALL]

dtoverlay=audremap,swap_lr=off,pins_18_19
#dtoverlay=i2c-gpio,i2c_gpio_sda=10,i2c_gpio_scl=11,bus=74,i2c_gpio_delay_us=0
#dtoverlay=i2c-gpio,i2c_gpio_sda=10,i2c_gpio_scl=11,bus=74
#dtoverlay=pca953x,addr=0x20,pca9555
dtoverlay=gpio-poweroff,gpiopin=26,active_low
dtoverlay=gpio-shutdown,gpio_pin=27,active_low=0,gpio_pull=off,debounce=4000

#dtparam=i2c1_baudrate=10000 #low speed mode
#dtparam=i2c1_baudrate=100000 #standard mode
#dtparam=i2c1_baudrate=400000 #fast mode 
dtparam=i2c1_baudrate=1000000 #works with Freeplay i2c Joystick
#dtparam=i2c1_baudrate=3400000 #high speed mode
dtoverlay=i2c1,pins_44_45
#dtoverlay=i2c1,pins_2_3

#dtparam=watchdog=on

#dtoverlay=freeplay-joystick,reg=0x30,interrupt=40,analogsticks=2,dpads=1,digitalbuttons=11,joy0-swapped-x-y=0,joy1-swapped-x-y=1
#interrupt=40 addr=0x30 analogsticks=2 digitalbuttons=13 dpads=0 joy1-x-min=0x1A0 joy1-x-max=0xCC0 joy1-y-min=0x1A0 joy1-y-max=0xD40 joy0-x-fuzz=20 joy0-x-flat=100


gpio=10=np
#disable_audio_dither=1
#dtoverlay=disable-wifi
#dtoverlay=disable-bt

dtparam=act_led_trigger=none
dtparam=act_led_activelow=on

audio_pwm_mode=2
```
	


## To install in an existing system
Run ./install_binary_package.sh to install this Freeplay i2c Joystick binary package

