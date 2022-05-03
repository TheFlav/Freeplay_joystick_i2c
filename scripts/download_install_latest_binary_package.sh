#!/bin/bash
rm -rf /tmp/fpjoy
rm -f /tmp/fpjoy_binary_package.zip
wget -O /tmp/fpjoy_binary_package.zip $(curl -s https://api.github.com/repos/TheFlav/Freeplay_joystick_i2c/releases/latest | grep 'browser_' | cut -d\" -f4)
cd /tmp
unzip fpjoy_binary_package.zip
cp -r /home/pi/Freeplay/Freeplay_joystick_i2c /home/pi/Freeplay/Freeplay_joystick_i2c_BAK_$(date +"%Y-%m-%d-%H-%M-%S")
/tmp/fpjoy/install_binary_package.sh
