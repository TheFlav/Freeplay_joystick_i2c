// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Freeplay i2c Joystick driver
 *   This Freeplay i2c Joystick is built into the Freeplay Zero/CM4
 *   and it is on standalone DIY PCBs.
 *
 *   Where further functionality or customization is needed, it's
 *     suggested to use a user-space UHID driver.
 *
 * Copyright (C) 2022 Edward Mandy (freeplaytech@gmail.com)
 *                    https://www.freeplaytech.com/contact/
 *
 * See https://github.com/TheFlav/Freeplay_joystick_i2c_attiny
 *
 */




#define DRV_NAME "freeplay-joystick"

//#include <linux/bits.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>


#define FREEPLAY_JOY_REGISTER_INDEX		0x00
#define FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX	0x09
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_ANALOG	9
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL 3


#define FREEPLAY_JOY_AXIS_MIN  0
#define FREEPLAY_JOY_AXIS_MAX  0b111111111111    //the Freeplay Joy will always return 12-bit resolution
#define FREEPLAY_JOY_AXIS_FUZZ 16                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c
#define FREEPLAY_JOY_AXIS_FLAT 10                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c

#define FREEPLAY_JOY_POLL_FAST_MS 8      //targetting 125Hz
#define FREEPLAY_JOY_POLL_SLOW_MS 1000	//targetting 1Hz (just to make sure any missed interrupt will still get serviced and not lock things up)

#define FREEPLAY_MAX_DIGITAL_BUTTONS 17
static unsigned int button_codes[FREEPLAY_MAX_DIGITAL_BUTTONS] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_START, BTN_SELECT, BTN_TL, BTN_TR, BTN_MODE, BTN_TL2, BTN_TR2, BTN_THUMBL, BTN_THUMBR, BTN_2, BTN_3, BTN_0, BTN_1};

#define BTN_INDEX_A 0
#define BTN_INDEX_B 1
#define BTN_INDEX_X 2
#define BTN_INDEX_Y 3
#define BTN_INDEX_START 4
#define BTN_INDEX_SELECT 5
#define BTN_INDEX_TL 6
#define BTN_INDEX_TR 7
#define BTN_INDEX_MODE 8
#define BTN_INDEX_TL2 9
#define BTN_INDEX_TR2 10
#define BTN_INDEX_THUMBL 11
#define BTN_INDEX_THUMBR 12
#define BTN_INDEX_2 13
#define BTN_INDEX_3 14
#define BTN_INDEX_0 15
#define BTN_INDEX_1 16

#define INPUT0_BTN_X      (1 << 0)      //IO0_0
#define INPUT0_BTN_Y      (1 << 1)      //IO0_1
#define INPUT0_BTN_START  (1 << 2)      //IO0_2
#define INPUT0_BTN_SELECT (1 << 3)      //IO0_3
#define INPUT0_BTN_TL     (1 << 4)      //IO0_4
#define INPUT0_BTN_TR     (1 << 5)      //IO0_5
#define INPUT0_BTN_A      (1 << 6)      //IO0_6
#define INPUT0_BTN_B      (1 << 7)      //IO0_7

#define INPUT1_DPAD_UP    (1 << 0)      //IO1_0
#define INPUT1_DPAD_DOWN  (1 << 1)      //IO1_1
#define INPUT1_DPAD_LEFT  (1 << 2)      //IO1_2
#define INPUT1_DPAD_RIGHT (1 << 3)      //IO1_3
#define INPUT1_BTN_TL2    (1 << 4)      //IO1_4
#define INPUT1_BTN_TR2    (1 << 5)      //IO1_5
#define INPUT1_BTN_MODE   (1 << 6)      //IO1_6


//the chip returns 0 for pressed, so "invert" bits
#define IS_PRESSED_BTN_A(i0) ((i0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define IS_PRESSED_BTN_B(i0) ((i0 & INPUT0_BTN_B) != INPUT0_BTN_B)
#define IS_PRESSED_BTN_X(i0) ((i0 & INPUT0_BTN_X) != INPUT0_BTN_X)
#define IS_PRESSED_BTN_Y(i0) ((i0 & INPUT0_BTN_Y) != INPUT0_BTN_Y)
#define IS_PRESSED_BTN_START(i0) ((i0 & INPUT0_BTN_START) != INPUT0_BTN_START)
#define IS_PRESSED_BTN_SELECT(i0) ((i0 & INPUT0_BTN_SELECT) != INPUT0_BTN_SELECT)
#define IS_PRESSED_BTN_TL(i0) ((i0 & INPUT0_BTN_TL) != INPUT0_BTN_TL)
#define IS_PRESSED_BTN_TR(i0) ((i0 & INPUT0_BTN_TR) != INPUT0_BTN_TR)

#define IS_PRESSED_BTN_TL2(i1) ((i1 & INPUT1_BTN_TL2) != INPUT1_BTN_TL2)
#define IS_PRESSED_BTN_TR2(i1) ((i1 & INPUT1_BTN_TR2) != INPUT1_BTN_TR2)
#define IS_PRESSED_BTN_MODE(i1) ((i1 & INPUT1_BTN_MODE) != INPUT1_BTN_MODE)



struct joystick_params_struct
{
    u32 min;
    u32 max;
    u32 fuzz;
    u32 flat;
    u32 inverted;
};
    
struct freeplay_joy {
    char phys[32];
    struct input_dev *dev;
    struct i2c_client *client;
    
    u32 num_analogsticks;    //default to digital only 0=digital, 1=single analog stick (2ADCs), 2=dual analog sticks (4ADCs)
    u32 num_digitalbuttons;  //number of digital inputs to turn on and report to the system
    u32 num_dpads;           //1=process/report dpad up/down/left/right, 0=no dpad
    
    struct joystick_params_struct joy0_x;
    struct joystick_params_struct joy0_y;
    struct joystick_params_struct joy1_x;
    struct joystick_params_struct joy1_y;

    
    u32 joy0_swapped_x_y;
    u32 joy1_swapped_x_y;
};

struct freeplay_i2c_register_struct
{
    uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
    uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
    uint8_t input2;          // Reg: 0x03 - INPUT port 2 (extended digital buttons)     BTN_Z and BTN_C among other things
    uint8_t a0_msb;          // Reg: 0x04 - ADC0 most significant 8 bits
    uint8_t a1_msb;          // Reg: 0x05 - ADC1 most significant 8 bits
    uint8_t a1a0_lsb;        // Reg: 0x06 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
    uint8_t a2_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
    uint8_t a3_msb;          // Reg: 0x08 - ADC2 most significant 8 bits
    uint8_t a3a2_lsb;        // Reg: 0x09 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
    uint8_t adc_conf_bits;   // Reg: 0x09 - R/W
    //             High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
    //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
    //                                                     (but can only turn ON ADCs that are present on in the high nibble.)
    uint8_t config0;         // Reg: 0x0A - R/W
    //             config register (turn on/off PB4 resistor ladder)
    uint8_t adc_res;         // Reg: 0x0B - current ADC resolution (even though the above a0-a3 will always return 12-bit)
    uint8_t rfu0;            // Reg: 0x0C - reserved for future use (or device-specific use)
    uint8_t manuf_ID;        // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
    uint8_t device_ID;       // Reg: 0x0E -
    uint8_t version_ID;      // Reg: 0x0F -
};

void fpjoy_report_digital_inputs(struct input_dev *input, u8 num_digitalbuttons, u8 num_dpads, u8 input0, u8 input1, u8 input2, bool sync_when_done)
{
    bool dpad_l, dpad_r, dpad_u, dpad_d;
    bool button_states[FREEPLAY_MAX_DIGITAL_BUTTONS];
    u8 button_index = 0;


    
    //digital input0
    button_states[BTN_INDEX_A] = IS_PRESSED_BTN_A(input0);
    button_states[BTN_INDEX_B] = IS_PRESSED_BTN_B(input0);
    button_states[BTN_INDEX_X] = IS_PRESSED_BTN_X(input0);
    button_states[BTN_INDEX_Y] = IS_PRESSED_BTN_Y(input0);
    button_states[BTN_INDEX_START] = IS_PRESSED_BTN_START(input0);
    button_states[BTN_INDEX_SELECT] = IS_PRESSED_BTN_SELECT(input0);
    button_states[BTN_INDEX_TL] = IS_PRESSED_BTN_TL(input0);
    button_states[BTN_INDEX_TR] = IS_PRESSED_BTN_TR(input0);

    //digital input1
    button_states[BTN_INDEX_TL2] = IS_PRESSED_BTN_TL2(input1);
    button_states[BTN_INDEX_TR2] = IS_PRESSED_BTN_TR2(input1);
    button_states[BTN_INDEX_MODE] = IS_PRESSED_BTN_MODE(input1);

    //digital input2

    for(button_index = 0; button_index < num_digitalbuttons; button_index++)
    {
        input_report_key(input, button_codes[button_index], button_states[button_index]);
    }
    
    
    if(num_dpads > 0)
    {
        //dpad_r = 
        input_report_abs(input, ABS_HAT0X, dpad_r - dpad_l);
        input_report_abs(input, ABS_HAT0Y, dpad_u - dpad_d);
    }
    
    if(sync_when_done)
        input_sync(input);
}

static irqreturn_t fpjoy_irq(int irq, void *irq_data)
{
    struct freeplay_joy *priv = irq_data;
    //	struct i2c_client *client = priv->client;
    struct input_dev *input = priv->dev;
    struct freeplay_i2c_register_struct regs;

    int err;
    
    //        dev_info(&priv->client->dev, "Freeplay i2c Joystick, fpjoy_irq\n");
    
    err = i2c_smbus_read_i2c_block_data(priv->client, FREEPLAY_JOY_REGISTER_INDEX,
                                        FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL, (u8 *)&regs);       //only poll the FREEPLAY_JOY_REGISTER_POLL_SIZE
    
    if (err != FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL)      //don't use the registers past FREEPLAY_JOY_REGISTER_POLL_SIZE for polling
    {
        dev_info(&priv->client->dev, "Freeplay i2c Joystick, fpjoy_irq: err=%d\n", err);
        return -1;//IRQ_UNHANDLED;
    }
    
    fpjoy_report_digital_inputs(input, priv->num_digitalbuttons, priv->num_dpads, regs.input0, regs.input1, regs.input2, true);
    
    
    return IRQ_HANDLED;
}

static void freeplay_i2c_poll(struct input_dev *input)
{
    struct freeplay_joy *priv = input_get_drvdata(input);
    struct freeplay_i2c_register_struct regs;
    int err;
    uint16_t adc0, adc1, adc2, adc3;
    bool btn_a, btn_b, btn_power, btn_x, btn_y, btn_l, btn_r, btn_start, btn_select, btn_l2, btn_r2, btn_thumbl, btn_thumbr;
    bool dpad_l, dpad_r, dpad_u, dpad_d;
    uint8_t temp_byte;
    
    //TODO:  Break this into analog/digital and maybe even allow for turning on/off adc_conf_bits
    err = i2c_smbus_read_i2c_block_data(priv->client, FREEPLAY_JOY_REGISTER_INDEX,
                                        FREEPLAY_JOY_REGISTER_POLL_SIZE_ANALOG, (u8 *)&regs);	//only poll the FREEPLAY_JOY_REGISTER_POLL_SIZE
    
    if (err != FREEPLAY_JOY_REGISTER_POLL_SIZE_ANALOG)	//don't use the registers past FREEPLAY_JOY_REGISTER_POLL_SIZE for polling
        return;
    
    /* The i2c device has registers for 4 ADC values (jeft joystick X, left joystick Y, right joystick x, and right joystick y).
     * These are 12-bit integers,
     *   but they are presented as the most-significant 8 bits of x,
     *   followed by the most significant 8 bits of y,
     *   followed by a byte that is broken into 2 nibbles holding the
     *      least significant 4 bits of x followed by the
     *      least significant 4 bits of y
     * and then repeated for the other (right) analog stick
     */
    adc0 = (regs.a0_msb << 4) | (regs.a1a0_lsb & 0x0F);
    adc1 = (regs.a1_msb << 4) | (regs.a1a0_lsb >> 4);
    adc2 = (regs.a2_msb << 4) | (regs.a3a2_lsb & 0x0F);
    adc3 = (regs.a3_msb << 4) | (regs.a3a2_lsb >> 4);
    
    input_report_abs(input, ABS_X, adc0);
    input_report_abs(input, ABS_Y, adc1);
    input_report_abs(input, ABS_RX, adc2);
    input_report_abs(input, ABS_RY, adc3);
    
    //digital input0
    temp_byte = regs.input0;
    temp_byte = ~temp_byte;		//the chip returns 0 for pressed, so invert all bits
    btn_x = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_y = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_start = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_select = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_l = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_r = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_a = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_b = temp_byte & 0b1;
    
    //digital input1
    temp_byte = regs.input1;
    temp_byte = ~temp_byte;         //the chip returns 0 for pressed, so invert all bits
    dpad_u = temp_byte & 0b1;
    temp_byte >>= 1;
    dpad_d = temp_byte & 0b1;
    temp_byte >>= 1;
    dpad_l = temp_byte & 0b1;
    temp_byte >>= 1;
    dpad_r = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_l2 = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_r2 = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_power = temp_byte & 0b1;
    
    //digital input2
    temp_byte = regs.input2;
    temp_byte = ~temp_byte;         //the chip returns 0 for pressed, so invert all bits
    btn_thumbl = temp_byte & 0b1;
    temp_byte >>= 1;
    btn_thumbr = temp_byte & 0b1;
    
    
    input_report_key(input, BTN_A, btn_a);
    input_report_key(input, BTN_B, btn_b);
    input_report_key(input, BTN_X, btn_x);
    input_report_key(input, BTN_Y, btn_y);
    input_report_key(input, BTN_START, btn_start);
    input_report_key(input, BTN_SELECT, btn_select);
    input_report_key(input, BTN_TL, btn_l);
    input_report_key(input, BTN_TR, btn_r);
    input_report_key(input, BTN_MODE, btn_power);
    
    input_report_key(input, BTN_TL2, btn_l2);
    input_report_key(input, BTN_TR2, btn_r2);
    
    input_report_key(input, BTN_THUMBL, btn_thumbl);
    input_report_key(input, BTN_THUMBR, btn_thumbr);
    
    input_report_abs(input, ABS_HAT0X, dpad_r - dpad_l);
    input_report_abs(input, ABS_HAT0Y, dpad_u - dpad_d);
    
    input_sync(input);
}

static int freeplay_probe(struct i2c_client *client)
{
    struct freeplay_joy *priv;
    struct freeplay_i2c_register_struct regs;
    int err;
    u8 adc_mask;
    u8 i;
    bool irq_installed;
    
    dev_info(&client->dev, "Freeplay i2c Joystick: probe\n");
    dev_info(&client->dev, "Freeplay i2c Joystick: interrupt=%d\n", client->irq);
    
    err = i2c_smbus_read_i2c_block_data(client, FREEPLAY_JOY_REGISTER_INDEX,
                                        sizeof(regs), (u8 *)&regs);
    if (err < 0)
        return err;
    if (err != sizeof(regs))
        return -EIO;
    
    dev_info(&client->dev, "Freeplay i2c Joystick, ManufID: 0x%02X, DeviceID: 0x%02X, Ver: %u\n",
             regs.manuf_ID, regs.device_ID, regs.version_ID);
    
    priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;
    
    err = device_property_read_u32(&client->dev, "num-analogsticks", &priv->num_analogsticks);
    if(err)
    {
        priv->num_analogsticks = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error reading analogsticks property\n");
    }

    err = device_property_read_u32(&client->dev, "num-digitalbuttons", &priv->num_digitalbuttons);
    if(err)
    {
        priv->num_digitalbuttons = 11;
        dev_info(&client->dev, "Freeplay i2c Joystick: error reading digitalbuttons property\n");
    }

    err = device_property_read_u32(&client->dev, "num-dpads", &priv->num_dpads);
    if(err)
    {
        priv->num_dpads = 1;
        dev_info(&client->dev, "Freeplay i2c Joystick: error reading num-dpads property\n");
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick: analogsticks=%d digitalbuttons=%d dpad=%d\n", priv->num_analogsticks, priv->num_digitalbuttons, priv->num_dpads);

    
    err = device_property_read_u32_array(&client->dev, "joy0-x-params", (u32 *)&priv->joy0_x, sizeof(priv->joy0_x) / 4);
    if (err) {
        priv->joy0_x.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy0_x.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy0_x.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy0_x.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy0_x.inverted = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error %d reading joy0-x-params property\n", err);
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick 0 X: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy0_x.min, priv->joy0_x.max, priv->joy0_x.fuzz, priv->joy0_x.flat, priv->joy0_x.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy0-y-params", (u32 *)&priv->joy0_y, sizeof(priv->joy0_y) / 4);
    if (err) {
        priv->joy0_y.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy0_y.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy0_y.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy0_y.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy0_y.inverted = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error %d reading joy0-y-params property\n", err);
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick 0 Y: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy0_y.min, priv->joy0_y.max, priv->joy0_y.fuzz, priv->joy0_y.flat, priv->joy0_y.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy1-x-params", (u32 *)&priv->joy1_x, sizeof(priv->joy1_x) / 4);
    if (err) {
        priv->joy1_x.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy1_x.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy1_x.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy1_x.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy1_x.inverted = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error %d reading joy1-x-params property\n", err);
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick 1 X: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy1_x.min, priv->joy1_x.max, priv->joy1_x.fuzz, priv->joy1_x.flat, priv->joy1_x.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy1-y-params", (u32 *)&priv->joy1_y, sizeof(priv->joy1_y) / 4);
    if (err) {
        priv->joy1_y.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy1_y.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy1_y.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy1_y.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy1_y.inverted = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error %d reading joy1-y-params property\n", err);
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick 1 Y: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy1_y.min, priv->joy1_y.max, priv->joy1_y.fuzz, priv->joy1_y.flat, priv->joy1_y.inverted);

    
    err = device_property_read_u32(&client->dev, "joy0-swapped-x-y", &priv->joy0_swapped_x_y);
    if(err)
    {
        priv->joy0_swapped_x_y = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error reading joy0_swapped_x_y property\n");
    }
    
    err = device_property_read_u32(&client->dev, "joy1-swapped-x-y", &priv->joy1_swapped_x_y);
    if(err)
    {
        priv->joy1_swapped_x_y = 0;
        dev_info(&client->dev, "Freeplay i2c Joystick: error reading joy1_swapped_x_y property\n");
    }
    
    if(priv->num_analogsticks == 1)
        adc_mask = 0x03;
    else if(priv->num_analogsticks == 2)
        adc_mask = 0x0F;
    else
        adc_mask = 0x00;


    err = i2c_smbus_write_byte_data(client, FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX, adc_mask);       //turn on requested ADCs
    if (err < 0)
        return err;
    
    err = i2c_smbus_read_byte_data(client, FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX);
    if (err < 0)
        return err;
    dev_info(&client->dev, "Freeplay i2c Joystick: adc_conf=0x%02X\n", err);
    
    priv->client = client;
    snprintf(priv->phys, sizeof(priv->phys),
             "i2c/%s", dev_name(&client->dev));
    i2c_set_clientdata(client, priv);
    
    priv->dev = devm_input_allocate_device(&client->dev);
    if (!priv->dev)
        return -ENOMEM;
    
    priv->dev->id.bustype = BUS_I2C;
    priv->dev->name = "Freeplay i2c Joystick";
    priv->dev->phys = priv->phys;
    input_set_drvdata(priv->dev, priv);
    
    //left analog stick
    if(priv->num_analogsticks >= 1)
    {
        input_set_abs_params(priv->dev, ABS_X,  priv->joy0_x.min, priv->joy0_x.max, priv->joy0_x.fuzz, priv->joy0_x.flat);
        input_set_abs_params(priv->dev, ABS_Y,  priv->joy0_y.min, priv->joy0_y.max, priv->joy0_y.fuzz, priv->joy0_y.flat);
    }
    
    //right analog stick
    if(priv->num_analogsticks >= 2)
    {
        input_set_abs_params(priv->dev, ABS_RX, priv->joy1_x.min, priv->joy1_x.max, priv->joy1_x.fuzz, priv->joy1_x.flat);
        input_set_abs_params(priv->dev, ABS_RY, priv->joy1_y.min, priv->joy1_y.max, priv->joy1_y.fuzz, priv->joy1_y.flat);
    }
    
    //dpad
    if(priv->num_dpads > 0)
    {
        input_set_abs_params(priv->dev, ABS_HAT0X, -1, 1, 0, 0);
        input_set_abs_params(priv->dev, ABS_HAT0Y, -1, 1, 0, 0);
    }
    
    for(i=0; i<priv->num_digitalbuttons; i++)
    {
        input_set_capability(priv->dev, EV_KEY, button_codes[i]);
    }
    
    if(fpjoy_irq >= 0)
    {
        err = devm_request_threaded_irq(&client->dev, client->irq, NULL, fpjoy_irq,
                                        IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, priv);
        if (err) {
            dev_err(&client->dev, "Unable to request Freeplay joystick IRQ, err: %d\n", err);
            irq_installed = false;
        }
        else
        {
            irq_installed = true;
        }
    }
    else
    {
        irq_installed = false;
    }
    
    if(!irq_installed || priv->num_analogsticks > 0)
    {
        //if we aren't using an IRQ
        //  OR we are doing any analog
        //  then set up fast polling
        err = input_setup_polling(priv->dev, freeplay_i2c_poll);
        if (err) {
            dev_err(&client->dev, "failed to set up Freeplay joystick polling: %d\n", err);
            return err;
        }
       
        input_set_poll_interval(priv->dev, FREEPLAY_JOY_POLL_FAST_MS);
        input_set_min_poll_interval(priv->dev, FREEPLAY_JOY_POLL_FAST_MS-2);
        input_set_max_poll_interval(priv->dev, FREEPLAY_JOY_POLL_FAST_MS+2);
    }
    else
    {
        //we could just skip polling, but maybe we'll do it real slow just in case we miss something
        err = input_setup_polling(priv->dev, freeplay_i2c_poll);
        if (err) {
            dev_err(&client->dev, "failed to set up Freeplay joystick polling: %d\n", err);
            return err;
        }
       
        input_set_poll_interval(priv->dev, FREEPLAY_JOY_POLL_SLOW_MS);
        input_set_min_poll_interval(priv->dev, FREEPLAY_JOY_POLL_SLOW_MS-2);
        input_set_max_poll_interval(priv->dev, FREEPLAY_JOY_POLL_SLOW_MS+2);
    }
    
    err = input_register_device(priv->dev);
    if (err) {
        dev_err(&client->dev, "failed to register Freeplay joystick: %d\n", err);
        return err;
    }
    
    
    
    return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_freeplay_match[] = {
    { .compatible = "freeplaytech,freeplay-joystick", },
    { },
};
MODULE_DEVICE_TABLE(of, of_freeplay_match);
#endif /* CONFIG_OF */

static const struct i2c_device_id freeplay_id_table[] = {
    { KBUILD_MODNAME, 0 },
    { },
};
MODULE_DEVICE_TABLE(i2c, freeplay_id_table);

static struct i2c_driver freeplay_driver = {
    .driver = {
        .name		= DRV_NAME,
        .of_match_table	= of_match_ptr(of_freeplay_match),
    },
    .id_table	= freeplay_id_table,
    .probe_new	= freeplay_probe,
};
module_i2c_driver(freeplay_driver);

MODULE_AUTHOR("Ed Mandy <freeplaytech@gmail.com>");
MODULE_DESCRIPTION("Freeplay i2c Joystick driver");
MODULE_LICENSE("GPL");
