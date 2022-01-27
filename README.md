# Freeplay_joystick_i2c
        Freeplay i2c joystick firmware, kernel-space driver, and user-space driver        

## Arduino code
        ./arduino/megatinycore          Arduino sketch for use with megaTinyCore (attiny427, attiny817, attiny1627, etc.)

## kernel joystick driver to read i2c (polled and/or interrupts)
        ./kernel                        kernel module
        ./kernel/device-tree            device-tree blob overlay

## userspace uhid program to poll i2c and inject HID digital/analog joystick/gamepad
        ./uhid/digital                  simple example code for digital-only pad
        ./uhid/analog                   simple example code for dual analog 
        ./uhid/gamepad                  full-featured program for calibration and interfacing many features
        
        Example
                Setup:     sudo apt install libi2c-dev pigpio
                Compile:   gcc -o uhid-i2c-digital uhid-i2c-digital.c -li2c -lpigpio
                Run:       sudo ./uhid-i2c-digital 
