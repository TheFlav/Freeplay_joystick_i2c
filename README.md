# Freeplay_joystick_i2c_attiny
Freeplay i2c joystick firmware using attiny817 or attiny427 

## uhid program to poll i2c and inject HID Gamepad included
        Setup:     sudo apt install libi2c-dev
        Compile:   gcc -o uhid-i2c-gamepad uhid-i2c-gamepad.c -li2c
        Run:       sudo ./uhid-i2c-gamepad 
