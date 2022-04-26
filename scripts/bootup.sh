#!/bin/bash

#power button GPIO pin
GPIO=27

sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad -confignocreate -closeonwarn -inputsearch 0x13b

GAMEPAD_RETURN_CODE=$?

JOY_NEEDS_NEW_FIRMWARE=0

if [ $GAMEPAD_RETURN_CODE -ne 0 ]; then
	echo "Freeplay i2c Joystick status code $GAMEPAD_RETURN_CODE needs attention!"
	if [ $GAMEPAD_RETURN_CODE -eq 1 ]; then
		echo "START button detected.  Entering Freeplay i2c Joystick configuration tool."
		sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad-diag
	fi

	#return code -5 (256 minus 5)
	if [ $GAMEPAD_RETURN_CODE -eq 251 ]; then
		echo "Freeplay i2c Joystick not configured.  Entering Freeplay i2c Joystick configuration tool."
		sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad-diag -init
	fi

	#return code -4
	if [ $GAMEPAD_RETURN_CODE -eq 252 ]; then
		echo "Freeplay i2c Joystick version mismatch.  Firmware update required."
		JOY_NEEDS_NEW_FIRMWARE=1
	fi

	#return code -3
	if [ $GAMEPAD_RETURN_CODE -eq 253 ]; then
		echo "Freeplay i2c Joystick manufacturer mismatch.  Firmware update required."
		JOY_NEEDS_NEW_FIRMWARE=1
	fi

	#return code -2
	if [ $GAMEPAD_RETURN_CODE -eq 254 ]; then
		echo "Freeplay i2c Joystick not found on i2c bus.  Firmware update required (or i2c not connected/configured properly)."
		JOY_NEEDS_NEW_FIRMWARE=1
	fi
fi



#uncomment this section to use i2cget for Joystick firmware verification
#FPJSMAGIC=`i2cget -y 1 0x30 0x0D`
#FPJS2NDMAGIC=`i2cget -y 1 0x40 0x0D`
#if [ "${FPJSMAGIC}" != "0xed" ] || [ "${FPJS2NDMAGIC}" != "0xed" ]; then
#	JOY_NEEDS_NEW_FIRMWARE=1
#fi


if [ $JOY_NEEDS_NEW_FIRMWARE -eq 1 ]; then
	#echo "FAILED FPJSMAGIC=${FPJSMAGIC}, FPJS2NDMAGIC=${FPJS2NDMAGIC}"
	echo "Freeplay i2c Joystick needs new firmware."

	cd /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore
        ./verify.sh
	if [ $? == 0 ]; then
		echo "*** WARNING ***"
		echo "It seems like you have the proper Firmware installed, but the Freeplay i2c Joystick is not found on the i2c bus."
		echo "Check the ribbon cable and retry or run 'i2cdetect -y 0' to troubleshoot."
		echo "Also check /boot/config.txt for proper i2c settings."		
		echo "***************"
		sleep 2.5
		echo "***************"
		sleep 2.5
		echo "Continuing in 5 seconds."
		sleep 5
		exit 0
	fi

	# prepare the pin
	PINFUNC=`raspi-gpio get ${GPIO} | awk '{print $5;}'`
	if [ "$PINFUNC" != "func=INPUT" ] ; then
        	raspi-gpio set ${GPIO} ip
	fi
        PINFUNC=`raspi-gpio get ${GPIO} | awk '{print $5;}'`
	if [ "$PINFUNC" != "func=INPUT" ] ; then
		echo "ERROR: Can't set GPIO ${GPIO} pin function!"
		exit 20
        fi

	echo "Tap and release POWER button to skip Firmware Upload."
	SKIP=0
	let SECONDSTOWAIT=10
	STARTTIME=`date +%s`
	CURRTIME=`date +%s`
	let ELAPSED=${CURRTIME}-${STARTTIME}
	while [ "${SKIP}" == "0" ] && (( ELAPSED < SECONDSTOWAIT )); do
		PINVAL=`raspi-gpio get ${GPIO} | awk '{print $3;}'`

		if [ $PINVAL == "level=1" ]; then
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

	        SKIP=0
        	let SECONDSTOWAIT=10
	        STARTTIME=`date +%s`
	        CURRTIME=`date +%s`
	        let ELAPSED=${CURRTIME}-${STARTTIME}
	        while [ "${SKIP}" == "0" ] && (( ELAPSED < SECONDSTOWAIT )); do
			PINVAL=`raspi-gpio get ${GPIO} | awk '{print $3;}'`

			if [ $PINVAL == "level=1" ]; then
				SKIP=1
			fi

	                CURRTIME=`date +%s`
	                let ELAPSED=${CURRTIME}-${STARTTIME}
	                if (( ELAPSED != PREVELAPSED)); then
	                        let SECLEFT=${SECONDSTOWAIT}-${ELAPSED}
	                        echo "$SECLEFT seconds left to tap and release POWER button to skip reboot."
	                fi
	                let PREVELAPSED=${ELAPSED}
	        done
	        if [ "${SKIP}" == "0" ]; then
			sudo reboot
		fi
	fi
fi

echo "Freeplay i2c Joystick Found!"
echo "Freeplay i2c Joystick driver starting."
sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad --quiet > /dev/null &
