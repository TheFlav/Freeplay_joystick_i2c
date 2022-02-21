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

//#define DEBUG


#define FREEPLAY_JOY_REGISTER_DIGITAL_INDEX		0x00
#define FREEPLAY_JOY_REGISTER_ANALOG_INDEX      0x04
#define FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX	0x09
#define FREEPLAY_JOY_REGISTER_CONFIG0_INDEX     0x0A
#define FREEPLAY_JOY_REGISTER_BASE_INDEX        0x00

#define FREEPLAY_JOY_REGISTER_CONFIG0_THUMB_ON (1<<7)

#define FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL2 2
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL3 3
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_SINGLE_ANALOG 3
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_DUAL_ANALOG 6


#define FREEPLAY_JOY_AXIS_MIN  0
#define FREEPLAY_JOY_AXIS_MAX  0b111111111111    //the Freeplay Joy will always return 12-bit resolution
#define FREEPLAY_JOY_AXIS_FUZZ 16                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c
#define FREEPLAY_JOY_AXIS_FLAT 10                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c

#define FREEPLAY_POLL_TARGET_HZ   125
#define FREEPLAY_POLL_DIGITAL_DIVISOR (FREEPLAY_POLL_TARGET_HZ / 1)             //targetting 1Hz
#define FREEPLAY_JOY_POLL_MS (1000 / FREEPLAY_POLL_TARGET_HZ)                   //8ms = (1000 / FREEPLAY_POLL_TARGET_HZ) = targetting 125Hz

#define FREEPLAY_MAX_DIGITAL_BUTTONS 17
static unsigned int button_codes[FREEPLAY_MAX_DIGITAL_BUTTONS] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_START, BTN_SELECT, BTN_TL, BTN_TR, BTN_MODE, BTN_TL2, BTN_TR2, BTN_THUMBL, BTN_THUMBR, BTN_C, BTN_Z, BTN_0, BTN_1};

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
#define BTN_INDEX_C 13
#define BTN_INDEX_Z 14
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

#define INPUT2_BTN_THUMBL (1 << 0)      //IO2_0
#define INPUT2_BTN_THUMBR (1 << 1)      //IO2_1
                                        //IO2_2
                                        //IO2_3
#define INPUT2_BTN_0      (1 << 4)      //IO2_4
#define INPUT2_BTN_1      (1 << 5)      //IO2_5
#define INPUT2_BTN_C      (1 << 6)      //IO2_6
#define INPUT0_BTN_Z      (1 << 7)      //IO2_7



//the chip returns 0 for pressed, so "invert" bits
#define INPUT0_IS_PRESSED_BTN_A(i0) ((i0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define INPUT0_IS_PRESSED_BTN_B(i0) ((i0 & INPUT0_BTN_B) != INPUT0_BTN_B)
#define INPUT0_IS_PRESSED_BTN_X(i0) ((i0 & INPUT0_BTN_X) != INPUT0_BTN_X)
#define INPUT0_IS_PRESSED_BTN_Y(i0) ((i0 & INPUT0_BTN_Y) != INPUT0_BTN_Y)
#define INPUT0_IS_PRESSED_BTN_START(i0) ((i0 & INPUT0_BTN_START) != INPUT0_BTN_START)
#define INPUT0_IS_PRESSED_BTN_SELECT(i0) ((i0 & INPUT0_BTN_SELECT) != INPUT0_BTN_SELECT)
#define INPUT0_IS_PRESSED_BTN_TL(i0) ((i0 & INPUT0_BTN_TL) != INPUT0_BTN_TL)
#define INPUT0_IS_PRESSED_BTN_TR(i0) ((i0 & INPUT0_BTN_TR) != INPUT0_BTN_TR)

#define INPUT1_IS_PRESSED_DPAD_UP(i1) ((i1 & INPUT1_DPAD_UP) != INPUT1_DPAD_UP)
#define INPUT1_IS_PRESSED_DPAD_DOWN(i1) ((i1 & INPUT1_DPAD_DOWN) != INPUT1_DPAD_DOWN)
#define INPUT1_IS_PRESSED_DPAD_LEFT(i1) ((i1 & INPUT1_DPAD_LEFT) != INPUT1_DPAD_LEFT)
#define INPUT1_IS_PRESSED_DPAD_RIGHT(i1) ((i1 & INPUT1_DPAD_RIGHT) != INPUT1_DPAD_RIGHT)

#define INPUT1_IS_PRESSED_BTN_TL2(i1) ((i1 & INPUT1_BTN_TL2) != INPUT1_BTN_TL2)
#define INPUT1_IS_PRESSED_BTN_TR2(i1) ((i1 & INPUT1_BTN_TR2) != INPUT1_BTN_TR2)
#define INPUT1_IS_PRESSED_BTN_MODE(i1) ((i1 & INPUT1_BTN_MODE) != INPUT1_BTN_MODE)

#define INPUT2_IS_PRESSED_BTN_THUMBL(i0) ((i0 & INPUT2_BTN_THUMBL) != INPUT2_BTN_THUMBL)
#define INPUT2_IS_PRESSED_BTN_THUMBR(i0) ((i0 & INPUT2_BTN_THUMBR) != INPUT2_BTN_THUMBR)

#define INPUT2_IS_PRESSED_BTN_0(i0) ((i0 & INPUT2_BTN_0) != INPUT2_BTN_0)
#define INPUT2_IS_PRESSED_BTN_1(i0) ((i0 & INPUT2_BTN_1) != INPUT2_BTN_1)
#define INPUT2_IS_PRESSED_BTN_C(i0) ((i0 & INPUT2_BTN_C) != INPUT2_BTN_C)
#define INPUT2_IS_PRESSED_BTN_Z(i0) ((i0 & INPUT0_BTN_Z) != INPUT0_BTN_Z)


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
    
    u8 poll_iterations_digital_skip;
    
#ifdef DEBUG
    u16 adc0_detected_min;
    u16 adc0_detected_max;
    u16 adc1_detected_min;
    u16 adc1_detected_max;
    u16 adc2_detected_min;
    u16 adc2_detected_max;
    u16 adc3_detected_min;
    u16 adc3_detected_max;
#endif
    
    bool using_irq_for_digital_inputs;
};

struct freeplay_i2c_register_struct
{
    u8 input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
    u8 input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
    u8 input2;          // Reg: 0x03 - INPUT port 2 (extended digital buttons)     BTN_Z and BTN_C among other things
    u8 a0_msb;          // Reg: 0x04 - ADC0 most significant 8 bits
    u8 a1_msb;          // Reg: 0x05 - ADC1 most significant 8 bits
    u8 a1a0_lsb;        // Reg: 0x06 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
    u8 a2_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
    u8 a3_msb;          // Reg: 0x08 - ADC2 most significant 8 bits
    u8 a3a2_lsb;        // Reg: 0x09 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
    u8 adc_conf_bits;   // Reg: 0x09 - R/W
    //             High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
    //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
    //                                                     (but can only turn ON ADCs that are present on in the high nibble.)
    u8 config0;         // Reg: 0x0A - R/W
    //             config register (turn on/off PB4 resistor ladder)
    u8 adc_res;         // Reg: 0x0B - current ADC resolution (even though the above a0-a3 will always return 12-bit)
    u8 rfu0;            // Reg: 0x0C - reserved for future use (or device-specific use)
    u8 manuf_ID;        // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
    u8 device_ID;       // Reg: 0x0E -
    u8 version_ID;      // Reg: 0x0F -
};

void fpjoy_report_digital_inputs(struct input_dev *input, u8 num_digitalbuttons, u8 num_dpads, u8 input0, u8 input1, u8 input2, bool sync_when_done)
{
    bool dpad_l, dpad_r, dpad_u, dpad_d;
    bool button_states[FREEPLAY_MAX_DIGITAL_BUTTONS];
    u8 button_index = 0;


    
    //digital input0
    button_states[BTN_INDEX_A] = INPUT0_IS_PRESSED_BTN_A(input0);
    button_states[BTN_INDEX_B] = INPUT0_IS_PRESSED_BTN_B(input0);
    button_states[BTN_INDEX_X] = INPUT0_IS_PRESSED_BTN_X(input0);
    button_states[BTN_INDEX_Y] = INPUT0_IS_PRESSED_BTN_Y(input0);
    button_states[BTN_INDEX_START] = INPUT0_IS_PRESSED_BTN_START(input0);
    button_states[BTN_INDEX_SELECT] = INPUT0_IS_PRESSED_BTN_SELECT(input0);
    button_states[BTN_INDEX_TL] = INPUT0_IS_PRESSED_BTN_TL(input0);
    button_states[BTN_INDEX_TR] = INPUT0_IS_PRESSED_BTN_TR(input0);

    //digital input1
    button_states[BTN_INDEX_TL2]  = INPUT1_IS_PRESSED_BTN_TL2(input1);
    button_states[BTN_INDEX_TR2]  = INPUT1_IS_PRESSED_BTN_TR2(input1);
    button_states[BTN_INDEX_MODE] = INPUT1_IS_PRESSED_BTN_MODE(input1);

    //digital input2
    button_states[BTN_INDEX_THUMBL]  = INPUT2_IS_PRESSED_BTN_THUMBL(input2);
    button_states[BTN_INDEX_THUMBR]  = INPUT2_IS_PRESSED_BTN_THUMBR(input2);
    button_states[BTN_INDEX_C]       = INPUT2_IS_PRESSED_BTN_C(input2);
    button_states[BTN_INDEX_Z]       = INPUT2_IS_PRESSED_BTN_Z(input2);
    button_states[BTN_INDEX_0]       = INPUT2_IS_PRESSED_BTN_0(input2);
    button_states[BTN_INDEX_1]       = INPUT2_IS_PRESSED_BTN_1(input2);

    for(button_index = 0; button_index < num_digitalbuttons; button_index++)
    {
        input_report_key(input, button_codes[button_index], button_states[button_index]);
    }
    
    
    if(num_dpads > 0)
    {
        dpad_u = INPUT1_IS_PRESSED_DPAD_UP(input1);
        dpad_d = INPUT1_IS_PRESSED_DPAD_DOWN(input1);
        dpad_l = INPUT1_IS_PRESSED_DPAD_LEFT(input1);
        dpad_r = INPUT1_IS_PRESSED_DPAD_RIGHT(input1);

        input_report_abs(input, ABS_HAT0X, dpad_r - dpad_l);
        input_report_abs(input, ABS_HAT0Y, dpad_u - dpad_d);
    }
    
    if(sync_when_done)
        input_sync(input);
}


/*
 This is the interrupt handler for the freeplay i2c joystick (fpjoy)
 The fpjoy will ONLY generate interrupts for changes in the 3 digital input registers:
    input0, input1, input2
 
 If the system is configured to do analog inputs, then we need to poll analog inputs
    using freeplay_i2c_poll
 
 Digital inputs can be done via polling, too, if desired.
 */
static irqreturn_t fpjoy_irq(int irq, void *irq_data)
{
    struct freeplay_joy *priv = irq_data;
    //	struct i2c_client *client = priv->client;
    struct input_dev *input = priv->dev;
    struct freeplay_i2c_register_struct regs;
    u8 poll_size;


    int err;
    
    if(priv->num_digitalbuttons <= 8 && priv->num_dpads == 0)
    {
        //if we're only using/reporting 8 buttons (or less) AND we're not using the dpad inputs, then we only grab input0
        poll_size = FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL1;
        regs.input1 = 0xFF;
        regs.input2 = 0xFF;
    }
    if(priv->num_digitalbuttons <= 11)
    {
        //if we're only using/reporting 11 buttons (or less), then we only grab input0 and input1 (not input2)
        poll_size = FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL2;
        regs.input2 = 0xFF;
    }
    else
    {
        poll_size = FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL3;
    }
    
    err = i2c_smbus_read_i2c_block_data(priv->client, FREEPLAY_JOY_REGISTER_DIGITAL_INDEX, poll_size, (u8 *)&regs);       //only poll the neded input0-2
    
    if (err != poll_size)      //don't use the registers past FREEPLAY_JOY_REGISTER_POLL_SIZE for polling
    {
        dev_err(&priv->client->dev, "Freeplay i2c Joystick, fpjoy_irq: i2c_smbus_read_i2c_block_data returned %d (requested %d)\n", err, poll_size);
        return IRQ_NONE;
    }
    
    fpjoy_report_digital_inputs(input, priv->num_digitalbuttons, priv->num_dpads, regs.input0, regs.input1, regs.input2, true);
    
    return IRQ_HANDLED;
}

static void freeplay_i2c_get_and_report_inputs(struct input_dev *input, bool poll_digital, bool poll_analog)
{
    struct freeplay_joy *priv = input_get_drvdata(input);
    struct freeplay_i2c_register_struct regs;
    int err;
    u16 adc0, adc1, adc2, adc3;
    u8 poll_size;
    u8 poll_base;
    
    if(!poll_digital && !poll_analog)
        return;  //nothing to do!

    if(poll_digital)
    {
        poll_base = FREEPLAY_JOY_REGISTER_DIGITAL_INDEX;
        poll_size = FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL;
    }
    else
    {
        //analog only
        poll_base = FREEPLAY_JOY_REGISTER_ANALOG_INDEX;
        poll_size = 0;
    }
    

    //for now, we will always poll the digital inputs, AND we can poll any analog inputs that are configured
    if(priv->num_analogsticks == 1)
    {
        poll_size += FREEPLAY_JOY_REGISTER_POLL_SIZE_SINGLE_ANALOG;
    }
    else if(priv->num_analogsticks == 2)
    {
        poll_size += FREEPLAY_JOY_REGISTER_POLL_SIZE_DUAL_ANALOG;
    }
    
    //TODO:  Break this into analog/digital and maybe even allow for turning on/off adc_conf_bits
    err = i2c_smbus_read_i2c_block_data(priv->client, poll_base,
                                        poll_size, ((u8 *)&regs)+poll_base);    //only poll the FREEPLAY_JOY_REGISTER_POLL_SIZE
    
    if (err != poll_size)
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
    if(poll_analog)
    {
        if(priv->num_analogsticks >= 1)
        {
            adc0 = (regs.a0_msb << 4) | (regs.a1a0_lsb & 0x0F);
            adc1 = (regs.a1_msb << 4) | (regs.a1a0_lsb >> 4);
            
#ifdef DEBUG
            if(adc0 < priv->adc0_detected_min)
            {
                priv->adc0_detected_min = adc0;
                dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc0 min=0x%02X\n", priv->adc0_detected_min);
            }
            
            if(adc1 < priv->adc1_detected_min)
            {
                priv->adc1_detected_min = adc1;
                dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc1 min=0x%02X\n", priv->adc1_detected_min);
            }

            if(adc0 > priv->adc0_detected_max)
            {
                priv->adc0_detected_max = adc0;
                dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc0 max=0x%02X\n", priv->adc0_detected_max);
            }

            if(adc1 > priv->adc1_detected_max)
            {
                priv->adc1_detected_max = adc1;
                dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc1 max=0x%02X\n", priv->adc1_detected_max);
            }
#endif
            
            input_report_abs(input, ABS_X, adc0);
            input_report_abs(input, ABS_Y, adc1);
            
            if(priv->num_analogsticks == 2)
            {
                adc2 = (regs.a2_msb << 4) | (regs.a3a2_lsb & 0x0F);
                adc3 = (regs.a3_msb << 4) | (regs.a3a2_lsb >> 4);

#ifdef DEBUG
                if(adc2 < priv->adc2_detected_min)
                {
                    priv->adc2_detected_min = adc2;
                    dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc2 min=0x%02X\n", priv->adc2_detected_min);
                }
                
                if(adc3 < priv->adc3_detected_min)
                {
                    priv->adc3_detected_min = adc3;
                    dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc3 min=0x%02X\n", priv->adc3_detected_min);
                }

                if(adc2 > priv->adc2_detected_max)
                {
                    priv->adc2_detected_max = adc2;
                    dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc2 max=0x%02X\n", priv->adc2_detected_max);
                }

                if(adc3 > priv->adc3_detected_max)
                {
                    priv->adc3_detected_max = adc3;
                    dev_info(&priv->client->dev, "Freeplay i2c Joystick, freeplay_i2c_get_and_report_inputs: new adc3 max=0x%02X\n", priv->adc3_detected_max);
                }
#endif
                
                input_report_abs(input, ABS_RX, adc2);
                input_report_abs(input, ABS_RY, adc3);
            }
        }
    }

    if(poll_digital)
        fpjoy_report_digital_inputs(input, priv->num_digitalbuttons, priv->num_dpads, regs.input0, regs.input1, regs.input2, false);
    
    input_sync(input);
}

static void freeplay_i2c_poll(struct input_dev *input)
{
    struct freeplay_joy *priv = input_get_drvdata(input);
    bool poll_digital;
    bool poll_analog;
    
    if(priv->num_analogsticks == 0)
    {
        poll_analog = false;
    }
    else
    {
        poll_analog = true;
    }

    if(priv->using_irq_for_digital_inputs)
    {
        if(priv->poll_iterations_digital_skip == 0)
        {
            poll_digital = true;
            priv->poll_iterations_digital_skip = FREEPLAY_POLL_DIGITAL_DIVISOR;
        }
        else
            priv->poll_iterations_digital_skip--;
    }
    else
    {
        poll_digital = true;
    }
    


    freeplay_i2c_get_and_report_inputs(input, poll_digital, poll_analog);
}




static int freeplay_probe(struct i2c_client *client)
{
    struct freeplay_joy *priv;
    struct freeplay_i2c_register_struct regs;
    int err;
    u8 adc_mask;
    u8 i;
    u8 new_config0;
    
    err = i2c_smbus_read_i2c_block_data(client, FREEPLAY_JOY_REGISTER_BASE_INDEX,
                                        sizeof(regs), (u8 *)&regs);
    if (err < 0)
    {
        dev_err(&client->dev, "Freeplay i2c Joystick fatal i2c error %d\n", err);
        return err;
    }
    if (err != sizeof(regs))
    {
        dev_err(&client->dev, "Freeplay i2c Joystick fatal i2c IO mismatch.  Received %d bytes.  Requested %d bytes.\n", err, sizeof(regs));
        return -EIO;
    }
    
    dev_info(&client->dev, "Freeplay i2c Joystick Found, ManufID: 0x%02X, DeviceID: 0x%02X, Ver: %u\n", regs.manuf_ID, regs.device_ID, regs.version_ID);
    
    priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
    {
        dev_err(&client->dev, "Freeplay i2c Joystick fatal error.  No memory available for allocation.\n");
        return -ENOMEM;
    }
    
    err = device_property_read_u32(&client->dev, "num-analogsticks", &priv->num_analogsticks);
    if(err)
    {
        priv->num_analogsticks = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error reading analogsticks property\n");
    }
    
    if(priv->num_analogsticks > 2)      //anything over 2 is an error
    {
        priv->num_analogsticks = 0;
    }
        
    

    err = device_property_read_u32(&client->dev, "num-digitalbuttons", &priv->num_digitalbuttons);
    if(err)
    {
        priv->num_digitalbuttons = 11;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error reading digitalbuttons property\n");
    }

    err = device_property_read_u32(&client->dev, "num-dpads", &priv->num_dpads);
    if(err)
    {
        priv->num_dpads = 1;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error reading num-dpads property\n");
    }
    
    err = device_property_read_u32_array(&client->dev, "joy0-x-params", (u32 *)&priv->joy0_x, sizeof(priv->joy0_x) / 4);
    if (err) {
        priv->joy0_x.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy0_x.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy0_x.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy0_x.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy0_x.inverted = 0;
        //dev_err(&client->dev, "Freeplay i2c Joystick: error %d reading joy0-x-params property\n", err);
    }
    
    //dev_dbg(&client->dev, "Freeplay i2c Joystick 0 X: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy0_x.min, priv->joy0_x.max, priv->joy0_x.fuzz, priv->joy0_x.flat, priv->joy0_x.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy0-y-params", (u32 *)&priv->joy0_y, sizeof(priv->joy0_y) / 4);
    if (err) {
        priv->joy0_y.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy0_y.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy0_y.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy0_y.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy0_y.inverted = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error %d reading joy0-y-params property\n", err);
    }
    
    //dev_dbg(&client->dev, "Freeplay i2c Joystick 0 Y: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy0_y.min, priv->joy0_y.max, priv->joy0_y.fuzz, priv->joy0_y.flat, priv->joy0_y.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy1-x-params", (u32 *)&priv->joy1_x, sizeof(priv->joy1_x) / 4);
    if (err) {
        priv->joy1_x.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy1_x.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy1_x.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy1_x.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy1_x.inverted = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error %d reading joy1-x-params property\n", err);
    }
    
    //dev_dbg(&client->dev, "Freeplay i2c Joystick 1 X: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy1_x.min, priv->joy1_x.max, priv->joy1_x.fuzz, priv->joy1_x.flat, priv->joy1_x.inverted);

    
    err = device_property_read_u32_array(&client->dev, "joy1-y-params", (u32 *)&priv->joy1_y, sizeof(priv->joy1_y) / 4);
    if (err) {
        priv->joy1_y.min = FREEPLAY_JOY_AXIS_MIN;
        priv->joy1_y.max = FREEPLAY_JOY_AXIS_MAX;
        priv->joy1_y.fuzz = FREEPLAY_JOY_AXIS_FUZZ;
        priv->joy1_y.flat = FREEPLAY_JOY_AXIS_FLAT;
        priv->joy1_y.inverted = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error %d reading joy1-y-params property\n", err);
    }
    
    //dev_dbg(&client->dev, "Freeplay i2c Joystick 1 Y: min=%d max=%d fuzz=%d flat=%d inverted=%d\n", priv->joy1_y.min, priv->joy1_y.max, priv->joy1_y.fuzz, priv->joy1_y.flat, priv->joy1_y.inverted);

    
    err = device_property_read_u32(&client->dev, "joy0-swapped-x-y", &priv->joy0_swapped_x_y);
    if(err)
    {
        priv->joy0_swapped_x_y = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error reading joy0_swapped_x_y property\n");
    }
    
    err = device_property_read_u32(&client->dev, "joy1-swapped-x-y", &priv->joy1_swapped_x_y);
    if(err)
    {
        priv->joy1_swapped_x_y = 0;
        //dev_dbg(&client->dev, "Freeplay i2c Joystick: error reading joy1_swapped_x_y property\n");
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
    //dev_dbg(&client->dev, "Freeplay i2c Joystick: adc_conf=0x%02X\n", err);

    
    if(priv->num_digitalbuttons > 11)       //if we're using more than 11 buttons, then turn on THUMBL and THUMBR
    {
        new_config0 = regs.config0 | FREEPLAY_JOY_REGISTER_CONFIG0_THUMB_ON;        //turn THUMB support on
    }
    else
    {
        new_config0 = regs.config0 & ~FREEPLAY_JOY_REGISTER_CONFIG0_THUMB_ON;       //turn THUMB support OFF
    }
    

    err = i2c_smbus_write_byte_data(client, FREEPLAY_JOY_REGISTER_CONFIG0_INDEX, new_config0);       //set new_config0
    if (err < 0)
        return err;

    err = i2c_smbus_read_byte_data(client, FREEPLAY_JOY_REGISTER_CONFIG0_INDEX);
    if (err < 0)
        return err;
    //dev_dbg(&client->dev, "Freeplay i2c Joystick: config0=0x%02X (new_config0=0x%02X)\n", err, new_config0);

    
    priv->client = client;
    snprintf(priv->phys, sizeof(priv->phys), "i2c/%s", dev_name(&client->dev));
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
            dev_dbg(&client->dev, "Unable to request Freeplay joystick IRQ, err: %d\n", err);
            priv->using_irq_for_digital_inputs = false;
        }
        else
        {
            priv->using_irq_for_digital_inputs = true;
        }
    }
    else
    {
        priv->using_irq_for_digital_inputs = false;
    }
    
    if(priv->using_irq_for_digital_inputs)
    {
        //if we ARE using an IRQ for digital inputs, then poll analog on a fast timer
        // and poll digital on a slow timer just to make sure we never fully miss something
        
        
        priv->poll_iterations_digital_skip = FREEPLAY_POLL_DIGITAL_DIVISOR;       //we're using IRQ for digital, so try to do only 1 digital poll per second
        
        err = input_setup_polling(priv->dev, freeplay_i2c_poll);
        if (err) {
            dev_err(&client->dev, "failed to set up Freeplay joystick polling: %d\n", err);
            return err;
        }
       
        input_set_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS);
        input_set_min_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS-2);
        input_set_max_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS+2);
    }
    else
    {
        //if we aren't using an IRQ for digital inputs
        // poll digital AND analog on the fast timer
        
        priv->poll_iterations_digital_skip = 0;       //we're NOT using IRQ for digital, poll digital every time

        
        err = input_setup_polling(priv->dev, freeplay_i2c_poll);
        if (err) {
            dev_err(&client->dev, "failed to set up Freeplay joystick polling: %d\n", err);
            return err;
        }
       
        input_set_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS);
        input_set_min_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS-2);
        input_set_max_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS+2);
    }

    
    
    err = input_register_device(priv->dev);
    if (err) {
        dev_err(&client->dev, "failed to register Freeplay joystick: %d\n", err);
        return err;
    }
    
#ifdef DEBUG
    priv->adc0_detected_min = 0xFFF;
    priv->adc0_detected_max = 0x000;
    priv->adc1_detected_min = 0xFFF;
    priv->adc1_detected_max = 0x000;
    priv->adc2_detected_min = 0xFFF;
    priv->adc2_detected_max = 0x000;
    priv->adc3_detected_min = 0xFFF;
    priv->adc3_detected_max = 0x000;
#endif

    
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
