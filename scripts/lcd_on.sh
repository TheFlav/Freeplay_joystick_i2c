#!/bin/bash

LCDINUSE=$(/opt/vc/bin/tvservice -s | grep -c "\[LCD\]")
if [[ "$LCDINUSE" == "1" ]]; then 
	sudo echo 0 > /dev/shm/uhid_i2c_driver/0/lcd_dimming_mode
	sudo echo 0 > /dev/shm/uhid_i2c_driver/0/lcd_sleep_mode
fi
