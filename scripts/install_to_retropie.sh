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

if [ ! -d /etc/emulationstation/themes/freeplay ] ; then
	sudo git clone --recursive --depth 1 "https://github.com/rxbrad/es-theme-freeplay.git" "/etc/emulationstation/themes/freeplay"
fi

if [ ! -f /etc/emulationstation/themes/freeplay/theme_enabled_once_already.txt ] ; then
	echo "Enabling Freeplay Theme"
	sudo sed -i 's/<string name="ThemeSet" value=".*" \/>/<string name="ThemeSet" value="freeplay" \/>/g' /opt/retropie/configs/all/emulationstation/es_settings.cfg
	sudo sed -i 's/<string name="TransitionStyle" value=".*" \/>/<string name="TransitionStyle" value="instant" \/>/' /opt/retropie/configs/all/emulationstation/es_settings.cfg
	sudo touch /etc/emulationstation/themes/freeplay/theme_enabled_once_already.txt
fi


mkdir -p "/home/pi/RetroPie/retropiemenu/Freeplay Options"
cp /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/config_menu.sh "/home/pi/RetroPie/retropiemenu/Freeplay Options/"

if grep -q "Freeplay Configuration" /opt/retropie/configs/all/emulationstation/gamelists/retropie/gamelist.xml ; then
	echo "Freeplay Configuration already in menu"
else
	sudo sed -i 's|</gameList>|\t<game>\n\t\t<path>./Freeplay Options/config_menu.sh</path>\n\t\t<name>Freeplay Configuration</name>\n\t\t<desc>Configure settings for the Freeplay system</desc>\n\t\t<image></image>\n\t\t<playcount>0</playcount>\n\t\t<lastplayed>20220501T205700</lastplayed>\n\t</game>\n</gameList>|' /opt/retropie/configs/all/emulationstation/gamelists/retropie/gamelist.xml
fi

exit 0
