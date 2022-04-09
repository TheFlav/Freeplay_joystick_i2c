# Freeplay UHID Gamepad driver

## Introduction:  
This set of programs does allow to emulate a USB device using UHID device.  
  
Using this method have the benefit to allow faster compilation and debugging (incl. programing on Windows platform without messing with GDB) compared to a kernel driver but comes with some limitations linked to security in place on Linux systems (.e.g needs to run as root to access needed device).  
  
In its current state, this driver mainly targeting Raspberry Pi boards (additional comments on this later).  
<br>
  
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
Provided commands will compile driver to ``uhid-i2c-gamepad`` and Setup/Diag. program to ``uhid-i2c-gamepad-diag``.  
  
  - Driver : Basic with limited features (no IRQ support)  
  ```gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c```  
  <br>

  - Driver : Enable use of WiringPi IRQ  
  ```gcc -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi```  
  <br>

  - Driver : WiringPi IRQ and MCU features  
  ```gcc -DALLOW_MCU_SEC_I2C -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi```  
  <br>

  - Setup/Diag. : Basic with limited features  
  ```gcc -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c```  
  <br>

  - Setup/Diag. : MCU features  
  ```gcc -DALLOW_MCU_SEC_I2C -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c```  
  <br>

## Usage:
### Arguments:
  - Common :
    * ``-h`` or ``-help``  
    Show program usage.  
    <br>
    
  - Driver specific :  
    (*) : close program after function executed (including failures).
    <br>

    * ``-confignocreate``  
    Disable creation of configuration file if it doesn't exist.  
    Program closes and returns specific error code if so (next section).  

    * ``-configreset`` (*)  
    Reset configuration file to default defined in **driver_config.h**.  
    
    * ``-configset`` (*)  
    Set specified configuration variable with given value.  
    Format: ``VARIABLE=VALUE`` (no space, use argument once at a time)  
    Example: ``uhid-i2c-gamepad -configset debug=1``  
    
    * ``-configlist`` (*)  
    List all configuration variables.  
    
    * ``-inputsearch`` (*)  
    Check MCU input for pressed button, use [input-event-codes.h](https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/input-event-codes.h) numbering.  
    Example for (START) and (SELECT) : ``-inputsearch 0x13b 0x13a``  
    This argument needs to be defined last.  
    Program returns **0** if none pressed, **1** for first input detected, **2** for second, **3** for both.  
    
    * ``-closeonwarn``  
    Close program on major warnings.
    <br>
    
    * ``-quiet`` or ``--quiet``  
    Disable any program outputs.
    <br>
    
    * ``-noi2c``  
    Benchmark: Disable IRQ, I2C polls and pollrate, generate garbage data to UHID device. (can crash EVDEV monitoring softwares).
    <br>
    
    * ``-nouhid``  
    Benchmark: Disable IRQ, UHID reports and pollrate. (can crash EV monitoring softwares).
    <br>
    
  - Setup/Diag. specific :
    * ``-init``  
    Allow easier detection of analog settings.
    Warning: does discard already set ADC to joystick mapping, min/max values as well as axis reverse.  
    <br>
  
### Return codes of driver:
Since driver is meant to run during system boot process, it does return specific (no following common rules) to allow some sort of feedback.  
  
Theses are in place mainly to allow creation of script(s) to upload/update MCU program and allow user to be redirected to Setup/Diag. program during device first boot.  
  
Theses codes are mainly defined at the end of [driver_config.h](driver_config.h)  

Practical example: https://github.com/TheFlav/Freeplay_joystick_i2c/blob/main/scripts/bootup.sh










