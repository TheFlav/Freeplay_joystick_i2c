/*
* FreeplayTech UHID gamepad driver
* Default driver values, only used on driver first run or if diagnostic mode default button selected
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <linux/input-event-codes.h>

//default values
#define def_debug false //enable debug output
#define def_debug_adv false //enable advanced debug output, benchmark

#define def_irq_gpio 40 //gpio pin used for IRQ, limited to 31 for pigpio, set to -1 to disable

#define i2c_ignore_busy true //allow i2c running on busy address
#define def_i2c_bus 1 //I2C bus
#define def_i2c_addr 0x30 //main MCU I2C address
#define def_i2c_addr_sec 0x40 //secondary MCU I2C address

#define cfg_filename "config.cfg" //configuration filename
#define shm_path "/dev/shm/uhid_i2c_driver/" //SHM path used for temporary storage
#define user_uid 1000 //normal "non-root" user id, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define user_gid 1000 //normal "non-root" group id, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

#define uhid_device_name "Freeplay Gamepad" //UHID driver reported name
#define uhid_device_path "/dev/uhid" //UHID device path, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING, may be platform specific

#define def_i2c_poll_rate 125 //Driver pollrate in hz, set to 0 to allow full throttle update
#define def_i2c_adc_poll 1 //poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on

#define i2c_dev_manuf 0xED //MCU manufacturer signature, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define input_registers_count 3 //amount of registers dedicated to digital inputs, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define input_registers_size input_registers_count*8 //full size of input registers, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

#define mcu_input_dpad_start_index 0 //index position in merged input, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define uhid_buttons_count 15 //HID gamepad buttons count, limited to 16 by driver design, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define uhid_buttons_misc_count 4 //HID multiaxis buttons count (BTN_num), limited to 8 by driver design, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
static int16_t mcu_input_map[input_registers_size] = { //driver v11 input register used to remap MCU output to proper ev order, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
	-127, -127, -127, -127, BTN_START, BTN_SELECT, BTN_A, BTN_B, //input0
	BTN_MODE, BTN_THUMBR, BTN_TL2, BTN_TR2, BTN_X, BTN_Y, BTN_TL, BTN_TR, //input1. 4xDpad, BTN_TL2/TR2 shared with serial TX/RX, BTN_Z shared with nINT
	BTN_THUMBL, -127, BTN_C, BTN_Z, BTN_0, BTN_1, BTN_2, BTN_3, //input2. BTN_0-3 are part of Multiaxis
};

#define def_digital_debounce 5 //debounce filtering to mitigate possible pad false contact, default:5, max:7, 0 to disable

//adc to joystick map defaults. -1:disabled axis, 0:X1, 1:Y1, 2:X2, 3:Y2
#define def_adc0_map 0 //adc0
#define def_adc1_map 1 //adc1
#define def_adc2_map 2 //adc2
#define def_adc3_map 3 //adc3

//adc0 defaults
#define def_adc0_i2c_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined
#define def_adc0_enabled false
#define def_adc0_min 0 
#define def_adc0_max 4095
#define def_adc0_res 12 //bits
#define def_adc0_fuzz 24
#define def_adc0_flat 10 //percent
#define def_adc0_reversed false
#define def_adc0_autocenter false

//adc1 defaults
#define def_adc1_i2c_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined
#define def_adc1_enabled false
#define def_adc1_min 0 
#define def_adc1_max 4095
#define def_adc1_res 12 //bits
#define def_adc1_fuzz 24
#define def_adc1_flat 10 //percent
#define def_adc1_reversed false
#define def_adc1_autocenter false

//adc2 defaults
#define def_adc2_i2c_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined
#define def_adc2_enabled false
#define def_adc2_min 0 
#define def_adc2_max 4095
#define def_adc2_res 12 //bits
#define def_adc2_fuzz 24
#define def_adc2_flat 10 //percent
#define def_adc2_reversed false
#define def_adc2_autocenter false

//adc3 defaults
#define def_adc3_i2c_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined
#define def_adc3_enabled false
#define def_adc3_min 0 
#define def_adc3_max 4095
#define def_adc3_res 12 //bits
#define def_adc3_fuzz 24
#define def_adc3_flat 10 //percent
#define def_adc3_reversed false
#define def_adc3_autocenter false

