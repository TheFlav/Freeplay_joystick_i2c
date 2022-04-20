
mkdir -p /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/
cp Freeplay_joystick_i2c_megatinycore.ino.hex /home/pi/Freeplay/Freeplay_joystick_i2c/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/Freeplay_joystick_i2c_megatinycore.ino.hex
mkdir -p /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad
cp uhid-i2c-gamepad /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad
cp uhid-i2c-gamepad-diag /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad
./install_to_retropie.sh 
