#!/bin/bash

LCDINUSE=$(/opt/vc/bin/tvservice -s | grep -c "\[LCD\]")
if [[ "$LCDINUSE" == "1" ]]; then
	echo 1 > /dev/shm/uhid_i2c_driver/0/lcd_sleep_mode
fi
