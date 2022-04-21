#cleanup
rm uhid-i2c-gamepad
rm uhid-i2c-gamepad-diag

#compile without IRQ support
#gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c

#compile with wiringPi IRQ
gcc -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi

#compile diagnostic program
gcc -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c

#preprocessor variables
# ALLOW_MCU_SEC_I2C : MCU secondary features, on-the-fly I2C address update, backlight control, ...
# USE_SHM_REGISTERS : SHM to MCU bridge, allow to direct update some registers using file system.
# ALLOW_EXT_ADC : External ADCs.
# USE_POLL_IRQ_PIN : IRQ pin poll using WiringPi.
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