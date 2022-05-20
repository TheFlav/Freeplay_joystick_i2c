sudo raspi-config nonint do_i2c 0
sudo raspi-config nonint do_serial 1
sudo raspi-config nonint set_config_var enable_uart 1 /boot/config.txt
#sudo apt install i2c-tools libi2c-dev wiringpi
#if ! grep -Fxq "/etc/modules" i2c-dev; then
#	sed -i '1 i /etc/modules' i2c-dev
#fi

