# Freeplay UHID Gamepad driver

## Introduction:  
This set of programs does allow to emulate a USB input device using UHID device.  
  
Using this method have the benefit to allow faster compilation and debugging (incl. programing on Windows platform without messing with GDB) compared to a kernel driver but comes with some limitations linked to security in place on Linux systems (.e.g needs to run as root to access needed device).  
  
In its current state, this driver mainly targeting Raspberry Pi boards (additional comments on this later).  

This page does mainly reference 2 programs:
- ``uhid-i2c-gamepad`` : UHID input driver.
- ``uhid-i2c-gamepad-diag`` : Setup/Diagnostic program.  
  Does allow end-user to edit and set most of editable settings.  
  It doesn't edit any of the .c or .h files.  
  This program is meant to run on at least 640x480 screen (80 cols x 30 lines TTY).  
<br>
  
## Compilation:
### Required libraries
  - ``libi2c-dev``
  - ``wiringpi`` : please refer to ``USE_POLL_IRQ_PIN``  
<br>

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
    * Preimplement.
    * Allow to interface I2C device register(s) to given file(s).
    * Require programming of specific ADC ICs interfacing.  
    * Please refer to [driver_main.h](driver_main.h) (shm_vars[] var) for structure.  
  <br>

  - ``ALLOW_EXT_ADC``
    * Preimplement.
    * Enable usage of external (relative to MCU) ADCs.
    * Require programming of specific ADC ICs interfacing
    * Please refer to [driver_adc_external.h](driver_adc_external.h) for more informations.  
  <br>

  - ``DIAG_PROGRAM``
    * Compile Setup/Diagnostic part of the driver.
    * ``USE_POLL_IRQ_PIN`` and ``USE_SHM_REGISTERS`` variables will be discarded as not used in this part.  
  <br>

  - ``MULTI_INSTANCES``
    * Allow driver and setup/diag program to run more than once at a time.
    * New instance (once first already during) needs to use a different configuration file (no check for this, UHID may fail), please refer to argument ``-config`` to do so.
  <br><br>

### Examples:
Provided commands will compile driver to ``uhid-i2c-gamepad`` and Setup/Diag. program to ``uhid-i2c-gamepad-diag``.  
  
  - Driver : Basic with limited features (no IRQ support)  
    ```
    gcc -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c
    ```

  - Driver : Enable use of WiringPi IRQ  
    ```
    gcc -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi
    ```

  - Driver : WiringPi IRQ and MCU features  
    ```
    gcc -DALLOW_MCU_SEC_I2C -DUSE_POLL_IRQ_PIN -o uhid-i2c-gamepad nns_config.c uhid-i2c-gamepad.c -li2c -lwiringPi
    ```

  - Setup/Diag. : Basic with limited features  
    ```
    gcc -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c
    ```

  - Setup/Diag. : MCU features  
    ```
    gcc -DALLOW_MCU_SEC_I2C -DDIAG_PROGRAM -o uhid-i2c-gamepad-diag nns_config.c driver_diag.c uhid-i2c-gamepad.c -li2c
    ```
<br>

## Usage:
### Arguments:
  - Common :
    * ``-h`` or ``-help`` : Show program usage.  

    * ``-config`` : Set path (relative or absolute) to configuration file (including filename).  
    Needs to be the very first argument set.  
    Folder must aleady exist.  
    <br>
    
  - Driver specific :  
    (*) : close program after function executed (including failures).
    <br>

    * ``-confignocreate`` : Disable creation of configuration file if it doesn't exist.  
    Program closes and returns specific error code if so (next section).  

    * ``-configreset`` (*) : Reset configuration file to default defined in [driver_config.h](driver_config.h).  
    
    * ``-configset`` (*) : Set specified configuration variable with given value.  
    Format: ``VARIABLE=VALUE`` (no space, use argument once at a time)  
    Example: ``uhid-i2c-gamepad -configset debug=1``  
    
    * ``-configlist`` (*) : List all configuration variables.  
    
    * ``-inputsearch`` (*) : Check MCU input for pressed button, use [input-event-codes.h](https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/input-event-codes.h) numbering.  
    Example for (START) and (SELECT) : ``-inputsearch 0x13b 0x13a``  
    This argument needs to be defined last.  
    Program returns **0** if none pressed, **1** for first input detected, **2** for second, **3** for both.  
    
    * ``-closeonwarn`` : Close program on major warnings.
    <br>
    
    * ``-quiet`` or ``--quiet`` : Disable any program outputs.
    <br>
    
    * ``-noi2c`` : Benchmark: Disable IRQ, I2C polls and pollrate, generate garbage data to UHID device. (can crash EVDEV monitoring softwares).
    <br>
    
    * ``-nouhid`` : Benchmark: Disable IRQ, UHID reports and pollrate. (can crash EV monitoring softwares).
    <br>
    
  - Setup/Diag. specific :
    * ``-init`` : First run mode.  
    Allow easier detection of analog settings.  
    Warning: does discard already set ADC to joystick mapping, min/max values as well as axis reverse.  
    Program will run in this mode if no configuration file exists.  
<br>

### Return codes of driver (not Setup/Diag program):
  Since driver is meant to run during system boot process, it does return specific (no following common rules) to allow some sort of feedback.  
    
  Theses are in place mainly to allow creation of script(s) to upload/update MCU program and allow user to be redirected to Setup/Diag. program during device first boot.  
    
  Theses codes are mainly defined at the end of [driver_config.h](driver_config.h)  

  Practical example: https://github.com/TheFlav/Freeplay_joystick_i2c/blob/main/scripts/bootup.sh

  Exception to following rules : ``-inputsearch`` (previous section).  

- Return list:  
  (*) Please consider to reflash MCU.  
  * ``0`` : Everything is fine.  
    
  * ``-1`` : Undefined or generic failure.  
    
  * ``-2`` : I2C failed : This can be linked to multiple thing. Wrong Bus or Adress, Important read/write command failed (*).  
    
  * ``-3`` : MCU **manuf_ID** register mismatched  **mcu_manuf** variable ([driver_config.h](driver_config.h)) (*).  
    
  * ``-4`` : MCU version (**version_ID** register) under **mcu_version_even** variable ([driver_i2c_registers.h](driver_i2c_registers.h)), should be considered as outdated MCU (*).  
    
  * ``-5`` : Failed to read, write or parse configuration file.  
  If this happen when ``-confignocreate`` argument used, this mainly mean configuration file doesn't exist.  
  This return code can be used to send user to Setup/Diag program first run mode.  
    
  * ``-6`` : Undefined or generic MCU related failure.  
  This code is a bit tricky as it is extremely vague.  
  Needs some investigations if it happen.  
    
  * ``-7`` : Program is already running.  
  Driver or setup/diag program are limited to one instance per program at once if not compiled with preprocessor variable ``MULTI_INSTANCES``.  
<br>

### Specific behaves:
- Driver and Setup/diag program running at the same time:  
  * While driver is running, if Setup/diag program starts, the driver will enter into "lock state".  
  * When this happen, a last report will be pushed to UHID device to set all inputs to unpressed and center analog values, after this no more report will be done until the "lock state" reset.  
  * This reset will happen when Setup/diag program closes.  
<br>
- Enter Setup/Diag program first run mode:
  * Run program with configuration file not existing.
  * Run program with argument ``-init`` set.  
<br>
- Needs to running drivers at once with multiple MCUs (to be tested):
  * Programs to be compiled with preprocessor variable ``MULTI_INSTANCES``.
  * Each instance to have its own configuration file set with ``-config`` argument.
  * Run each once to create configuration file, close driver, edit ``uhid_device_id`` variable in config. files to have different driver IDs, this ID will be added after driver name report to EV/JS dev.  
<br>

## Repository files
- **driver_config.h** : User default driver settings.
- **driver_hid_desc.h** : HID descriptor definition.
- **driver_i2c_registers.h** : MCU register and some other shared structure.
- **uhid-i2c-gamepad.c**/**driver_main.h** : Driver files.
- **driver_diag.c**/**driver_diag.h** : Setup/Diagnostic files.
- **driver_debug_print.h** : Shared debug output functions.
- **nns_config.c**/**nns_config.h** : Shared configuration functions.
- **build-gamepad.sh** : Example compilation script.
<br>

