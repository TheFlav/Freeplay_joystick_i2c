#!/usr/bin/env bash

if [ ! -d /opt/retropie ]; then
	echo "RetroPie not found!"
	exit 1
fi

mkdir -p /opt/retropie/configs/all/emulationstation/scripts/sleep/
mkdir -p /opt/retropie/configs/all/emulationstation/scripts/wake/
mkdir -p /opt/retropie/configs/all/emulationstation/scripts/screensaver-stop/
mkdir -p /opt/retropie/configs/all/emulationstation/scripts/screensaver-start/

ln -s /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/lcd_off.sh /opt/retropie/configs/all/emulationstation/scripts/sleep/
ln -s /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/lcd_on.sh /opt/retropie/configs/all/emulationstation/scripts/wake/
ln -s /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/lcd_on.sh /opt/retropie/configs/all/emulationstation/scripts/screensaver-stop/
ln -s /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/lcd_dim.sh /opt/retropie/configs/all/emulationstation/scripts/screensaver-start/

chmod -R a+x /opt/retropie/configs/all/emulationstation/scripts/

if ! grep -Fxq "/home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh" /opt/retropie/configs/all/autostart.sh; then
	sed -i '1 i /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh' /opt/retropie/configs/all/autostart.sh
fi

