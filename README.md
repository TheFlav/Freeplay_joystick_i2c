# Freeplay_joystick_i2c
This repository does provide files needs to turn a ATtiny MCU into IO/ADC I2C expansion IC with advanced configuration.  
It does also contain Linux kernel-space and user-space drivers to turn it into a input driver (only one kind at once).

MCU code uses [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore) and mainly target following MCUs (may require compilation and upload scripts update): ATtiny417, ATtiny817, ATtiny1617, ATtiny427, ATtiny827, ATtiny1627.

Files provided here are mainly aimed to be used on [Freeplaytech](https://www.freeplaytech.com/) Zero platform (2022 640x480 IPS version) but can also be used on other projects if you want.

Please check following sections for more indepth instructions/informations.
<br><br>

### MCU
- [arduino/Freeplay_joystick_i2c_megatinycore/](arduino/Freeplay_joystick_i2c_megatinycore/) : Arduino sketch with compilation and flash scripts.
<br>

### Kernel-space driver
- [kernel/](kernel/) : Kernel driver files.
- [kernel/device-tree/](kernel/device-tree/) : Device-tree blob overlay.
- [kernel/diag/](kernel/diag/) : Diagnostic/setup program to allow end users do there own settings in a simpler way (use with caution).
<br>

### User-space driver
Note: Require a kernel compiled with UHID support, check existance of ``/dev/uhid`` file on your system to confirm that.  
- [uhid/digital/](uhid/digital/) : Simple example code for digital-only pad.
- [uhid/analog/](uhid/analog/) : Simple example code for dual analog.
- [uhid/gamepad/](uhid/gamepad/) : Full-featured set including driver and diagnostic/setup program for calibration and interfacing many features.
<br>

### Install scripts
- [scripts/](scripts/) : Contain install scripts (mainly aimed at Retropie image).
- [scripts/binary_package/](scripts/binary_package/) : Installation procedure and automated scripts to install already compiled drivers and programs on a fresh image.
<br><br>

## Installation into a Freeplay Zero (2022 640x480 IPS version)
Please follow [binary package instructions](scripts/binary_package/README.md).



