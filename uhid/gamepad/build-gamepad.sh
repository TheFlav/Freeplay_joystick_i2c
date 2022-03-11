#cleanup
rm uhid-i2c-gamepad
rm uhid-i2c-gamepad-diag

#compile without IRQ support
#gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c

#compile with PIGPIO IRQ
#gcc -DUSE_PIGPIO_IRQ -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c lpigpio

#compile with wiringPi IRQ
gcc -DUSE_WIRINGPI_IRQ -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi

#compile diagnostic program
gcc -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c


#testing
#sudo ./uhid-i2c-gamepad #normal usage
#sudo ./uhid-i2c-gamepad -h #display help, -help work as well

#sudo ./uhid-i2c-gamepad -configset js0_enable=1 #set config var value
#sudo ./uhid-i2c-gamepad -configlist #list config vars

#sudo ./uhid-i2c-gamepad -noi2c #disable IRQ, I2C polls and pollrate to stress UHID (generate garbage data)
#sudo ./uhid-i2c-gamepad -nouhid #disable UHID reports
#sudo ./uhid-i2c-gamepad -noi2c -nouhid #overkill benchmark, nothing done but generate garbage data

#sudo ./uhid-i2c-gamepad-diag #diagnostic program