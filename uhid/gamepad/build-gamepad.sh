#cleanup
rm uhid-i2c-gamepad
rm uhid-i2c-gamepad-diag

#compile without IRQ support
#gcc -march=armv6 -mfpu=vfp -mfloat-abi=hard -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -l:libi2c.a

#compile with wiringPi IRQ
#gcc -march=armv6 -mfpu=vfp -mfloat-abi=hard -DUSE_WIRINGPI -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -l:libi2c.a -lwiringPi

#compile with libGPIOd IRQ
gcc -march=armv6 -mfpu=vfp -mfloat-abi=hard -DUSE_GPIOD -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -l:libi2c.a -l:libgpiod.a

#compile diagnostic program
gcc -march=armv6 -mfpu=vfp -mfloat-abi=hard -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -l:libi2c.a

#preprocessor variables
# ALLOW_MCU_SEC_I2C : MCU secondary features, on-the-fly I2C address update, backlight control, ...
# USE_SHM_REGISTERS : SHM to MCU bridge, allow to direct update some registers using file system.
# ALLOW_EXT_ADC : External ADCs.
# USE_WIRINGPI : IRQ pin poll using WiringPi.
# USE_GPIOD : IRQ pin poll using libGPIOd.
# DIAG_PROGRAM : Compile Setup/Diagnostic part of the driver, IRQ and SHM functions/variables will be discarded.
# MULTI_INSTANCES : Allow more than one instance of the driver/diag program at the same time.

#testing
#sudo ./uhid-i2c-gamepad #normal usage
#sudo ./uhid-i2c-gamepad -h #display help, -help work as well

#sudo ./uhid-i2c-gamepad -configset js0_enable=1 #set config var value
#sudo ./uhid-i2c-gamepad -configlist #list config vars

#sudo ./uhid-i2c-gamepad -noi2c #disable IRQ, I2C polls and pollrate to stress UHID (generate garbage data)
#sudo ./uhid-i2c-gamepad -nouhid #disable UHID reports
#sudo ./uhid-i2c-gamepad -noi2c -nouhid #overkill benchmark, nothing done but generate garbage data

#sudo ./uhid-i2c-gamepad-diag #diagnostic program
