/*
FreeplayTech UHID gamepad driver

This program sets up a gamepad device in the sytem using UHID-kernel interface.
Current version is mainly meant to be used with FreeplayTech gen2 device (embedded ATTINY controller for IO/ADC management).

Notes when using Pi Zero 2 W and willing to use WiringPi for IRQ:
You may need to clone and compile for unofficial github repository as official WiringPi ended developpement, please refer to: https://github.com/PinkFreud/WiringPi
*/

#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <linux/uhid.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <termios.h>

#include "driver_i2c_registers.h"
#include "driver_debug_print.h"
#include "driver_config.h"
#include "nns_config.h"
#ifdef DIAG_PROGRAM
	#undef USE_WIRINGPI_IRQ
	#undef USE_PIGPIO_IRQ
#else
	#include "driver_hid_desc.h"
#endif

#include "driver_main.h"

#if defined(USE_PIGPIO_IRQ) && defined(USE_WIRINGPI_IRQ)
	#error Cannot do both IRQ styles
#elif defined(USE_WIRINGPI_IRQ)
	#include <wiringPi.h>
#elif defined(USE_PIGPIO_IRQ)
	#include <pigpio.h>
#endif


//Debug related functions
/*
static void debug_print_binary_int(int val, int bits, char* var){ //print given var in binary format
	if(!debug) return;
	printf("DEBUG: BIN: %s : ", var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\n");
}

static void debug_print_binary_int_term (int line, int col, int val, int bits, char* var){ //print given var in binary format at given term position
	printf("\e[%d;%dH\e[1;100m%s : ", line, col, var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\e[0m");
}
*/

//Time related functions
double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return -1.; //failed
}

//UHID related functions
#ifndef DIAG_PROGRAM
static int uhid_create(int fd){ //create uhid device
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE2;
	strcpy((char*)ev.u.create2.name, uhid_device_name); //ev.u.create.rd_data = hid_descriptor;
	memcpy(ev.u.create2.rd_data, hid_descriptor, sizeof(hid_descriptor));
	ev.u.create2.rd_size = sizeof(hid_descriptor);
	ev.u.create2.bus = BUS_USB;
	ev.u.create2.vendor = 0x15d9;
	ev.u.create2.product = 0x0a37;
	ev.u.create2.version = 0;
	ev.u.create2.country = 0;

	return uhid_write(fd, &ev);
}

static void uhid_destroy(int fd){ //close uhid devic
	//TODO EmulationStation looks to crash from time to time when closing driver
	if (fd < 0) return; //already closed
	struct uhid_event ev; memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;
	int ret = uhid_write(fd, &ev);
	if (ret < 0){print_stdout("failed to destroy uhid device, errno:%d (%s)\n", -ret, strerror(-ret));}
	print_stdout("uhid device destroyed\n");
}

static int uhid_write(int fd, const struct uhid_event* ev){ //write data to uhid device
	ssize_t ret = write(fd, ev, sizeof(*ev));
	if (ret < 0){print_stderr("write to uhid device failed with errno:%d (%m)\n", -ret);
	} else if (ret != sizeof(*ev)){
		print_stderr("wrong size wrote to uhid device: %zd != %zu\n", ret, sizeof(ev));
		return -EFAULT;
	}
	return ret;
}

static int uhid_send_event(int fd){ //send event to uhid device
	if (uhid_disabled){return 0;}
	if (uhid_fd < 0){return -EIO;}

	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT2;

	const int adc_size = sizeof(uint16_t);
	int index = 0;
	ev.u.input2.data[index++] = gamepad_report.buttons7to0; //digital msb
	ev.u.input2.data[index++] = gamepad_report.buttons15to8; //digital lsb
	ev.u.input2.data[index++] = gamepad_report.hat0; //dpad

	memcpy(&ev.u.input2.data[index], &gamepad_report.left_x, adc_size); //x1
	memcpy(&ev.u.input2.data[index+2], &gamepad_report.left_y, adc_size); //y1
	memcpy(&ev.u.input2.data[index+4], &gamepad_report.right_x, adc_size); //x2
	memcpy(&ev.u.input2.data[index+6], &gamepad_report.right_y, adc_size); //y2
	index += adc_size*4;

	ev.u.input2.data[index++] = gamepad_report.buttonsmisc; //digital misc

	ev.u.input2.size = index;
	return uhid_write(fd, &ev);
}
#endif

//I2C related
int i2c_check_bus(int bus){ //check I2C bus, return 0 on success, -1:addr
	if (bus < 0){print_stderr("invalid I2C bus:%d\n", bus); return -1;} //invalid bus
	char fd_path[13]; sprintf(fd_path, "/dev/i2c-%d", bus);
	int fd = open(fd_path, O_RDWR);
	if (fd < 0){print_stderr("failed to open '%s', errno:%d (%m)\n", fd_path, -fd); return -1; //invalid bus
	} else {print_stdout("I2C bus '%s' found\n", fd_path);}
	close(fd);
	return 0;
}

int i2c_open_dev(int* fd, int bus, int addr){ //open I2C device, return 0 on success, -1:bus, -2:addr, -3:generic error
	if (bus < 0){print_stderr("invalid I2C bus:%d\n", bus); return -1;} //invalid bus
    if (addr < 0 || addr > 127){print_stderr("invalid I2C address:0x%02X (%d)\n", addr, addr); return -2;} //invalid address

	char fd_path[13]; sprintf(fd_path, "/dev/i2c-%d", bus);
	close(*fd); *fd = open(fd_path, O_RDWR);

	int ret = ioctl(*fd, i2c_ignore_busy ? I2C_SLAVE_FORCE : I2C_SLAVE, addr);
	if (ret < 0) {close(*fd); *fd = -1; print_stderr("ioctl failed for address 0x%02X, errno:%d (%m)\n", addr, -ret); return -2;} //invalid address

	ret = i2c_smbus_read_byte_data(*fd, 0);
	if (ret < 0){ //invalid address
		i2c_allerrors_count++; close(*fd); *fd = -2;
		print_stderr("failed to read from address 0x%02X, errno:%d (%m)\n", addr, -ret);
		return -2;
	}

	print_stdout("I2C address:0x%02X opened\n", addr);
	return 0;
}

void i2c_close_all(void){ //close all I2C files
	int* fd_array[] = {&mcu_fd,
	#ifdef ALLOW_MCU_SEC
		&mcu_fd_sec,
	#endif
	#ifdef ALLOW_EXT_ADC
		&adc_fd[0], &adc_fd[1], &adc_fd[2], &adc_fd[3],
	#endif
	};
	int* addr_array[] = {&mcu_addr,
	#ifdef ALLOW_MCU_SEC
		&mcu_addr_sec,
	#endif
	#ifdef ALLOW_EXT_ADC
		&adc_addr[0], &adc_addr[1], &adc_addr[2], &adc_addr[3],
	#endif
	};
	for (int8_t i=0; i<(sizeof(fd_array)/sizeof(fd_array[0])); i++){
		if(*fd_array[i] >= 0){ //"valid" fd
			int ret = close(*fd_array[i]);
			if (ret < 0){print_stderr("failed to close I2C handle for address 0x%02X, errno:%d (%m)\n", *addr_array[i], -ret);}
			print_stdout("I2C handle for address:0x%02X closed\n", *addr_array[i]);
			*fd_array[i] = -1;
		}
	}
}

void adc_data_compute(int adc_index){ //compute adc max value, flat in/out
	if (!adc_params[adc_index].enabled){return;}

	adc_params[adc_index].res_limit = 0xFFFFFFFF >> (32 - adc_params[adc_index].res); //compute adc limit
	if(adc_params[adc_index].max > adc_params[adc_index].res_limit) {
		print_stdout("WARNING: adc%d_max (%d) over ADC resolution (%dbits:%u), limited to said resolution\n", adc_index, adc_params[adc_index].max, adc_params[adc_index].res, adc_params[adc_index].res_limit);
		adc_params[adc_index].max = adc_params[adc_index].res_limit;
	} else {print_stdout("ADC%d resolution: %dbits (%u)\n", adc_index, adc_params[adc_index].res, adc_params[adc_index].res_limit);}

	unsigned int adc_halfres = (adc_params[adc_index].res_limit / 2);
	int_constrain(&adc_params[adc_index].flat_in, 0, 35); int_constrain(&adc_params[adc_index].flat_out, 0, 35); //limit flat to 0-35
	adc_params[adc_index].flat_in_comp = adc_halfres * adc_params[adc_index].flat_in / 100; //compute inside flat
	adc_params[adc_index].flat_out_comp = (adc_halfres * (100 + adc_params[adc_index].flat_out) / 100) - adc_halfres; //compute outside flat
	if (adc_params[adc_index].flat_in_comp < 0){adc_params[adc_index].flat_in_comp = 0;}
	if (adc_params[adc_index].flat_out_comp < 0){adc_params[adc_index].flat_out_comp = 0;}
	print_stdout("ADC%d computed flats: inside:%d, outside:%d\n", adc_index, adc_params[adc_index].flat_in_comp, adc_params[adc_index].flat_out_comp);

	if(adc_params[adc_index].autocenter){adc_params[adc_index].offset = adc_params[adc_index].raw - (adc_params[adc_index].res_limit / 2); //auto center
	} else {adc_params[adc_index].offset = (((adc_params[adc_index].max - adc_params[adc_index].min) / 2) + adc_params[adc_index].min) - (adc_params[adc_index].res_limit / 2);}
	print_stdout("ADC%d computed offset:%d\n", adc_index, adc_params[adc_index].offset);

	logs_write("-ADC%d (%s): resolution:%d(%d), min:%d, max:%d, flat(inner):%d%%, flat(outer):%d%%, reversed:%d, autocenter:%d (offset:%d)\n", adc_index, (adc_fd_valid[adc_index])?"extern":"mcu", adc_params[adc_index].res, adc_params[adc_index].res_limit, adc_params[adc_index].min, adc_params[adc_index].max, adc_params[adc_index].flat_in, adc_params[adc_index].flat_out, adc_params[adc_index].reversed?1:0, adc_params[adc_index].autocenter?1:0, adc_params[adc_index].offset);
}

static int adc_defuzz(int value, int old_val, int fuzz){ //apply fuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56
	if (fuzz) {
		if (value > old_val - fuzz / 2 && value < old_val + fuzz / 2){return old_val;}
		if (value > old_val - fuzz && value < old_val + fuzz){return (old_val * 3 + value) / 4;}
		if (value > old_val - fuzz * 2 && value < old_val + fuzz * 2){return (old_val + value) / 2;}
	}
	return value;
}

static int adc_correct_offset_center(int adc_resolution, int adc_value, int adc_min, int adc_max, int adc_offset, int flat_in, int flat_out){ //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent)
	int max = adc_resolution / 2, offset = max + adc_offset;
	adc_value -= offset; //center adc value to 0

	int dir = 1, limit;
	if (adc_value < 0){dir = -1; adc_value *= -1; limit = (adc_min - offset) * -1; //convert adc value to positive
	} else {limit = adc_max - offset;}
    if(limit==0){limit=1;} //avoid div by 0

	adc_value = adc_value * (max + flat_out) / limit; //convert to 0->adc_min/adc_max to 0->adc_resolution+(outside flat)
	if (flat_in > 0){adc_value = (adc_value - flat_in) * (max + flat_out) / ((max + flat_out) - flat_in);} //convert to (-inside flat)->adc_resolution+(outside flat) to 0->adc_resolution+(outside flat)

	if(adc_value < 0){adc_value = 0;} adc_value *= dir; adc_value += max; //reapply proper direction and recenter value to adc_resolution/2
	if(adc_value < 0){return 0;} else if (adc_value > adc_resolution){return adc_resolution;} //limit return from 0 to adc_resolution
	return adc_value;
}

#ifdef ALLOW_EXT_ADC
	static int MCP3021_read(int fd){ //placeholder for external adc, MCP3021 ADC for test purpose
		int ret = i2c_smbus_read_word_data(fd, 0);
		if (ret < 0){
			i2c_allerrors_count++;
			print_stderr("read failed, errno:%d (%m)\n", -ret);
			return 0;
		}
		return (((uint16_t)(ret) & (uint16_t)0x00ffU) << 8) | (((uint16_t)(ret) & (uint16_t)0xff00U) >> 8); //return swapped, uapi/linux/swab.h
	}
#endif

void i2c_poll_joystick(bool force_update){ //poll data from i2c device
	poll_clock_start = get_time_double();

	int read_limit = input_registers_count, ret = 0;
	bool update_adc = false; //limit adc pull rate
	
	if (force_update || i2c_adc_poll_loop == i2c_adc_poll || i2c_poll_rate_disable){update_adc = true; i2c_adc_poll_loop = 0;}
	if (update_adc){
#ifdef ALLOW_EXT_ADC
		if ((adc_params[2].enabled && !adc_fd_valid[2]) || (adc_params[3].enabled && !adc_fd_valid[3])){read_limit += 6; //needs to read all adc resisters
		} else if ((adc_params[0].enabled && !adc_fd_valid[0]) || (adc_params[1].enabled && !adc_fd_valid[1])){read_limit += 3;} //needs to read adc0-1 adc resisters
#else
		if (adc_params[2].enabled || adc_params[3].enabled){read_limit += 6; //needs to read all adc resisters
		} else if (adc_params[0].enabled || adc_params[1].enabled){read_limit += 3;} //needs to read adc0-1 adc resisters
#endif
	}

	if (!i2c_disabled){ret = i2c_smbus_read_i2c_block_data(mcu_fd, 0, read_limit, (uint8_t *)&i2c_joystick_registers);
	} else { //uhid stress mode
		uint8_t tmp; update_adc = true;
		if (poll_stress_loop > 0){tmp=0xFF; poll_stress_loop=0;} else {tmp=0; poll_stress_loop++;} //toogle all data between 0 and 255
		for (int i=0; i<=input_registers_count+6; i++){((uint8_t *)&i2c_joystick_registers)[i] = tmp;} //write data to registers struct
	}

	if (ret < 0){
		i2c_errors_count++; i2c_allerrors_count++;
		if (ret == -6){print_stderr("FATAL: i2c_smbus_read_i2c_block_data() failed, errno:%d (%m)\n", -ret); kill_requested = true;}
		if (i2c_errors_count >= i2c_errors_report){print_stderr("WARNING: I2C requests failed %d times in a row\n", i2c_errors_count); i2c_errors_count = 0;}
		i2c_last_error = ret; return;
	}

	if (i2c_errors_count > 0){print_stderr("last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error)); i2c_last_error = i2c_errors_count = 0;} //report last i2c error if high error count

	uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0; //merge to word to ease work

	//dpad
	const uint8_t dpad_lookup[16] = {0/*0:none*/, 1/*1:up*/, 5/*2:down*/, 1/*3:up+down*/, 7/*4:left*/, 2/*5:up+left*/, 6/*6:down+left*/, 1/*7:up+down+left*/, 3/*8:right*/, 2/*9:up+right*/, 4/*10:down+right*/, 1/*11:up+down+right*/, 7/*12:left+right*/, 1/*13:up+left+right*/, 5/*14:down+left+right*/, 0/*15:up+down+left+right*/};
	gamepad_report.hat0 = dpad_lookup[(uint8_t)(~(inputs >> mcu_input_dpad_start_index) & 0x0F)];

	//digital: reorder raw input to ev mapping
	uint16_t input_report_digital = 0xFFFF; uint8_t input_report_digital_misc = 0xFF;
	for (int i=0; i<mcu_input_map_size; i++){
		int16_t curr_input = mcu_input_map[i];
		if (curr_input != -127 && ~(inputs >> i) & 0b1){
			if (curr_input >= BTN_MISC && curr_input < BTN_9 + 1){input_report_digital_misc &= ~(1U << (abs(curr_input - BTN_MISC))); //misc
			} else if (curr_input >= BTN_GAMEPAD && curr_input < BTN_THUMBR + 1){input_report_digital &= ~(1U << (abs(curr_input - BTN_GAMEPAD)));} //gamepad
		}
	}
	gamepad_report.buttons7to0 = ~(input_report_digital & 0xFF);
	gamepad_report.buttons15to8 = ~(input_report_digital >> 8);
	gamepad_report.buttonsmisc = ~input_report_digital_misc;

	//analog
	if (update_adc){
		for (int i=0; i<4; i++){ //adc loop
			if (adc_params[i].enabled){
#ifdef ALLOW_EXT_ADC
				if (!adc_fd_valid[i]){ //read mcu adc value
#endif
					uint8_t *tmpPtr = (uint8_t*)&i2c_joystick_registers; //pointer to i2c store
					uint8_t tmpPtrShift = i, tmpMask = 0xF0, tmpShift = 0; //pointer offset, lsb mask, lsb bitshift
					if (i<2){tmpPtr += mcu_i2c_register_adc0;} else {tmpPtr += mcu_i2c_register_adc2; tmpPtrShift -= 2;} //update pointer
					if (tmpPtrShift == 0){tmpMask = 0x0F; tmpShift = 4;} //adc0-2 lsb
					adc_params[i].raw = ((*(tmpPtr + tmpPtrShift) << 8) | (*(tmpPtr + 2) & tmpMask) << tmpShift) >> (16 - adc_params[i].res); //char to word
#ifdef ALLOW_EXT_ADC
				} else { //external adc value
					adc_params[i].raw = MCP3021_read(adc_fd[i]); //placeholder external adc read function
				}
#endif

				if (adc_firstrun){adc_data_compute(i); adc_params[i].raw_prev = adc_params[i].raw;} //compute adc max value, flat in/out, offset

				if(adc_params[i].raw < adc_params[i].raw_min){adc_params[i].raw_min = adc_params[i].raw;} //update min value
				if(adc_params[i].raw > adc_params[i].raw_max){adc_params[i].raw_max = adc_params[i].raw;} //update max value

				adc_params[i].value = adc_defuzz(adc_params[i].raw, adc_params[i].raw_prev, adc_params[i].fuzz); adc_params[i].raw_prev = adc_params[i].raw; //defuzz
				adc_params[i].value = adc_correct_offset_center(adc_params[i].res_limit, adc_params[i].value, adc_params[i].min, adc_params[i].max, adc_params[i].offset, adc_params[i].flat_in_comp, adc_params[i].flat_out_comp); //re-center adc value, apply flats and extend to adc range
				if(adc_params[i].reversed){adc_params[i].value = abs(adc_params[i].res_limit - adc_params[i].value);} //reverse value

				adc_params[i].value <<= 16 - adc_params[i].res; //convert to 16bits value for report
				if (adc_params[i].value < 1){adc_params[i].value = 1;} else if (adc_params[i].value > 0xFFFF-1){adc_params[i].value = 0xFFFF-1;} //Reicast overflow fix

				if(adc_map[i] > -1){*js_values[adc_map[i]] = (uint16_t)adc_params[i].value;} //adc mapped to -1 should be disabled during runtime if not in diagnostic mode
			}
		}
		adc_firstrun = false;
	}
	i2c_adc_poll_loop++;

	//report
	#ifndef DIAG_PROGRAM
		int report_val = 0, report_prev_val = 1;
		if (!update_adc){
			report_val = gamepad_report.buttons7to0 + gamepad_report.buttons15to8 + gamepad_report.buttonsmisc + gamepad_report.hat0;
			report_prev_val = gamepad_report_prev.buttons7to0 + gamepad_report_prev.buttons15to8 + gamepad_report.buttonsmisc + gamepad_report_prev.hat0;
		}
	#endif

	i2c_poll_duration = get_time_double() - poll_clock_start;
	#ifndef DIAG_PROGRAM
		if (report_val != report_prev_val){gamepad_report_prev = gamepad_report; uhid_send_event(uhid_fd);} //uhid update
	#endif
}

//MCU related functions
int mcu_check_manufacturer(){ //check device manufacturer, fill signature,id,version, return 0 on success, 1 if version missmatch , -1 on wrong manufacturer, -2 on i2c error
	int tmp_reg = offsetof(struct i2c_joystick_register_struct, manuf_ID) / sizeof(uint8_t); //check signature
	int ret = i2c_smbus_read_byte_data(mcu_fd, tmp_reg);
	if (ret != mcu_manuf){
		if (ret < 0){i2c_allerrors_count++; print_stderr("FATAL: reading I2C device manuf_ID (register:0x%02X) failed, errno:%d (%m)\n", tmp_reg, -ret);
		} else {print_stderr("FATAL: invalid I2C device signature: 0x%02X\n", ret);}
		return -1;
	}
	mcu_signature = (uint8_t)ret;

	tmp_reg = offsetof(struct i2c_joystick_register_struct, device_ID) / sizeof(uint8_t); //check version
	ret = i2c_smbus_read_word_data(mcu_fd, tmp_reg);
	if (ret < 0){i2c_allerrors_count++; print_stderr("FATAL: reading I2C device device_ID (register:0x%02X) failed, errno:%d (%m)\n", tmp_reg, -ret); return -2;}
	mcu_id = ret & 0xFF;
	mcu_version = (ret >> 8) & 0xFF;
	print_stdout("I2C device detected, signature:0x%02X, id:%d, version:%d\n", mcu_signature, mcu_id, mcu_version);
	logs_write("I2C device: signature:0x%02X, id:%d, version:%d\n\n", mcu_signature, mcu_id, mcu_version);

	if (mcu_version != mcu_version_even){
		print_stdout("WARNING: program register version (%d) mismatch MCU version (%d)\n", mcu_version_even, mcu_version);
		logs_write("WARNING: program register version (%d) mismatch MCU version (%d)\n", mcu_version_even, mcu_version);
		return 1;
	}

	return 0;
}

int mcu_update_config0(){ //read/update config0 register, return 0 on success, -1 on error
    int ret = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_config0);
    if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("FATAL: reading MCU config0 (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_config0, -ret);
		return -1;
	}
    mcu_config0.bits = (uint8_t)ret;
	mcu_config0.vals.debounce_level = digital_debounce;
	ret = i2c_smbus_write_byte_data(mcu_fd, mcu_i2c_register_config0, mcu_config0.bits); //update i2c config
    if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("FATAL: writing MCU config0 (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_config0, -ret);
		return -1;
	}

	return 0;
}

int mcu_update_register(int* fd, uint8_t reg, uint8_t value, bool check){ //update and check register, return 0 on success, -1 on error
	if(*fd < 0){return -1;}

	int ret = i2c_smbus_write_byte_data(*fd, reg, value);
	if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("writing to register 0x%02X failed, errno:%d (%m)\n", reg, -ret);
		return -1;
	}

	if (check){
		ret = i2c_smbus_read_byte_data(*fd, reg);
		if (ret < 0){
			i2c_allerrors_count++;
			print_stderr("reading register 0x%02X failed, errno:%d (%m)\n", reg, -ret);
			return -1;
		}

		if (ret != value){print_stderr("register 0x%02X update failed, expected:0x%02X but got 0x%02X\n", reg, value, ret); return -1;}
	}

	return 0;
}

int init_adc(){ //init adc data, return 0 on success, -1 on resolution read fail, -2 on adc conf
	adc_firstrun = true; //force initial adc update

	//mcu adc resolution
	int tmp_adc_res = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_adc_res);
	if (tmp_adc_res < 0){print_stderr("FATAL: reading MCU ADC resolution (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_adc_res, -tmp_adc_res); i2c_allerrors_count++; return -1;
	} else if (tmp_adc_res == 0){print_stdout("WARNING: MCU ADC is currently disabled, please refer to documentation for more informations\n");
	} else {print_stdout("MCU ADC resolution: %dbits\n", tmp_adc_res);}

	//mcu analog config
	int ret = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_adc_conf);
	if (ret < 0){print_stderr("FATAL: reading MCU ADC configuration (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_adc_conf, -ret); i2c_allerrors_count++; return -2;}

	uint8_t mcu_adc_config_old = (uint8_t)ret;
	uint8_t mcu_adc_config_new = mcu_adc_config_old & 0xF0; //copy enable bits

	for (int i=0; i<4; i++){ //mcu adc loop
		int_constrain(&adc_map[i], -1, 3); //correct invalid map
		bool mcu_adc_used = false;
		mcu_adc_enabled[i] = (mcu_adc_config_old >> (i+4)) & 0b1; //mcu adc enabled
		if (adc_map[i] > -1 || diag_mode){
			if (!adc_fd_valid[i]){ //external adc not used
#ifdef ALLOW_EXT_ADC
				adc_init_err[i] = -1;
#endif
				adc_params[i].res = tmp_adc_res; //mcu adc resolution
				if (!mcu_adc_enabled[i]){adc_params[i].enabled = false; //mcu adc fully disable
				} else if (adc_params[i].enabled){mcu_adc_config_new |= 1U << i; mcu_adc_used = true;} //mcu adc used
			}
#ifdef ALLOW_EXT_ADC
			else { //TODO EXTERNAL
				if (1 != 2){adc_init_err[i] = -3; //init external adc failed
				} else if (!adc_params[i].enabled){adc_init_err[i] = -1; //not enabled
				} else {adc_init_err[i] = 0;} //ok
			}
#endif
		} else {adc_params[i].enabled = false;} //disable adc because map is invalid
		*js_values[i] = (uint16_t)0x7FFF; //reset uhid axis value, mainly used in diag part

		//report
		if(!diag_mode_init){
			print_stdout("ADC%d: %s", i, adc_params[i].enabled?"enabled":"disabled");
			fputs(", ", stdout);
			if (mcu_adc_used){fputs("MCU", stdout);
#ifdef ALLOW_EXT_ADC
			} else if (adc_fd_valid[i]){fprintf(stdout, "External(0x%02X)", adc_addr[i]);
#endif
			} else {fputs("None", stdout);}
			fprintf(stdout, ", mapped to %s(%d)\n", js_axis_names[adc_map[i]+1], adc_map[i]);
		}
	}
	
	if (mcu_adc_config_old != mcu_adc_config_new){ //mcu adc config needs update
		ret = i2c_smbus_write_byte_data(mcu_fd, mcu_i2c_register_adc_conf, mcu_adc_config_new);
		if (ret < 0){print_stderr("failed to set new MCU ADC configuration (0x%02X), errno:%d (%m)\n", mcu_i2c_register_adc_conf, -ret); i2c_allerrors_count++; return -2;
		} else { //read back wrote value by safety
			ret = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_adc_conf);
			if (ret != mcu_adc_config_new){print_stderr("failed to update MCU ADC configuration, should be 0x%02X but is 0x%02X\n", mcu_adc_config_new, ret); return -2;
			} else {print_stdout("MCU ADC configuration updated, is now 0x%02X, was 0x%02X\n", ret, mcu_adc_config_old);}
		}
	}

	return 0;
}

//IRQ related functions
#ifdef USE_PIGPIO_IRQ
static void gpio_callback(int gpio, int level, uint32_t tick){ //seems to work
	switch (level) {
		case 0:
			if(debug) print_stdout("DEBUG: GPIO%d low\n", gpio);
			i2c_poll_joystick(false);
			break;
		case 1:
			if(debug) print_stdout("DEBUG: GPIO%d high\n", gpio);
			break;
		case 2:
			if(debug) print_stdout("DEBUG: GPIO%d WATCHDOG\n", gpio);
			break;
	}
}
#elif defined(USE_WIRINGPI_IRQ)
static void mcu_irq_handler(void){
	if (debug){print_stdout("DEBUG: GPIO%d triggered\n", irq_gpio);}
    i2c_poll_joystick(false);
}
#endif

//TTY related functions
static void tty_signal_handler(int sig){ //handle signal func
	if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
    if (term_backup.c_cflag){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
	kill_requested = true;
}

//SHM related functions
#ifndef DIAG_PROGRAM
#ifdef USE_SHM_REGISTERS
static int file_read(char* path, char* bufferptr, int buffersize){ //read file
	int ret = 0;
	FILE *filehandle = fopen(path, "r");
	if (filehandle != NULL) {
		fgets(bufferptr, buffersize, filehandle);
		if (ferror(filehandle)){print_stderr("failed to read '%s'\n", path); ret = -EFAULT;}
		fclose(filehandle);
	} else {print_stderr("failed to read '%s'\n", path); return -EFAULT;}
	return ret;
};
#endif

static int file_write(char* path, char* content){ //write file
	FILE *filehandle = fopen(path, "w");
	if (filehandle != NULL) {
		fprintf(filehandle, "%s", content);
		fclose(filehandle);
	} else {print_stderr("failed to write '%s'\n", path); return -EFAULT;}
	return 0;
}
#endif

static int folder_create(char* path, int rights, int uid, int gid) { //create folder(s), set rights/uid/gui if not -1. Return number of folder created, -errno on error
	int ret, depth = 0; //security
	struct stat file_stat = {0};
	char curr_path[strlen(path)+1], sub_path[strlen(path)+2]; sub_path[0]='\0';

	strcpy(curr_path, path);
	if(curr_path[strlen(curr_path)-1] == '/'){curr_path[strlen(curr_path)-1] = '\0';}

	char *tmpPtr = strtok (curr_path, "/"); //split path
	while (tmpPtr != NULL){
		strcat(sub_path, "/"); strcat(sub_path, tmpPtr);

		if (stat(sub_path, &file_stat) == -1){
			ret = mkdir(sub_path, (rights == -1)?0644:rights);
			if (ret < 0){print_stderr("failed to create directory '%s', errno:%d (%m)\n", sub_path, -ret); return ret;}
			print_stdout("directory '%s' created\n", sub_path);
			if (uid != -1 || gid != -1){
				if (chown(sub_path, uid, gid) < 0 && debug){print_stderr("failed to set directory '%s' owner, err: %m\n", sub_path);}
			}
		} else if (debug){print_stdout("directory '%s' already exist\n", sub_path);}

		depth++; if(depth > 10){print_stderr("something gone very wrong, depth:10 hit, break\n"); break;}
		tmpPtr = strtok (NULL, "/"); //next sub folder
	}

	return depth;
}

static int logs_write(const char* format, ...){ //write to log, return chars written or -1 on error, based on printf source code
	int ret = 0;
	if (logs_fh != NULL){
		va_list args; va_start(args, format);
		ret = vfprintf(logs_fh, format, args);
		va_end(args);
	} else {ret = -1;}
	return ret;
}

static void shm_init(bool first){ //init shm related things, folders and files creation
	char curr_path[strlen(shm_path)+256]; //current path with additionnal 255 chars for filename
	if (first){ //initial run, skip i2c part
		//if(shm_path[strlen(shm_path)-1] == '/'){shm_path[strlen(shm_path)-1] = '\0';}
		if (folder_create(shm_path, 0666, user_uid, user_gid) < 0){return;} //recursive folder create

		//log file
		sprintf(curr_path, "%s/driver.log", shm_path);
		logs_fh = fopen(curr_path, "w+");
		if (logs_fh != NULL) {print_stdout("logs: %s\n", curr_path);
		} else {print_stderr("failed to open '%s' (%m)\n", curr_path);}

		shm_enable=true;
	} else if (shm_enable) {
		#ifndef DIAG_PROGRAM
			//status file
			sprintf(curr_path, "%s/status", shm_path);
			if (file_write (curr_path, "1") == 0){print_stdout("'%s' content set to 1\n", curr_path);
			}else{return;}
		#endif

		//registers to files "bridge"
		#ifdef USE_SHM_REGISTERS
			char buffer[128]; int ret;
			for (unsigned int i = 0; i < shm_vars_arr_size; i++) {
				ret = i2c_smbus_read_byte_data(*(shm_vars[i].i2c_fd), shm_vars[i].i2c_register);
				if (ret < 0){i2c_allerrors_count++; print_stderr("failed to read register 0x%02X, errno:%d (%m)\n", shm_vars[i].i2c_register, -ret); return;
				} else {
					*(shm_vars[i].ptr) = (uint8_t) ret; //set register backup value
					sprintf(shm_vars[i].path, "%s/%s", shm_path, shm_vars[i].file); //set path
					sprintf(buffer, "%d", ret);
					if (file_write (shm_vars[i].path, buffer) == 0){
						chown(shm_vars[i].path, (uid_t) user_uid, (gid_t) user_gid);
						print_stdout("'%s' content set to '%s'\n", shm_vars[i].path, buffer);
					}else{return;}
				}
			}
		#endif
	}
}

#ifndef DIAG_PROGRAM
static void shm_update(void){ //update registers/files linked to shm things
	#ifdef USE_SHM_REGISTERS
		if (shm_enable){
			int ret, ret_read;
			char buffer[128];

			for (unsigned int i = 0; i < shm_vars_arr_size; i++) {
				ret_read = file_read (shm_vars[i].path, buffer, 127);
				if (ret_read >= 0){ret_read = atoi(buffer);}

				ret = i2c_smbus_read_byte_data(*(shm_vars[i].i2c_fd), shm_vars[i].i2c_register);
				if (ret < 0){i2c_allerrors_count++; print_stderr("failed to read register 0x%02X, errno:%d (%m)\n", shm_vars[i].i2c_register, -ret); return;
				} else {
					sprintf(buffer, "%d", ret);
					if (*(shm_vars[i].ptr) != (uint8_t)ret || ret_read < 0){ //register value have priority over file modification
						if (*(shm_vars[i].ptr) != (uint8_t)ret){
							if (debug){print_stdout("register 0x%02X changed, stored:'%d', i2c:'%d'\n", shm_vars[i].i2c_register, *(shm_vars[i].ptr), ret);}
							*(shm_vars[i].ptr) = (uint8_t)ret; //backup new register value
						}
						if (file_write (shm_vars[i].path, buffer) == 0){
							chown(shm_vars[i].path, (uid_t) user_uid, (gid_t) user_gid);
							if (debug){print_stdout("'%s' content set to '%s' because %s\n", shm_vars[i].path, buffer, (ret_read<0)?"file was missing":"I2C value changed");}
						}
					} else if (!shm_vars[i].rw && ret_read != ret){
						if (file_write (shm_vars[i].path, buffer) == 0){
							chown(shm_vars[i].path, (uid_t) user_uid, (gid_t) user_gid);
							if (debug){print_stdout("'%s' content restore to '%s' because register 0x%02X is read only\n", shm_vars[i].path, buffer, shm_vars[i].i2c_register);}
						}
					} else if (shm_vars[i].rw && *(shm_vars[i].ptr) != (uint8_t)ret_read){ //file modification
						if (debug){print_stdout("'%s' changed, file:'%d', stored:'%d'\n", shm_vars[i].path, ret_read, *(shm_vars[i].ptr));}
						*(shm_vars[i].ptr) = (uint8_t)ret_read; //backup new register value
						ret = i2c_smbus_write_byte_data(*(shm_vars[i].i2c_fd), shm_vars[i].i2c_register, (uint8_t)ret_read);
						if (ret < 0){i2c_allerrors_count++; print_stderr("failed to update register 0x%02X, errno:%d (%m)\n", shm_vars[i].i2c_register, -ret);
						} else {print_stdout("register 0x%02X updated with value:%d\n", shm_vars[i].i2c_register, ret_read);}
					}
				}
			}
		}
	#endif
}
#endif

static void shm_close(void){ //close shm related things
	#ifndef DIAG_PROGRAM
		if (shm_enable){
			char curr_path[strlen(shm_path)+256]; //current path with additionnal 255 chars for filename
			sprintf(curr_path, "%s/status", shm_path); //current path
			if (file_write (curr_path, "0") == 0){print_stdout("'%s' content set to 0\n", curr_path);}
		}
	#endif
	if (logs_fh != NULL){fclose(logs_fh);}
}

//integer manipulation functs
void int_rollover(int* val, int min, int max){ //rollover int value between (incl) min and max, work both way
    if(*val < min){*val = max;} else if(*val > max){*val = min;}
}

void int_constrain(int* val, int min, int max){ //limit int value to given (incl) min and max value
    if(*val < min){*val = min;} else if(*val > max){*val = max;}
}

//Generic functions
static void program_close(void){ //regroup all close functs
	if (already_killed){return;}
    if (term_backup.c_cflag){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
	#ifndef DIAG_PROGRAM
		uhid_destroy(uhid_fd);
	#endif
	i2c_close_all();
	shm_close();
	already_killed = true;
}

void program_get_path(char** args, char* var){ //get current program path based on program argv or getcwd if failed
	char tmp_path[PATH_MAX], tmp_subpath[PATH_MAX];
	struct stat file_stat = {0};
	strcpy(tmp_path, args[0]); if (args[0][0]=='.'){strcpy(var, ".\0");}
	char *tmpPtr = strtok(tmp_path, "/");
	while (tmpPtr != NULL) {
		sprintf(tmp_subpath, "%s/%s", var, tmpPtr);
		if (stat(tmp_subpath, &file_stat) == 0 && S_ISDIR(file_stat.st_mode) != 0){strcpy(var, tmp_subpath);}
		tmpPtr = strtok(NULL, "/");
	}
	if (strcmp(var, "./.") == 0){getcwd(var, PATH_MAX);}
	if (debug){print_stdout("program path:'%s'\n", var);}
}


//main functions, obviously, no? lool
static void program_usage (char* program){
	fprintf(stdout, "Version: %s\n", programversion);
	fprintf(stdout, "Dev: %s\n\n", dev_webpage);
	fprintf(stdout, "Since it needs to access/write to system device, program needs to run as root.\n\n");

	#if defined(ALLOW_MCU_SEC) || defined(USE_SHM_REGISTERS) || defined(ALLOW_EXT_ADC) || defined(USE_PIGPIO_IRQ) || defined(USE_WIRINGPI_IRQ)
		fprintf(stdout, "Enabled feature(s) (at compilation time):\n"
		#ifdef ALLOW_MCU_SEC
			"\t-MCU secondary features, on-the-fly I2C address update, backlight control, ...\n"
		#endif
		#ifdef USE_SHM_REGISTERS
			"\t-SHM to MCU bridge, allow to direct update some registers using file system.\n"
		#endif
		#ifdef ALLOW_EXT_ADC
			"\t-External ADCs, TO BE IMPLEMENTED FEATURE, placeholder functions for now.\n"
		#endif
		#ifdef USE_PIGPIO_IRQ
			"\t-PIGPIO IRQ.\n"
		#endif
		#ifdef USE_WIRINGPI_IRQ
			"\t-WiringPi IRQ.\n"
		#endif
			"\n"
		);
	#endif

	#ifndef DIAG_PROGRAM
		fprintf(stdout, "Example : %s -configset debug=1\n", program);
		fprintf(stdout, "Arguments:\n"
		"\t-h or -help: show arguments list.\n"
		"\t-configreset: reset configuration file to default (*).\n"
		"\t-configset: set custom configuration variable with specific variable, format: 'VAR=VALUE' (e.g. debug=1) (*).\n"
		"\t-configlist: list all configuration variables (*).\n"
		"\t-noi2c: disable IRQ, I2C polls and pollrate, generate garbage data for UHID, mainly used for benchmark. (mostly crash EV monitoring softwares).\n"
		"\t-nouhid: disable IRQ, UHID reports and pollrate, mainly used for benchmark. (mostly crash EV monitoring softwares).\n"
		"(*): close program after function executed (incl failed).\n"
		);
	#else
		fprintf(stdout, "Setup/diagnostic program doesn't implement any arguments\n", program);
	#endif
}

int main(int argc, char** argv){
	program_start_time = get_time_double(); //program start time, used for detailed outputs
	if (getuid() != 0 || (argc > 1 && (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0))){program_usage(argv[0]); return 0;} //help argument requested or not running as root
	//if (getuid() != 0) {print_stderr("FATAL: this program needs to run as root, current user:%d\n", getuid()); return EXIT_FAILURE;} //not running as root

	int main_return = EXIT_SUCCESS; //rpogram retunr

	program_get_path(argv, program_path); //get current program path
	sprintf(config_path, "%s/%s", program_path, cfg_filename); //convert config relative to full path
	print_stdout("Config file: %s\n", config_path);

	//program arguments parse
	for(int i=1; i<argc; ++i){
		#ifndef DIAG_PROGRAM
			if (strcmp(argv[i],"-configreset") == 0){return config_save(cfg_vars, cfg_vars_arr_size, cfg_filename, user_uid, user_gid, true); //reset config file
			} else if (strcmp(argv[i],"-configset") == 0){ //set custom config var
				if (++i<argc){return config_set(cfg_vars, cfg_vars_arr_size, cfg_filename, user_uid, user_gid, true, argv[i]);
				} else {
					print_stderr("FATAL: -configset defined with invalid argument, format: -configset VAR=VALUE\n");
					print_stderr("Run program with -h or -help for usage\n");
					return -EPERM;
				}
			} else if (strcmp(argv[i],"-configlist") == 0){config_list(cfg_vars, cfg_vars_arr_size); return 0; //list config vars
			} else if (strcmp(argv[i],"-noi2c") == 0){i2c_disabled = true; //disable i2c, pollrate, garbage data to uhid for benchmark
			} else if (strcmp(argv[i],"-nouhid") == 0){uhid_disabled = true; //disable irq, uhid for benchmark
			}
		#endif
	}
	
	config_parse(cfg_vars, cfg_vars_arr_size, cfg_filename, user_uid, user_gid); //parse config file, create if needed
	shm_init(true); //init shm path and files

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle
	signal(SIGABRT, tty_signal_handler); //failure

	atexit(program_close); at_quick_exit(program_close); //run on program exit
	
	//open i2c devices
	if (!i2c_disabled){
		if (i2c_check_bus(i2c_bus) != 0){return EXIT_FAILURE;}

		if (i2c_open_dev(&mcu_fd, i2c_bus, mcu_addr) != 0){return EXIT_FAILURE;} //main
		#ifdef ALLOW_MCU_SEC
			if (i2c_open_dev(&mcu_fd_sec, i2c_bus, mcu_addr_sec) != 0){/*return EXIT_FAILURE;*/} //secondary
		#endif
		print_stdout("MCU detected registers: adc_conf_bits:0x%02X, adc_res:0x%02X, config0:0x%02X\n", mcu_i2c_register_adc_conf, mcu_i2c_register_adc_res, mcu_i2c_register_config0);

		if (mcu_check_manufacturer() < 0){return EXIT_FAILURE;} //invalid manufacturer
		if (mcu_update_config0() != 0){return EXIT_FAILURE;} //read/update of config0 register failed

		#ifdef ALLOW_EXT_ADC
			for (int i=0; i<4; i++){ //external adcs device loop
				if (adc_params[i].enabled || diag_mode){adc_fd_valid[i] = (i2c_open_dev(&adc_fd[i], i2c_bus, adc_addr[i]) == 0);}
			}
		#endif

		if (init_adc() < 0){return EXIT_FAILURE;} //init adc data failed
	}

	//advanced features if mcu secondary address valid
	#ifdef ALLOW_MCU_SEC
		//if (mcu_fd_sec >= 0){;}
	#endif

	#ifndef DIAG_PROGRAM
		if (i2c_disabled){ //i2c disable, "enable" all adcs for fake report
			for (int i=0; i<4; i++){
				adc_params[i].enabled = true;
				adc_params[i].res = 10; adc_params[i].res_limit = 1023;
				adc_params[i].flat_in = adc_params[i].flat_out = 10;
				adc_map[i] = i;
			}
		}

		if (i2c_disabled || uhid_disabled){
			i2c_poll_rate_disable = true; irq_enable = false;
			print_stdout("running in a specific mode (no-i2c:%d, no-uhid:%d), pollrate disabled\n", i2c_disabled, uhid_disabled);
			if (irq_gpio >= 0){print_stderr("IRQ disabled\n");}
		}

		shm_init(false); //init shm i2c things
		
		//uhdi defs
		if (!uhid_disabled){ //don't create uhid device is -nouhid set
			uhid_fd = open(uhid_device_path, O_RDWR | O_CLOEXEC);
			if (uhid_fd < 0){print_stderr("failed to open uhid-cdev %s, errno:%d (%m)\n", uhid_device_path, -uhid_fd); return EXIT_FAILURE;}
			print_stdout("uhid-cdev %s opened\n", uhid_device_path);

			int ret = uhid_create(uhid_fd);
			if (ret < 0){return EXIT_FAILURE;}
			print_stdout("uhid device created\n");
		}
	#else
		i2c_poll_rate_disable = true;
	#endif

	//initial poll
	if (i2c_poll_rate < 1){i2c_poll_rate_disable = true; //disable pollrate limit
	} else {i2c_poll_rate_time = 1. / i2c_poll_rate;} //poll interval in sec
	if (i2c_adc_poll < 1){i2c_adc_poll = 1;}
	i2c_adc_poll_loop = i2c_adc_poll;
	i2c_poll_joystick(true);

	//interrupt
	if (irq_enable && (adc_params[0].enabled || adc_params[1].enabled || adc_params[2].enabled || adc_params[3].enabled)){print_stdout("IRQ disabled because ADC enabled\n"); irq_enable = false;}

	if (irq_gpio >= 0 && irq_enable){ //TODO FIX generate a glitch on very first input report making joystick report 0 instead of 0x7FFF
		irq_enable = false;
	#ifdef USE_WIRINGPI_IRQ
		#define WIRINGPI_CODES 1 //allow error code return
		int err;
		if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
			print_stderr("failed to initialize wiringPi, errno:%d\n", -err);
		} else {
			if ((err = wiringPiISR(irq_gpio, INT_EDGE_FALLING, &mcu_irq_handler)) < 0){
				print_stderr("wiringPi failed to set callback for GPIO%d\n", irq_gpio);
			} else {
				print_stdout("using wiringPi IRQ on GPIO%d\n", irq_gpio);
				irq_enable = true;
			}
		}
	#elif defined(USE_PIGPIO_IRQ)
		int ver, err; bool irq_failed=false;
		if ((ver = gpioInitialise()) > 0){
			print_stdout("pigpio: version: %d\n",ver);
		} else {
			print_stderr("failed to detect pigpio version\n"); irq_failed = true;
		}

		if (!irq_failed && irq_gpio > 31){
			print_stderr("pigpio limited to GPIO0-31, asked for %d\n", irq_gpio); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetMode(irq_gpio, PI_INPUT)) != 0){ //set as input
			print_stderr("pigpio failed to set GPIO%d to input\n", irq_gpio); irq_failed = true;
		}
		
		if (!irq_failed && (err = gpioSetPullUpDown(irq_gpio, PI_PUD_UP)) != 0){ //set pull up
			print_stderr("pigpio failed to set PullUp for GPIO%d\n", irq_gpio); irq_failed = true;
		}

		if (!irq_failed && (err = gpioGlitchFilter(irq_gpio, 100)) != 0){ //glitch filter to avoid bounce
			print_stderr("pigpio failed to set glitch filter for GPIO%d\n", irq_gpio); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetAlertFunc(irq_gpio, gpio_callback)) != 0){ //callback setup
			print_stderr("pigpio failed to set callback for GPIO%d\n", irq_gpio); irq_failed = true;
		} else {
			print_stdout("using pigpio IRQ on GPIO%d\n", irq_gpio);
			gpioSetSignalFunc(SIGINT, tty_signal_handler); //ctrl-c
			gpioSetSignalFunc(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle
			gpioSetSignalFunc(SIGABRT, tty_signal_handler); //failure
			irq_enable = true;
		}
	#else
		irq_enable = false;
	#endif
	} else {irq_enable = false;}
	
    fprintf(stderr, "Press '^C' to quit...\n");
	
	#ifndef DIAG_PROGRAM
		if (irq_enable){
			while (!kill_requested){ //sleep until app close requested
				i2c_poll_joystick(false); //poll just in case irq missed one shot
				shm_update(); //shm update
				usleep((useconds_t)(shm_update_interval * 1000000));
			}
		} else {
			if (!i2c_poll_rate_disable){print_stdout("pollrate: digital:%dhz, adc:%dhz\n", i2c_poll_rate, i2c_poll_rate / i2c_adc_poll);} else {print_stdout("poll speed not limited\n");}
			while (!kill_requested){
				i2c_poll_joystick(false);

				if (shm_clock_start < -0.1) {shm_clock_start = poll_clock_start;} //used for shm interval
				if (debug_adv && poll_benchmark_clock_start < -0.1) {poll_benchmark_clock_start = poll_clock_start;} //benchmark restart
				if (poll_clock_start - shm_clock_start > shm_update_interval){shm_update(); shm_clock_start = -1.;} //shm update

				//poll rate implement
				if (kill_requested) break; 
				//if(debug) print_stdout ("DEBUG: poll_clock_start:%lf, i2c_poll_duration:%lf\n", poll_clock_start, i2c_poll_duration);

				if (!i2c_poll_rate_disable){
					if (i2c_poll_duration > i2c_poll_duration_warn){print_stderr ("WARNING: extremely long loop duration: %dms\n", (int)(i2c_poll_duration*1000));}
					if (i2c_poll_duration < 0){i2c_poll_duration = 0;} //hum, how???
					if (i2c_poll_duration < i2c_poll_rate_time){usleep((useconds_t) ((double)(i2c_poll_rate_time - i2c_poll_duration) * 1000000));} //need to sleep to match poll rate. note: doesn't implement uhid_write_duration as uhid write duration way faster than i2c transaction
				}

				if (i2c_poll_rate_disable && debug_adv){ //benchmark mode
					poll_benchmark_loop++;
					if ((poll_clock_start - poll_benchmark_clock_start) > 2.) { //report every seconds
						print_stdout("poll loops per sec (2secs samples) : %ld\n", poll_benchmark_loop / 2);
						poll_benchmark_loop = 0; poll_benchmark_clock_start = -1.;
					}
				}
			}
		}

		if (i2c_last_error != 0) {print_stderr("last detected I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));}
	#else
		program_diag_mode(); //diagnostic mode
	#endif

	if (adc_params[0].enabled || adc_params[1].enabled || adc_params[2].enabled || adc_params[3].enabled){
		print_stdout("Detected ADC limits:\n");
		logs_write("Detected ADC limits:\n");
		for (uint8_t i=0; i<4; i++){
			if (adc_params[i].raw_min != INT_MAX){
				print_stdout("ADC%d: min:%d max:%d\n", i, adc_params[i].raw_min, adc_params[i].raw_max);
				logs_write("-ADC%d: min:%d, max:%d\n", i, adc_params[i].raw_min, adc_params[i].raw_max);
			}
		}
	}

	return main_return;
}
