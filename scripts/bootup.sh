#!/bin/bash
FPJSMAGIC=`i2cget -y 1 0x30 0x0D`
FPJS2NDMAGIC=`i2cget -y 1 0x40 0x0D`

if [ "${FPJSMAGIC}" != "0xed" ] || [ "${FPJS2NDMAGIC}" != "0xed" ]; then
	#echo "FAILED FPJSMAGIC=${FPJSMAGIC}, FPJS2NDMAGIC=${FPJS2NDMAGIC}"
	echo "Freeplay i2c Joystick device not found on i2c-1!"
	echo "Uploading firmware via UPDI."
	cd /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore
	./upload_updi.sh
fi
