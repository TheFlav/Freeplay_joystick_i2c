# Freeplay UHID Gamepad driver

## Introduction:  
This set of programs does allow to emulate a USB device using UHID device.  
  
Using this method have the benefit to allow faster compilation and debugging (incl. programing on Windows platform without messing with GDB) compared to a kernel driver but comes with some limitations linked to security in place on Linux systems (.e.g needs to run as root to access needed device).  
  
In its current state, this driver mainly targeting Raspberry Pi boards (additional comments on this later).  
  



This part of the repository consist of 2 programs:
- ``uhid-i2c-gamepad`` : UHID driver itself
- ``uhid-i2c-gamepad-diag`` : Setup/diagnostic to allow end-user to edit MCU, input, analogs settings using keyboard or MCU inputs.

## Compilation:
### Required libraries
  - ``libi2c-dev``
  - ``wiringpi`` : please refer to ``USE_POLL_IRQ_PIN``
  <br><br>

### Preprocessor variable (gcc -D) to enable features:
Some parts of the driver are in place as preimplement to ease future additions and improvements or use outside of FreeplayTech products line.  

  - ``USE_POLL_IRQ_PIN``
    * Allow to poll MCU IRQ pin using WiringPi library.  
    * ``-lwiringPi`` needs to be added to compilation command line.  
    * **Note for Pi Zero 2**: You may need to clone and compile for unofficial github repository as official WiringPi ended development, please refer to: https://github.com/PinkFreud/WiringPi  
  <br>

  - ``ALLOW_MCU_SEC_I2C``
    * Use MCU secondary features.
    * Allow better detection of I2C addresses if theses are misconfigured by user.
    * Setup/Diag. : Allow LED output/Backlight control, on-the-fly I2C addresses update (main and secondary).  
  <br>

  - ``USE_SHM_REGISTERS``
    * Preimplement
    * Allow to interface I2C device register(s) to given file(s).
    * Require programming of specific ADC ICs interfacing.  
    * Please refer to **driver_main.h** (shm_vars[] var) for structure.  
  <br>

  - ``ALLOW_EXT_ADC``
    * Preimplement
    * Enable usage of external (relative to MCU) ADCs.
    * Require programming of specific ADC ICs interfacing
    * Please refer to **driver_adc_external.h** for more informations.  
  <br>

  - ``DIAG_PROGRAM``
    * Compile Setup/Diagnostic part of the driver.
    * ``USE_POLL_IRQ_PIN`` and ``USE_SHM_REGISTERS`` variables will be discarded as not used in this part.
  <br><br>

### Examples:
  - Driver : Basic with limited features (no IRQ support)  
  `gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c`  
  <br>

  - Driver : Enable use of WiringPi IRQ  
  ``gcc -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi``  
  <br>

  - Driver : WiringPi IRQ and MCU features  
  ``gcc -DALLOW_MCU_SEC_I2C -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi``  
  <br>

  - Setup/Diag. : Basic with limited features  
  ``gcc -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c``  
  <br>

  - Setup/Diag. : MCU features  
  ``gcc -DALLOW_MCU_SEC_I2C -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c``  
  <br>

