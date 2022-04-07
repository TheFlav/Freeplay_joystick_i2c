/*
* FreeplayTech UHID gamepad driver
*
* Contain default driver values.
* Only used on driver first run to create configuartion file or if running setup/diagnostic program to enable reset to default.
*
* For I2C registers, please refer to "driver_i2c_registers.h"
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <linux/input-event-codes.h>

//setup/diagnostic specific
#define diag_program_filename "uhid-i2c-gamepad-diag" //has to match actual setup/diagnostic program name, used to start diag if config missing to setup ADCs
#define diag_first_run_command "-init" //argument used to set setup/diagnostic program into 'first run' mode to setup ADCs
#define diag_input_mcu_read_interval 0.15 //interval to process MCU digital inputs for menu navigation, may have to be finetuned as overshot happen very quickly

//driver default settings/values
#define def_debug false //enable debug output
#define def_debug_adv false //enable advanced debug output, benchmark

#define def_irq_gpio 40 //gpio pin used for digital input interrupt, set to -1 to disable, "USE_POLL_IRQ_PIN" needs to be defined in compilation command line

#define i2c_ignore_busy true //allow i2c running on busy address
#define def_i2c_bus 1 //I2C bus
#define def_i2c_bus_path_format "/dev/i2c-%d" //path to i2c bus, follow printf format rules
#define def_mcu_search true //enable search of proper MCU address if provided one fails.
#define def_mcu_addr 0x30 //main MCU I2C address
#define def_mcu_addr_sec 0x40 //secondary MCU I2C address, "ALLOW_MCU_SEC_I2C" needs to be defined in compilation command line

#define cfg_filename "config.cfg" //configuration filename
#define cfg_mcu_addr_name "mcu_address" //MCU main I2C address field name into config file
#define cfg_mcu_addr_sec_name "mcu_address_sec" //MCU secondary I2C address field name into config file, "ALLOW_MCU_SEC_I2C" needs to be defined in compilation command line

#define shm_path "/dev/shm/uhid_i2c_driver/" //SHM path used for temporary storage
#define user_uid 1000 //normal "non-root" user id, 1000 for 'pi' user, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define user_gid 1000 //normal "non-root" group id, 1000 for 'pi' group, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

#define uhid_device_name "Freeplay Gamepad" //UHID device reported name
#define uhid_device_path "/dev/uhid" //UHID device path, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING, may be platform specific

#define def_i2c_poll_rate 125 //Driver pollrate in hz, set to 0 to disable limitation.
#define def_i2c_adc_poll 1 //poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on...

#define mcu_manuf 0xED //MCU manufacturer signature, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define input_registers_count 3 //amount of registers dedicated to digital inputs, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define input_registers_size input_registers_count*8 //full size of input registers, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

#define mcu_input_dpad_start_index 0 //index position in merged input, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define uhid_buttons_count 15 //HID gamepad buttons count, limited to 16 by driver design, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
//#define uhid_buttons_misc_enabled //comment to disable BTN_0 to whatever report
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
#define def_adc0_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined in compilation command line
#define def_adc0_type 0 //external adc type identifier
#define def_adc0_enabled false
#define def_adc0_min 0 
#define def_adc0_max 4095
#define def_adc0_res 12 //bits
#define def_adc0_fuzz 24
#define def_adc0_flat 10 //percent
#define def_adc0_reversed false
#define def_adc0_autocenter false

//adc1 defaults
#define def_adc1_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined in compilation command line
#define def_adc1_type 0 //external adc type identifier
#define def_adc1_enabled false
#define def_adc1_min 0 
#define def_adc1_max 4095
#define def_adc1_res 12 //bits
#define def_adc1_fuzz 24
#define def_adc1_flat 10 //percent
#define def_adc1_reversed false
#define def_adc1_autocenter false

//adc2 defaults
#define def_adc2_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined in compilation command line
#define def_adc2_type 0 //external adc type identifier
#define def_adc2_enabled false
#define def_adc2_min 0 
#define def_adc2_max 4095
#define def_adc2_res 12 //bits
#define def_adc2_fuzz 24
#define def_adc2_flat 10 //percent
#define def_adc2_reversed false
#define def_adc2_autocenter false

//adc3 defaults
#define def_adc3_addr 0xFF //external I2C address, set to 0xFF to disable, "ALLOW_EXT_ADC" needs to be defined in compilation command line
#define def_adc3_type 0 //external adc type identifier
#define def_adc3_enabled false
#define def_adc3_min 0 
#define def_adc3_max 4095
#define def_adc3_res 12 //bits
#define def_adc3_fuzz 24
#define def_adc3_flat 10 //percent
#define def_adc3_reversed false
#define def_adc3_autocenter false

//program exit codes
#define EXIT_FAILED_GENERIC -1 //failed to something somewhere
#define EXIT_FAILED_I2C -2 //failed to found/connect to MCU
#define EXIT_FAILED_MANUF -3 //invalid MCU manuf
#define EXIT_FAILED_VERSION -4 //MCU version < program register verion
#define EXIT_FAILED_CONFIG -5 //failed to read/save configuration
#define EXIT_FAILED_MCU -6 //generic fail of mcu



