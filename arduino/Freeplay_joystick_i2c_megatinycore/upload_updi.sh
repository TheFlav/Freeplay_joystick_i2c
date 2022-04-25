sudo systemctl stop hciuart.service
sudo systemctl stop bluetooth.service
sudo rmmod hci_uart
sudo rmmod bnep btbcm bluetooth
raspi-gpio set 30-33 ip
raspi-gpio set 14 a0
raspi-gpio set 15 a0 pu
raspi-gpio set 11 op dh


if [ "${CHIP}" != "817" ]; then
  CHIP=1627
  echo Setting CHIP as ${CHIP}
else
  echo Using CHIP as ${CHIP}
fi
#bin/arduino-cli upload -b megaTinyCore:megaavr:atxy7:chip=${CHIP},clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0 -p /dev/ttyAMA0 -P serialupdi -t --input-dir ./Freeplay_joystick_i2c_megatinycore_build_${CHIP}/
./bin/avrdude -c serialupdi -p t${CHIP} -P /dev/ttyAMA0 -U flash:w:Freeplay_joystick_i2c_megatinycore_build_${CHIP}/Freeplay_joystick_i2c_megatinycore.ino.hex:i
