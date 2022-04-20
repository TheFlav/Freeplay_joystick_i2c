#!/bin/bash
# /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/setup_linux.sh
rm -rf /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package
mkdir -p /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package/contents
cp binary_package/contents/* /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package/contents
cp binary_package/* /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package
cd /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore
make clean
make compile
cd /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad
./build-gamepad.sh

cp /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/Freeplay_joystick_i2c_megatinycore.ino.hex /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package/contents
cp /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package/contents
cp /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad-diag /home/pi/Freeplay/Freeplay_joystick_i2c_binary_package/contents
