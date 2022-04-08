if ! grep -Fxq "/home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh" /opt/retropie/configs/all/autostart.sh; then
	sed -i '1 i /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh' /opt/retropie/configs/all/autostart.sh
fi

