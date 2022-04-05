#!/bin/bash
FPJSMAGIC=`i2cget -y 1 0x30 0x0D`
FPJS2NDMAGIC=`i2cget -y 1 0x40 0x0D`

#power button GPIO pin
GPIO=27


if [ "${FPJSMAGIC}" != "0xed" ] || [ "${FPJS2NDMAGIC}" != "0xed" ]; then
	#echo "FAILED FPJSMAGIC=${FPJSMAGIC}, FPJS2NDMAGIC=${FPJS2NDMAGIC}"
	echo "Freeplay i2c Joystick device not found on i2c-1!"

	# prepare the pin
	if [ ! -d /sys/class/gpio/gpio${GPIO} ]; then
	  echo "${GPIO}" > /sys/class/gpio/export
	fi
	echo "in" > /sys/class/gpio/gpio"${GPIO}"/direction

	echo "Tap and release POWER button to skip Firmware Upload."
	SKIP=0
	let SECONDSTOWAIT=10
	STARTTIME=`date +%s`
	CURRTIME=`date +%s`
	let ELAPSED=${CURRTIME}-${STARTTIME}
	while [ "${SKIP}" == "0" ] && (( ELAPSED < SECONDSTOWAIT )); do
		if [ 1 == "$(</sys/class/gpio/gpio"${GPIO}"/value)" ]; then
			SKIP=1
		fi
		CURRTIME=`date +%s`
		let ELAPSED=${CURRTIME}-${STARTTIME}
		if (( ELAPSED != PREVELAPSED)); then
			let SECLEFT=${SECONDSTOWAIT}-${ELAPSED}
			echo "$SECLEFT seconds left to tap and release POWER button to skip firmware upload."
		fi
		let PREVELAPSED=${ELAPSED}
	done

	if [ "${SKIP}" == "1" ]; then
		echo "UPDI Firmware Upload skipped!"
		exit 10
	else
		echo "Uploading firmware via UPDI."
		cd /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore
		./upload_updi.sh
		sudo reboot
	fi
fi

echo "Freeplay i2c Joystick Found!"
