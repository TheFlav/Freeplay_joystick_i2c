/*
* FreeplayTech UHID gamepad driver
* I2C registers header
*
* Only edit if you know what you are doing
* "i2c_joystick_register_struct" and "i2c_secondary_address_register_struct" have to reflect registers stuctures defined in MCU source code.
*/

#pragma once

#define mcu_version_even 24 //what version of mcu is even with this file 

struct i2c_joystick_register_struct {
    uint8_t input0;         // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
    uint8_t input1;         // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
    uint8_t input2;         // Reg: 0x02 - INPUT port 2 (extended digital buttons)     BTN_THUMBL and BTN_THUMBR among other things
    uint8_t a0_msb;         // Reg: 0x03 - ADC0 most significant 8 bits
    uint8_t a1_msb;         // Reg: 0x04 - ADC1 most significant 8 bits
    uint8_t a1a0_lsb;       // Reg: 0x05 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
    uint8_t a2_msb;         // Reg: 0x06 - ADC2 most significant 8 bits
    uint8_t a3_msb;         // Reg: 0x07 - ADC2 most significant 8 bits
    uint8_t a3a2_lsb;       // Reg: 0x08 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
    uint8_t adc_conf_bits;  // Reg: 0x09 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                            //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                            //             (but can only turn ON ADCs that are turned on in the high nibble.)
    uint8_t config0;        // Reg: 0x0A - config register //maybe allow PA4-7 to be digital inputs connected to input2  config0[7]=use_extended_inputs
    uint8_t adc_res;        // Reg: 0x0B - current ADC resolution (maybe settable?)
    uint8_t rfu0;           // Reg: 0x0C - reserved for future use (or device-specific use)
    uint8_t manuf_ID;       // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
    uint8_t device_ID;      // Reg: 0x0E -
    uint8_t version_ID;     // Reg: 0x0F - 
};

#ifdef ALLOW_MCU_SEC_I2C
    #define mcu_write_protect_enable 0xAA //write_protect register value to restore write protection
    #define mcu_write_protect_disable 0x55 //write_protect register value to remove write protection

    struct i2c_secondary_address_register_struct {
        uint8_t config_backlight;   // Reg: 0x00
        uint8_t backlight_max;      // Reg: 0x01 
        uint8_t power_control;      // Reg: 0x02 - host can tell us stuff about the state of the power (like low-batt or shutdown imminent) or even tell us to force a shutdown
        uint8_t features_available; // Reg: 0x03 - bit define if ADCs are available or interrups are in use, etc.
        uint8_t rfu0;               // Reg: 0x04 - reserved for future use (or device-specific use)
        uint8_t rfu1;               // Reg: 0x05 - reserved for future use (or device-specific use)
        uint8_t rfu2;               // Reg: 0x06 - reserved for future use (or device-specific use)
        uint8_t rfu3;               // Reg: 0x07 - reserved for future use (or device-specific use)
        uint8_t battery_capacity;   // Reg: 0x08 - battery capacity (0-100 = battery %, 255 = UNKNOWN)
        uint8_t status_led_control; // Reg: 0x09 - turn on/off/blinkSlow/blinkFast/etc the blue status LED  (this is actuall a WRITE-ONLY "virtual" register)
        uint8_t write_protect;      // Reg: 0x0A - write 0x00 to make protected registers writeable
        uint8_t secondary_i2c_addr; // Reg: 0x0B - this holds the secondary i2c address (the address where this struct can be found)
        uint8_t joystick_i2c_addr;  // Reg: 0x0C - this holds the primary (joystick's) i2c address
        uint8_t manuf_ID;           // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
        uint8_t device_ID;          // Reg: 0x0E -
        uint8_t version_ID;         // Reg: 0x0F - 
    };

    //power_control
    typedef union {
        struct {uint8_t low_batt_mode:1, lcd_dimming_mode:1, lcd_sleep_mode:1, unused3:1, unused4:1, unused5:1, unused6:1, unused7:1;} vals;
        uint8_t bits;
    } mcu_power_control_t;
    #define power_control_pos_low_batt 0
    #define power_control_pos_lcd_dimming 1
    #define power_control_pos_lcd_sleep 2
#endif

//common structures, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
typedef union { //config0 register bitfield structure
    struct {uint8_t debounce_level:3, unused3:1, unused4:1, unused5:1, unused6:1, unused7:1;} vals;
    uint8_t bits;
} mcu_config0_t;

typedef struct adc_data_struct_t { //ADC read data structure
    int raw, raw_prev, raw_min, raw_max; //store raw values for min-max report
    unsigned char res; unsigned int res_limit; //adc resolution/limit set during runtime
    int value, min, max, offset; //current value, min/max limits, computed offset
    int fuzz, flat_in, flat_out; //fuzz, inside/outside flat
    int flat_in_comp, flat_out_comp; //computed recurring values
    bool enabled, reversed, autocenter; //enabled, reverse reading, autocenter: check adc value once to set as offset
} adc_data_t;
