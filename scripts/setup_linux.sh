sudo apt install i2c-tools libi2c-dev wiringpi
if ! grep -Fxq "/etc/modules" i2c-dev; then
	sed -i '1 i /etc/modules' i2c-dev
fi

