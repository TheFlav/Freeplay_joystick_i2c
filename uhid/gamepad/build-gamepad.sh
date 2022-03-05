rm uhid-i2c-gamepad
gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi -lm #compile for wiringPi IRQ, comment '#define USE_PIGPIO_IRQ' and uncomment '#define USE_WIRINGPI_IRQ' in driver_config.h needed
#gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lm -lpigpio #compile for pigpio IRQ, uncomment '#define USE_PIGPIO_IRQ' and comment '#define USE_WIRINGPI_IRQ' in driver_config.h needed

#testing
#sudo ./uhid-i2c-gamepad #normal usage
#sudo ./uhid-i2c-gamepad -h #display help, -help work as well

#sudo ./uhid-i2c-gamepad -configset js0_enable=1 #set config var value
#sudo ./uhid-i2c-gamepad -configlist #list config vars

#sudo ./uhid-i2c-gamepad -noi2c #disable IRQ, I2C polls and pollrate to stress UHID (generate garbage data)
#sudo ./uhid-i2c-gamepad -nouhid #disable UHID reports
#sudo ./uhid-i2c-gamepad -noi2c -nouhid #overkill benchmark, nothing done but generate garbage data
