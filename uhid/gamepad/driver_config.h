/*
* UHID driver configuration file
*/

#pragma once

bool debug = false; //enable debug output
bool debug_adv = false; //enable advanced debug output, benchmark
bool i2c_poll_rate_disable = false; //allow full throttle update if true

int i2c_bus = 1; //I2C bus
int i2c_addr = 0x30; //main MCU I2C address
int i2c_addr_sec = 0x40; //secondary MCU I2C address
int i2c_addr_adc[] = {0xFF, 0xFF, 0xFF, 0xFF}; //external ADC I2C address, set to 0xFF to disable
bool i2c_ignore_busy = true; //ignore errno 1

int nINT_GPIO = 40; //gpio pin used for IRQ, limited to 31 for pigpio, set to -1 to disable
#define USE_WIRINGPI_IRQ //uncomment to use wiringPi for IRQ
//#define USE_PIGPIO_IRQ //or USE_PIGPIO
//comment both to disable

char cfg_filename[PATH_MAX] = "config.cfg"; //configuration filename
char shm_path[] = "/dev/shm/uhid_i2c_driver/"; //SHM path used for temporary storage
int user_uid = 1000; //normal "non-root" user id, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
int user_gid = 1000; //normal "non-root" group id, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

int i2c_poll_rate = 125; //Driver pollrate in hz
int i2c_adc_poll = 1; //poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on
const char* uhid_device_name = "Freeplay Gamepad"; //HID driver reported name

const uint8_t i2c_dev_manuf = 0xED; //MCU manufacturer signature, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
const int input_regs_count = 3; //amount of registers dedicated to digital inputs

int mcu_input_dpad_start_index = 0; //index position in merged input, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define uhid_buttons_count 15 //HID gamepad buttons count, limited to 16 by driver design, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
#define uhid_buttons_misc_count 4 //HID multiaxis buttons count (BTN_num), limited to 8 by driver design, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
int16_t mcu_input_map[] = { //v8 used to remap MCU output to ev order, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
	-127, -127, -127, -127, BTN_START, BTN_SELECT, BTN_A, BTN_B, //input0
	BTN_MODE, BTN_THUMBR, BTN_TL2, BTN_TR2, BTN_X, BTN_Y, BTN_TL, BTN_TR, //input1. 4xDpad, BTN_TL2/TR2 shared with serial TX/RX, BTN_Z shared with nINT
	BTN_THUMBL, -127, BTN_C, BTN_Z, BTN_0, BTN_1, BTN_2, BTN_3, //input2. BTN_0-3 are part of Multiaxis
};

char* tty_buttons_names[uhid_buttons_count] = {"A","B","C","X","Y","Z","TL","TR","TL2","TR2","SELECT","START","MODE","THUMBL","THUMBR"}; //button naming for diag mode, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
char* tty_buttons_misc_names[uhid_buttons_misc_count] = {"BTN_0","BTN_1","BTN_2","BTN_3"}; //button naming for diag mode, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING

int digital_debounce = 5; //debounce filtering to mitigate possible pad false contact, default:5, max:7, 0 to disable
