#!/bin/bash
rm -rf /tmp/fpjoy
rm -f /tmp/fpjoy_binary_package.zip
wget -O /tmp/fpjoy_binary_package.zip $(curl -s https://api.github.com/repos/TheFlav/Freeplay_joystick_i2c/releases/latest | grep 'browser_' | cut -d\" -f4)
cd /tmp
unzip fpjoy_binary_package.zip
diff /tmp/fpjoy/contents/builddate.txt /home/pi/Freeplay/Freeplay_joystick_i2c/builddate.txt
if [ "$?" == "0" ]; then
  echo "Build dates match.  No need to upgrade."
  exit 1
fi

/home/pi/Freeplay/Freeplay_joystick_i2c/scripts/lcd_on.sh

cp -r /home/pi/Freeplay/Freeplay_joystick_i2c /home/pi/Freeplay/Freeplay_joystick_i2c_BAK_$(date +"%Y-%m-%d-%H-%M-%S")
/tmp/fpjoy/install_binary_package.sh

sudo killall uhid-i2c-gamepad
sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad --quiet > /dev/null &
