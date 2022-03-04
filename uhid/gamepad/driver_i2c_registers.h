/*
* UHID driver i2c register file
* Only edit if you know what you are doing
*/

#pragma once

struct i2c_joystick_register_struct {
  uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
  uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
  uint8_t input2;          // Reg: 0x03 - INPUT port 2 (extended digital buttons)     BTN_THUMBL and BTN_THUMBR among other things
  uint8_t a0_msb;          // Reg: 0x04 - ADC0 most significant 8 bits
  uint8_t a1_msb;          // Reg: 0x05 - ADC1 most significant 8 bits
  uint8_t a1a0_lsb;        // Reg: 0x06 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
  uint8_t a2_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
  uint8_t a3_msb;          // Reg: 0x08 - ADC2 most significant 8 bits
  uint8_t a3a2_lsb;        // Reg: 0x09 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
  uint8_t adc_conf_bits;   // Reg: 0x09 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                           //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                           //             (but can only turn ON ADCs that are turned on in the high nibble.)
  uint8_t config0;         // Reg: 0x0A - config register //maybe allow PA4-7 to be digital inputs connected to input2  config0[7]=use_extended_inputs
  uint8_t adc_res;         // Reg: 0x0B - current ADC resolution (maybe settable?)
  uint8_t rfu0;            // Reg: 0x0C - reserved for future use (or device-specific use)
  uint8_t manuf_ID;        // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
  uint8_t device_ID;       // Reg: 0x0E -
  uint8_t version_ID;      // Reg: 0x0F - 
} volatile i2c_joystick_registers;


struct i2c_secondary_address_register_struct {
  uint8_t config_backlight;  // Reg: 0x00
  uint8_t backlight_max;     // Reg: 0x01 
  uint8_t power_control;     // Reg: 0x02 - host can tell us stuff about the state of the power (like low-batt or shutdown imminent) or even tell us to force a shutdown
  uint8_t features_available;// Reg: 0x03 - bit define if ADCs are available or interrups are in use, etc.
  uint8_t rfu0;              // Reg: 0x04 - reserved for future use (or device-specific use)
  uint8_t rfu1;              // Reg: 0x05 - reserved for future use (or device-specific use)
  uint8_t rfu2;              // Reg: 0x06 - reserved for future use (or device-specific use)
  uint8_t rfu3;              // Reg: 0x07 - reserved for future use (or device-specific use)
  uint8_t rfu4;              // Reg: 0x08 - reserved for future use (or device-specific use)
  uint8_t rfu5;              // Reg: 0x09 - reserved for future use (or device-specific use)
  uint8_t rfu6;              // Reg: 0x0A - reserved for future use (or device-specific use)
  uint8_t secondary_i2c_addr; // Reg: 0x0B - this holds the secondary i2c address (the address where this struct can be found)
  uint8_t joystick_i2c_addr; // Reg: 0x0C - this holds the primary (joystick's) i2c address
  uint8_t manuf_ID;          // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
  uint8_t device_ID;         // Reg: 0x0E -
  uint8_t version_ID;        // Reg: 0x0F - 
} i2c_secondary_registers;
