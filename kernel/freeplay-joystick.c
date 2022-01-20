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


#define FREEPLAY_JOY_REGISTER_INDEX		0x00
#define FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX	0x09
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_ANALOG	9
#define FREEPLAY_JOY_REGISTER_POLL_SIZE_DIGITAL 3


#define FREEPLAY_JOY_AXIS_MAX  0b111111111111    //the Freeplay Joy will always return 12-bit resolution
#define FREEPLAY_JOY_AXIS_FUZZ 16                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c
#define FREEPLAY_JOY_AXIS_FLAT 10                //guess based on https://github.com/TheFlav/Freeplay_joystick_i2c_attiny/blob/main/uhid/uhid-i2c-gamepad.c

#define FREEPLAY_JOY_POLL_MS 8	//targetting 125Hz

struct freeplay_joy {
	char phys[32];
	struct input_dev *dev;
	struct i2c_client *client;
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

	dev_info(&client->dev, "Freeplay i2c Joystick: probe\n");

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

	err = i2c_smbus_write_byte_data(client, FREEPLAY_JOY_REGISTER_ADC_CONF_INDEX, 0x0F);//turn on all ADCs
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
	input_set_abs_params(priv->dev, ABS_X,  0, FREEPLAY_JOY_AXIS_MAX, FREEPLAY_JOY_AXIS_FUZZ, FREEPLAY_JOY_AXIS_FLAT);
	input_set_abs_params(priv->dev, ABS_Y,  0, FREEPLAY_JOY_AXIS_MAX, FREEPLAY_JOY_AXIS_FUZZ, FREEPLAY_JOY_AXIS_FLAT);

	//right analog stick
	input_set_abs_params(priv->dev, ABS_RX, 0, FREEPLAY_JOY_AXIS_MAX, FREEPLAY_JOY_AXIS_FUZZ, FREEPLAY_JOY_AXIS_FLAT);
	input_set_abs_params(priv->dev, ABS_RY, 0, FREEPLAY_JOY_AXIS_MAX, FREEPLAY_JOY_AXIS_FUZZ, FREEPLAY_JOY_AXIS_FLAT);

	//dpad
        input_set_abs_params(priv->dev, ABS_HAT0X, -1, 1, 0, 0);
        input_set_abs_params(priv->dev, ABS_HAT0Y, -1, 1, 0, 0);

	//buttons in input0
	input_set_capability(priv->dev, EV_KEY, BTN_A);
	input_set_capability(priv->dev, EV_KEY, BTN_B);
	input_set_capability(priv->dev, EV_KEY, BTN_X);
	input_set_capability(priv->dev, EV_KEY, BTN_Y);
	input_set_capability(priv->dev, EV_KEY, BTN_START);
	input_set_capability(priv->dev, EV_KEY, BTN_SELECT);
	input_set_capability(priv->dev, EV_KEY, BTN_TL);
	input_set_capability(priv->dev, EV_KEY, BTN_TR);

	//buttons in input1 (dpad is also in input1)
        input_set_capability(priv->dev, EV_KEY, BTN_MODE);	//power button used for hotkey stuff
        input_set_capability(priv->dev, EV_KEY, BTN_TL2);
        input_set_capability(priv->dev, EV_KEY, BTN_TR2);

	//buttons in input2
	input_set_capability(priv->dev, EV_KEY, BTN_THUMBL);
	input_set_capability(priv->dev, EV_KEY, BTN_THUMBR);
	//input_set_capability(priv->dev, EV_KEY, BTN_0);		//if ADC0 is unused
	//input_set_capability(priv->dev, EV_KEY, BTN_1);         //if ADC1 is unused
	//input_set_capability(priv->dev, EV_KEY, BTN_2);         //if ADC2 is unused
	//input_set_capability(priv->dev, EV_KEY, BTN_3);         //if ADC3 is unused

	err = input_setup_polling(priv->dev, freeplay_i2c_poll);
	if (err) {
		dev_err(&client->dev, "failed to set up Freeplay joystick polling: %d\n", err);
		return err;
	}
	input_set_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS);
	input_set_min_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS-2);
	input_set_max_poll_interval(priv->dev, FREEPLAY_JOY_POLL_MS+2);

	err = input_register_device(priv->dev);
	if (err) {
		dev_err(&client->dev, "failed to register Freeplay joystick: %d\n", err);
		return err;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id of_freeplay_match[] = {
	{ .compatible = "freeplay,freeplay-joystick", },
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
