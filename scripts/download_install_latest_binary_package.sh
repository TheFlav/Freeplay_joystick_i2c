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

cp -r /home/pi/Freeplay/Freeplay_joystick_i2c /home/pi/Freeplay/Freeplay_joystick_i2c_BAK_$(date +"%Y-%m-%d-%H-%M-%S")
sudo killall uhid-i2c-gamepad

/tmp/fpjoy/install_binary_package.sh

echo "Freeplay i2c Joystick driver starting."
sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad --quiet > /dev/null &
sleep 3
sudo chmod a+x /dev/shm/uhid_i2c_driver
sudo chmod a+x /dev/shm/uhid_i2c_driver/0
sudo chmod a+w /dev/shm/uhid_i2c_driver/0/*

