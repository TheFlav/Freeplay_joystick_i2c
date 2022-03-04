/*
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

#include "driver_i2c_registers.h"
#include "driver_debug_print.h"
#include "driver_config.h"
#include "driver_hid_desc.h"
#include "nns_config.h"
#include "driver.h"

#if defined(USE_PIGPIO_IRQ) && defined(USE_WIRINGPI_IRQ)
	#error Cannot do both IRQ styles
#elif defined(USE_WIRINGPI_IRQ)
	#include <wiringPi.h>
#elif defined(USE_PIGPIO_IRQ)
	#include <pigpio.h>
#endif



//Time related functions
static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return -1.; //failed
}

//UHID related functions
static int uhid_create(int fd) { //create uhid device
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

static void uhid_destroy(int fd){ //close uhid device
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

static int uhid_send_event(int fd) { //send event to uhid device
	if (uhid_disabled){return 0;}
	if (uhid_fd < 0){return -EIO;}

	struct uhid_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT2;

	int index = 0;
	ev.u.input2.data[index++] = gamepad_report.buttons7to0; //digital msb
	ev.u.input2.data[index++] = gamepad_report.buttons15to8; //digital lsb
	ev.u.input2.data[index++] = gamepad_report.hat0; //dpad
	memcpy(&ev.u.input2.data[index], &gamepad_report.left_x, 2); //x1
	memcpy(&ev.u.input2.data[index+2], &gamepad_report.left_y, 2); //y1
	memcpy(&ev.u.input2.data[index+4], &gamepad_report.right_x, 2); //x2
	memcpy(&ev.u.input2.data[index+6], &gamepad_report.right_y, 2); //y2
	index+=8;
	ev.u.input2.data[index++] = gamepad_report.buttonsmisc; //digital misc

	ev.u.input2.size = index;
	return uhid_write(fd, &ev);
}

//I2C related
static int i2c_open(int bus, int addr){ //open I2C file
	char fd_path[13];
	if (bus < 0){bus = 0;}
	if (addr < 0 || addr > 127) {print_stderr("FATAL: invalid I2C address:0x%02X (%d)\n", addr, addr); program_close(); exit(EXIT_FAILURE);}

	sprintf(fd_path, "/dev/i2c-%d", bus);
	
	int fd = open(fd_path, O_RDWR);
	if (fd < 0) {
		print_stderr("FATAL: failed to open '%s', errno:%d (%m)\n", fd_path, -fd);
		program_close(); exit(EXIT_FAILURE);
	}

	int ret = ioctl(fd, i2c_ignore_busy ? I2C_SLAVE_FORCE : I2C_SLAVE, addr);
	if (ret < 0) {
		close(fd); print_stderr("FATAL: ioctl failed for address 0x%02X, errno:%d (%m)\n", addr, -ret);
		program_close(); exit(EXIT_FAILURE);
	}

	ret = i2c_smbus_read_byte_data(fd, 0);
	if (ret < 0) {
		i2c_allerrors_count++;
		close(fd); print_stderr("FATAL: failed to read from address 0x%02X, errno:%d (%m)\n", addr, -ret);
		program_close(); exit(EXIT_FAILURE);
	}

	print_stdout("address:0x%02X opened (bus:%s)\n", addr, fd_path);
	return fd;
}

static void i2c_close(void){ //close all I2C files
	int fd_array [] = {i2c_fd, i2c_fd_sec, i2c_adc_fd[0], i2c_adc_fd[1], i2c_adc_fd[2], i2c_adc_fd[3]};
	int addr_array [] = {i2c_addr, i2c_addr_sec, i2c_addr_adc[0], i2c_addr_adc[1], i2c_addr_adc[2], i2c_addr_adc[3]};

	for (int8_t i=0; i<(sizeof(fd_array)/sizeof(fd_array[0])); i++){
		if(fd_array[i] >= 0){ //"valid" fd
			int ret = close(fd_array[i]);
			if (ret < 0){print_stderr("failed to close I2C handle for address 0x%02X, errno:%d (%m)\n", addr_array[i], -ret);}
			print_stdout("I2C handle for address:0x%02X closed\n", addr_array[i]);
			fd_array[i] = -1;
		}
	}
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

static int MCP3021_read (int fd){ //MCP3021 ADC implement for test purpose
	int ret = i2c_smbus_read_word_data(fd, 0);
	if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("read failed, errno:%d (%m)\n", -ret);
		return 0;
	}
	return (((uint16_t)(ret) & (uint16_t)0x00ffU) << 8) | (((uint16_t)(ret) & (uint16_t)0xff00U) >> 8); //return swapped, uapi/linux/swab.h
}

static void i2c_poll_joystick(void){ //poll data from i2c device
	poll_clock_start = get_time_double();

	int read_limit = input_regs_count, ret = 0;
	bool update_adc = false; //limit adc pull rate

	if ((i2c_adc_poll_loop == i2c_adc_poll && (uhid_js_left_enable || uhid_js_left_external_enable || uhid_js_right_enable || uhid_js_right_external_enable)) || i2c_poll_rate_disable){update_adc = true; i2c_adc_poll_loop = 0;}
	if (update_adc){if (uhid_js_right_enable){read_limit += 6;} else if (uhid_js_left_enable){read_limit += 3;}}

	if (!i2c_disabled){ret = i2c_smbus_read_i2c_block_data(i2c_fd, 0, read_limit, (uint8_t *)&i2c_joystick_registers);
	} else { //uhid stress mode
		uint8_t tmp; update_adc = true;
		if (poll_stress_loop > 0){tmp=0xFF; poll_stress_loop=0;} else {tmp=0; poll_stress_loop++;} //toogle all data between 0 and 255
		for (int i=0; i<=input_regs_count+6; i++){((uint8_t *)&i2c_joystick_registers)[i] = tmp;} //write data to registers struct
	}

	if (ret < 0){
		i2c_errors_count++; i2c_allerrors_count++;
		if (ret == -6) {print_stderr("FATAL: i2c_smbus_read_i2c_block_data() failed, errno:%d (%m)\n", -ret); kill_resquested = true;}
		if (i2c_errors_count >= i2c_errors_report) {print_stderr("WARNING: I2C requests failed %d times in a row\n", i2c_errors_count); i2c_errors_count = 0;}
		i2c_last_error = ret; return;
	}

	if (i2c_errors_count > 0){
		print_stderr("last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));
		i2c_last_error = i2c_errors_count = 0; //reset error count
	}

	uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0; //merge to word to ease work

	//dpad
	const uint8_t dpad_lockup[16] = {0/*0:none*/, 1/*1:up*/, 5/*2:down*/, 1/*3:up+down*/, 7/*4:left*/, 2/*5:up+left*/, 6/*6:down+left*/, 1/*7:up+down+left*/, 3/*8:right*/, 2/*9:up+right*/, 4/*10:down+right*/, 1/*11:up+down+right*/, 7/*12:left+right*/, 1/*13:up+left+right*/, 5/*14:down+left+right*/, 0/*15:up+down+left+right*/};
	gamepad_report.hat0 = dpad_lockup[(uint8_t)(~(inputs >> mcu_input_dpad_start_index) & 0x0F)];

	//digital: reorder raw input to ev mapping
	uint16_t input_report_digital = 0xFFFF; uint8_t input_report_digital_misc = 0xFF;
	for (int i=0; i<mcu_input_map_size; i++){
		int16_t curr_input = mcu_input_map[i];
		if (curr_input != -127 && ~(inputs >> i) & 0b1) {
			if (curr_input >= BTN_MISC && curr_input < BTN_9 + 1){input_report_digital_misc &= ~(1U << (abs(curr_input - BTN_MISC))); //misc
			} else if (curr_input >= BTN_GAMEPAD && curr_input < BTN_THUMBR + 1){input_report_digital &= ~(1U << (abs(curr_input - BTN_GAMEPAD)));} //gamepad
		}
	}
	gamepad_report.buttons7to0 = ~(input_report_digital & 0xFF);
	gamepad_report.buttons15to8 = ~(input_report_digital >> 8);
	gamepad_report.buttonsmisc = ~input_report_digital_misc;

	//analog
	if (update_adc){
		if (uhid_js_left_enable){
			adc_data[0].raw = ((i2c_joystick_registers.a0_msb << 8) | (i2c_joystick_registers.a1a0_lsb & 0x0F) << 4) >> (16 - adc_data[0].res);
			adc_data[1].raw = ((i2c_joystick_registers.a1_msb << 8) | (i2c_joystick_registers.a1a0_lsb & 0xF0)) >> (16 - adc_data[1].res);
		} else if (uhid_js_left_external_enable){ //MCP3021 ADC implement for test purpose
			adc_data[0].raw = MCP3021_read(i2c_adc_fd[0]);
			adc_data[1].raw = MCP3021_read(i2c_adc_fd[1]);
		}

		if (uhid_js_right_enable){
			adc_data[2].raw = ((i2c_joystick_registers.a2_msb << 8) | (i2c_joystick_registers.a3a2_lsb & 0x0F) << 4) >> (16 - adc_data[2].res);
			adc_data[3].raw = ((i2c_joystick_registers.a3_msb << 8) | (i2c_joystick_registers.a3a2_lsb & 0xF0)) >> (16 - adc_data[3].res);
		} else if (uhid_js_right_external_enable){ //MCP3021 ADC implement for test purpose
			adc_data[2].raw = MCP3021_read(i2c_adc_fd[2]);
			adc_data[3].raw = MCP3021_read(i2c_adc_fd[3]);
		}

		for (uint8_t i=0; i<4; i++){
			if (((uhid_js_left_enable || uhid_js_left_external_enable) && i <= 1) || ((uhid_js_right_enable || uhid_js_right_external_enable) && i >= 2)){
				if (adc_firstrun){
					if(adc_data[i].autocenter){adc_data[i].offset = adc_data[i].raw - (adc_data[i].res_limit / 2); //auto center enable
					} else {adc_data[i].offset = (((adc_data[i].max - adc_data[i].min) / 2) + adc_data[i].min) - (adc_data[i].res_limit / 2);}
					adc_data[i].raw_prev = adc_data[i].raw;
				}
				
				if(adc_data[i].raw < adc_data[i].raw_min){adc_data[i].raw_min = adc_data[i].raw;} //update min value
				if(adc_data[i].raw > adc_data[i].raw_max){adc_data[i].raw_max = adc_data[i].raw;} //update max value

				adc_data[i].value = adc_defuzz(adc_data[i].raw, adc_data[i].raw_prev, adc_data[i].fuzz);
				adc_data[i].raw_prev = adc_data[i].raw;

				adc_data[i].value = adc_correct_offset_center(adc_data[i].res_limit, adc_data[i].value, adc_data[i].min, adc_data[i].max, adc_data[i].offset, adc_data[i].flat_in_comp, adc_data[i].flat_out_comp); //re-center adc value, apply flats and extend to adc range
				if(adc_data[i].reversed){adc_data[i].value = abs(adc_data[i].res_limit - adc_data[i].value);} //reverse value

				adc_data[i].value <<= 16 - adc_data[i].res; //convert to 16bits for report
				if (adc_data[i].value < 1){adc_data[i].value = 1;} else if (adc_data[i].value > 0xFFFF-1){adc_data[i].value = 0xFFFF-1;} //Reicast overflow fix

				*js_values[i] = (uint16_t)adc_data[i].value;
			}
		}
		
		adc_firstrun = false;
	}
	i2c_adc_poll_loop++;

	//report
	int report_val = 0, report_prev_val = 1;
	if (!update_adc){
		report_val = gamepad_report.buttons7to0 + gamepad_report.buttons15to8 + gamepad_report.buttonsmisc + gamepad_report.hat0;
		report_prev_val = gamepad_report_prev.buttons7to0 + gamepad_report_prev.buttons15to8 + gamepad_report.buttonsmisc + gamepad_report_prev.hat0;
	}

	i2c_poll_duration = get_time_double() - poll_clock_start;

	if (report_val != report_prev_val){gamepad_report_prev = gamepad_report; uhid_send_event(uhid_fd);} //uhid update
}


//IRQ related functions
#ifdef USE_PIGPIO_IRQ
static void gpio_callback(int gpio, int level, uint32_t tick){ //seems to work
	switch (level) {
		case 0:
			if(debug) print_stdout("DEBUG: GPIO%d low\n", gpio);
			i2c_poll_joystick();
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
	if (debug){print_stdout("DEBUG: GPIO%d triggered\n", nINT_GPIO);}
    i2c_poll_joystick();
}
#endif


//TTY related functions
static void tty_signal_handler(int sig){ //handle signal func
	if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
	kill_resquested = true;
}


//Debug related functions
static void debug_print_binary_int(int val, int bits, char* var){ //print given var in binary format
	if(!debug) return;
	printf("DEBUG: BIN: %s : ", var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\n");
}

static void debug_print_binary_int_term (int line, int col, int val, int bits, char* var){ //print given var in binary format at given term position
	printf("\e[%d;%dH\e[1;100m%s : ", line, col, var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\e[0m");
}

//SHM related functions
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

static int file_write(char* path, char* content){ //write file
	FILE *filehandle = fopen(path, "w");
	if (filehandle != NULL) {
		fprintf(filehandle, "%s", content);
		fclose(filehandle);
	} else {print_stderr("failed to write '%s'\n", path); return -EFAULT;}
	return 0;
}

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
		if(shm_path[strlen(shm_path)-1] == '/'){shm_path[strlen(shm_path)-1] = '\0';}
		if (folder_create(shm_path, 0666, user_uid, user_gid) < 0){return;} //recursive folder create

		//log file
		sprintf(curr_path, "%s/driver.log", shm_path);
		logs_fh = fopen(curr_path, "w+");
		if (logs_fh != NULL) {print_stdout("logs: %s\n", curr_path);
		} else {print_stderr("failed to open '%s' (%m)\n", curr_path);}

		shm_enable=true;
	} else if (shm_enable) {
		//status file
		sprintf(curr_path, "%s/status", shm_path);
		if (file_write (curr_path, "1") == 0){print_stdout("'%s' content set to 1\n", curr_path);
		}else{return;}

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

static void shm_close(void){ //close shm related things
	if (shm_enable){
		char curr_path[strlen(shm_path)+256]; //current path with additionnal 255 chars for filename
		sprintf(curr_path, "%s/status", shm_path); //current path
		if (file_write (curr_path, "0") == 0){print_stdout("'%s' content set to 0\n", curr_path);}
	}
	if (logs_fh != NULL){fclose(logs_fh);}
}


//Generic functions
static int in_array_int16 (int16_t* arr, int16_t value, int arr_size){ //search in value in int16 array, return index or -1 on failure
    for (int i=0; i < arr_size; i++) {if (arr[i] == value) {return i;}}
    return -1;
}

static void program_close (void){ //regroup all close functs
	uhid_destroy(uhid_fd);
	i2c_close();
	shm_close();
}

static void program_get_path (char** args, char* var){ //get current program path
	char tmp_path[PATH_MAX], tmp_subpath[PATH_MAX];
	struct stat file_stat = {0};

	strcpy(tmp_path, args[0]);
	if (args[0][0]=='.'){strcpy(var, ".\0");}

	char *tmpPtr = strtok (tmp_path, "/");
	while (tmpPtr != NULL) {
		sprintf(tmp_subpath, "%s/%s", var, tmpPtr);
		if (stat(tmp_subpath, &file_stat) == 0){
			if (S_ISDIR(file_stat.st_mode) != 0){strcpy(var, tmp_subpath);}
		}
		tmpPtr = strtok (NULL, "/");
	}

	if (strcmp(var, "./.") == 0){getcwd(var, PATH_MAX);}
	if (debug){print_stdout("program path:'%s'\n", var);}
}


//main functions, obviously, no? lool
static void program_usage (char* program){
	fprintf(stdout, "Version: %s\n", programversion);
	fprintf(stdout, "Example : %s -debug -configset disable_pollrate=1\n", program);
	fprintf(stdout, "Need to run as root.\n"
	"Arguments:\n"
	"\t-h or -help: show arguments list.\n"
	"\t-configreset: reset configuration file to default (*).\n"
	"\t-configset: set custom configuration variable with specific variable, format: 'VAR=VALUE' (e.g. debug=1) (*).\n"
	"\t-configlist: list all configuration variables (*).\n"
	"\t-noi2c: disable IRQ, I2C polls and pollrate, generate garbage data for UHID, mainly used for benchmark. (mostly crash EV monitoring softwares).\n"
	"\t-nouhid: disable IRQ, UHID reports and pollrate, mainly used for benchmark. (mostly crash EV monitoring softwares).\n"
	"(*): close program after function executed (incl failed).\n"
	);
}

int main (int argc, char** argv){
	program_start_time = get_time_double();

	if (argc > 1 && (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0)){program_usage(argv[0]); return 0;}
	if (getuid() != 0) {print_stderr("FATAL: this program needs to run as root, current user:%d\n", getuid()); return EXIT_FAILURE;}

	const char *path = "/dev/uhid";
	int ret, main_return = EXIT_SUCCESS;

	//program arguments parse. TODO
	program_get_path(argv, program_path); //get current program path
	{char tmp_cfg_filename[strlen(cfg_filename)+1]; strcpy(tmp_cfg_filename, cfg_filename); sprintf(cfg_filename, "%s/%s", program_path, tmp_cfg_filename);} //convert config relative to full path

	for(int i=1; i<argc; ++i){
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
	}

	config_parse(cfg_vars, cfg_vars_arr_size, cfg_filename, user_uid, user_gid); //parse config file, create if needed

	shm_init(true); //init shm path and files

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
	//signal(SIGKILL, tty_signal_handler); //doesn't work, program get killed before able to handle

	//mcu
	i2c_fd = i2c_open(i2c_bus, i2c_addr);
	i2c_fd_sec = i2c_open(i2c_bus, i2c_addr_sec);
	ret = i2c_smbus_read_byte_data(i2c_fd, offsetof(struct i2c_joystick_register_struct, manuf_ID) / sizeof(uint8_t)); //check signature
	if (ret != i2c_dev_manuf){
		if (ret < 0){i2c_allerrors_count++; print_stderr("FATAL: reading I2C device signature failed, errno:%d (%m)\n", -ret);
		} else {print_stderr("FATAL: invalid I2C device signature: 0x%02X\n", ret);}
		program_close();
		return EXIT_FAILURE;
	}

	ret = i2c_smbus_read_word_data(i2c_fd, offsetof(struct i2c_joystick_register_struct, device_ID) / sizeof(uint8_t)); //check version
	if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("FATAL: reading I2C device version failed, errno:%d (%m)\n", -ret);
		program_close();
		return EXIT_FAILURE;
	}
	i2c_dev_id = ret & 0xFF;
	i2c_dev_minor = (ret >> 8) & 0xFF;
	print_stdout("I2C device detected, signature:0x%02X, id:%d, version:%d\n", i2c_dev_manuf, i2c_dev_id, i2c_dev_minor);
	logs_write("I2C device: signature:0x%02X, id:%d, version:%d\n\n", i2c_dev_manuf, i2c_dev_id, i2c_dev_minor);

	//external adc
	if (mcu_js_enable[0]){i2c_addr_adc[0] = 0xFF; i2c_addr_adc[1] = 0xFF;} //disable external adc0-1 if mcu js0 explicitly enabled
	if (mcu_js_enable[1]){i2c_addr_adc[2] = 0xFF; i2c_addr_adc[3] = 0xFF;} //disable external adc2-3 if mcu js1 explicitly enabled

	if (i2c_addr_adc[0] != 0xFF && i2c_addr_adc[1] != 0xFF){ //external js0
		i2c_adc_fd[0] = i2c_open(i2c_bus, i2c_addr_adc[0]);
		i2c_adc_fd[1] = i2c_open(i2c_bus, i2c_addr_adc[1]);
		uhid_js_left_external_enable = true;
	}

	if (i2c_addr_adc[2] != 0xFF && i2c_addr_adc[3] != 0xFF){ //external js1
		i2c_adc_fd[2] = i2c_open(i2c_bus, i2c_addr_adc[2]);
		i2c_adc_fd[3] = i2c_open(i2c_bus, i2c_addr_adc[3]);
		uhid_js_right_external_enable = true;
	}

	if (uhid_js_left_external_enable || uhid_js_right_external_enable){
		print_stdout("detected external ADC configuration: %s %s\n", uhid_js_left_external_enable ? "JS0:left" : "", uhid_js_right_external_enable ? "JS1:right" : "");
	}

	//set proper register addresses
	i2c_mcu_register_adc_conf = offsetof(struct i2c_joystick_register_struct, adc_conf_bits) / sizeof(uint8_t);
	i2c_mcu_register_adc_res = offsetof(struct i2c_joystick_register_struct, adc_res) / sizeof(uint8_t);
	i2c_mcu_register_config0 = offsetof(struct i2c_joystick_register_struct, config0) / sizeof(uint8_t);
	print_stdout("MCU detected registers: adc_conf_bits:0x%02X, adc_res:0x%02X, config0:0x%02X\n", i2c_mcu_register_adc_conf, i2c_mcu_register_adc_res, i2c_mcu_register_config0);

	//update mcu config
    ret = i2c_smbus_read_byte_data(i2c_fd, i2c_mcu_register_config0);
    if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("FATAL: reading MCU config0 (0x%02X) failed, errno:%d (%m)\n", i2c_mcu_register_config0, -ret);
		program_close();
		return EXIT_FAILURE;
	}

    mcu_config0.bits = (uint8_t)ret;
	mcu_config0.vals.debounce_level = digital_debounce;
	ret = i2c_smbus_write_byte_data(i2c_fd, i2c_mcu_register_config0, mcu_config0.bits); //update i2c config
    if (ret < 0){
		i2c_allerrors_count++;
		print_stderr("FATAL: writing MCU config0 (0x%02X) failed, errno:%d (%m)\n", i2c_mcu_register_config0, -ret);
		program_close();
		return EXIT_FAILURE;
	}

	//mcu analog
	if(!uhid_js_left_external_enable || !uhid_js_right_external_enable){ //no external adc, fallback to mcu
		ret = i2c_smbus_read_byte_data(i2c_fd, i2c_mcu_register_adc_res); //current ADC resolution
		if (ret < 0){
			i2c_allerrors_count++;
			print_stderr("FATAL: reading MCU ADC resolution (0x%02X) failed, errno:%d (%m)\n", i2c_mcu_register_adc_res, -ret);
			program_close();
			return EXIT_FAILURE;
		}
		
		if (ret == 0){
			mcu_js_enable[0] = false; mcu_js_enable[1] = false;
			print_stdout("WARNING: MCU ADC is currently disabled, please refer to documentation for more informations\n");
		} else {
			int tmp_adc_res = ret;

			//detect analog config
			ret = i2c_smbus_read_byte_data(i2c_fd, i2c_mcu_register_adc_conf);
			if (ret < 0){
				i2c_allerrors_count++;
				print_stderr("FATAL: reading MCU ADC configuration (0x%02X) failed, errno:%d (%m)\n", i2c_mcu_register_adc_conf, -ret);
				program_close();
				return EXIT_FAILURE;
			}

			typedef union {struct {uint8_t use0:2, use1:2, en0:2, en1:2;} vals;	uint8_t bits;} mcu_adc_conf_t;
			mcu_adc_conf_t mcu_conf_current = {.bits=(uint8_t)ret};
			mcu_adc_conf_t mcu_conf_new = {.vals.en0=mcu_conf_current.vals.en0, .vals.en1=mcu_conf_current.vals.en1};

			print_stdout("current MCU ADC configuration:\n");
			for (uint8_t i=0; i<4; i++){print_stdout("ADC%d: enabled:%d, used:%d\n", i, (((uint8_t)ret >> (i+4)) & 0b1)?1:0, (((uint8_t)ret >> i) & 0b1)?1:0);}

			if(mcu_js_enable[0] && mcu_conf_current.vals.en0==3){
				adc_data[0].res = adc_data[1].res = tmp_adc_res;
				uhid_js_left_enable = true;
				mcu_conf_new.vals.use0 = 3;
			}

			if(mcu_js_enable[1] && mcu_conf_current.vals.en1==3){
				adc_data[2].res = adc_data[3].res = tmp_adc_res;
				uhid_js_right_enable = true;
				mcu_conf_new.vals.use1 = 3;
			}

			if (ret != mcu_conf_new.bits){ //need update mcu value
				uint8_t ret_back = (uint8_t)ret;
				ret = i2c_smbus_write_byte_data(i2c_fd, i2c_mcu_register_adc_conf, mcu_conf_new.bits);
				if (ret < 0){
					i2c_allerrors_count++;
					print_stderr("failed to set new MCU ADC configuration (0x%02X), errno:%d (%m)\n", i2c_mcu_register_adc_conf, -ret);
				}

				ret = i2c_smbus_read_byte_data(i2c_fd, i2c_mcu_register_adc_conf);
				if (ret != mcu_conf_new.bits){print_stderr("failed to update MCU ADC configuration, should be 0x%02X, is 0x%02X\n", mcu_conf_new.bits, ret);
				} else {print_stdout("MCU ADC configuration updated, is now 0x%02X, was 0x%02X\n", ret, ret_back);}
			}

			if (uhid_js_left_enable || uhid_js_right_enable){print_stdout("detected MCU ADC configuration: %s %s\n", uhid_js_left_enable ? "JS0:left" : "", uhid_js_right_enable ? "JS1:right" : "");}
		}
	}

	if (i2c_disabled){ //i2c disable , enable js0 and js1
		uhid_js_left_enable = uhid_js_right_enable = true;
		adc_data[0].res = adc_data[1].res = adc_data[2].res = adc_data[3].res = 10;
		adc_data[0].res_limit = adc_data[1].res_limit = adc_data[2].res_limit = adc_data[3].res_limit = 1023;
		adc_data[0].flat_in = adc_data[1].flat_in = adc_data[2].flat_in = adc_data[3].flat_in = 10;
		adc_data[0].flat_out = adc_data[1].flat_out = adc_data[2].flat_out = adc_data[3].flat_out = 10;
	}

	//check adc configuration
	if (uhid_js_left_enable || uhid_js_left_external_enable || uhid_js_right_enable || uhid_js_right_external_enable){
		for (uint8_t i=0; i<4; i++){
			if (((uhid_js_left_enable || uhid_js_left_external_enable) && i < 2) || ((uhid_js_right_enable || uhid_js_right_external_enable) && i > 1)){
				adc_data[i].res_limit = 0xFFFFFFFF >> (32 - adc_data[i].res); //compute adc limit
				if(adc_data[i].max > adc_data[i].res_limit) {
					print_stdout("WARNING: adc%d_max (%d) over ADC resolution (%u), limited to said resolution\n", i, adc_data[i].max, adc_data[i].res_limit);
					adc_data[i].max = adc_data[i].res_limit;
				} else {print_stdout("ADC%d resolution: %dbits (%u)\n", i, adc_data[i].res, adc_data[i].res_limit);}

				unsigned int adc_halfres = (adc_data[i].res_limit / 2);
				if (adc_data[i].flat_in < 0){adc_data[i].flat_in = 0;} else if (adc_data[i].flat_in > 35){adc_data[i].flat_in = 35;}
				if (adc_data[i].flat_out < 0){adc_data[i].flat_out = 0;} else if (adc_data[i].flat_out > 35){adc_data[i].flat_out = 35;}
				adc_data[i].flat_in_comp = adc_halfres * adc_data[i].flat_in / 100; //compute inside flat
				adc_data[i].flat_out_comp = (adc_halfres * (100 + adc_data[i].flat_out) / 100) - adc_halfres; //compute outside flat
				if (adc_data[i].flat_in_comp < 0){adc_data[i].flat_in_comp = 0;} if (adc_data[i].flat_out_comp < 0){adc_data[i].flat_out_comp = 0;}
				print_stdout("ADC%d computed flats: inside:%d, outside:%d\n", i, adc_data[i].flat_in_comp, adc_data[i].flat_out_comp);
			}
		}

		if (nINT_GPIO >= 0){print_stdout("IRQ disabled because ADC enabled\n"); nINT_GPIO = -1;}
	}
	
	if (i2c_disabled || uhid_disabled){
		i2c_poll_rate_disable = true;
		print_stdout("running in a specific mode (no-i2c:%d, no-uhid:%d), pollrate disabled\n", i2c_disabled, uhid_disabled);
		if (nINT_GPIO!=-1){print_stderr("IRQ disabled\n"); nINT_GPIO = -1;}
	}

/*
	//correct mcu input map, to keep for future enhancement
	if (mcu_nINT_shared != 0x8000 && nINT_GPIO!=-1){ //nInt shared with a input button
		ret = in_array_int16 (mcu_input_map, mcu_nINT_shared, mcu_input_map_size);
		if (ret != -1){
			print_stdout("button %d (%d) unmapped because shared with nInt (GPIO%d)\n", ret, mcu_input_map[ret], nINT_GPIO);
			mcu_input_map[ret] = -127;
		}
	}
*/

	shm_init(false); //init shm i2c things

	//uhdi defs
	if (!uhid_disabled){ //don't create uhid device is -nouhid set
		uhid_fd = open(path, O_RDWR | O_CLOEXEC);
		if (uhid_fd < 0) {
			print_stderr("failed to open uhid-cdev %s, errno:%d (%m)\n", path, -uhid_fd);
			program_close();
			return EXIT_FAILURE;
		}
		print_stdout("uhid-cdev %s opened\n", path);

		ret = uhid_create(uhid_fd);
		if (ret < 0) {program_close(); return EXIT_FAILURE;}
		print_stdout("uhid device created\n");
	}

	//initial poll
	i2c_poll_rate_time = 1. / i2c_poll_rate; //poll interval in sec
	if (i2c_adc_poll < 1){i2c_adc_poll = 1;}
	i2c_adc_poll_loop = i2c_adc_poll;
	i2c_poll_joystick(); 

	//log adc config
	if (uhid_js_left_enable || uhid_js_left_external_enable || uhid_js_right_enable || uhid_js_right_external_enable){
		logs_write("ADC configuration:\n");
		if (uhid_js_left_enable || uhid_js_left_external_enable){for (uint8_t i=0; i<2; i++){logs_write("-ADC%d (%s): resolution:%d(%d), min:%d, max:%d, flat(inner):%d%%, flat(outer):%d%%, reversed:%d, autocenter:%d (offset:%d)\n", i, uhid_js_left_external_enable?"extern":"mcu", adc_data[i].res, adc_data[i].res_limit, adc_data[i].min, adc_data[i].max, adc_data[i].flat_in, adc_data[i].flat_out, adc_data[i].reversed?1:0, adc_data[i].autocenter?1:0, adc_data[i].offset);}}
		if (uhid_js_right_enable || uhid_js_right_external_enable){for (uint8_t i=2; i<4; i++){logs_write("-ADC%d (%s): resolution:%d(%d), min:%d, max:%d, flat(inner):%d%%, flat(outer):%d%%, reversed:%d, autocenter:%d (offset:%d)\n", i, uhid_js_left_external_enable?"extern":"mcu", adc_data[i].res, adc_data[i].res_limit, adc_data[i].min, adc_data[i].max, adc_data[i].flat_in, adc_data[i].flat_out, adc_data[i].reversed?1:0, adc_data[i].autocenter?1:0, adc_data[i].offset);}}
		logs_write("\n");
	}

	if (nINT_GPIO >= 0){
	#ifdef USE_WIRINGPI_IRQ
		#define WIRINGPI_CODES 1 //allow error code return
		int err;
		if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
			print_stderr("failed to initialize wiringPi, errno:%d\n", -err);
		} else {
			if ((err = wiringPiISR(nINT_GPIO, INT_EDGE_FALLING, &mcu_irq_handler)) < 0){
				print_stderr("wiringPi failed to set callback for GPIO%d\n", nINT_GPIO);
			} else {
				print_stdout("using wiringPi IRQ on GPIO%d\n", nINT_GPIO);
				irq_enable = true;
			}
		}
	#elif defined(USE_PIGPIO_IRQ)
		int ver, err; bool irq_failed=false;
		if ((ver = gpioInitialise()) > 0){
			print_stdout("pigpio: version: %d\n",ver);
		} else {
			print_stderr("failed to detect pigpio version\n"); irq_failed = true
		}

		if (!irq_failed && nINT_GPIO > 31){
			print_stderr("pigpio limited to GPIO0-31, asked for %d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetMode(nINT_GPIO, PI_INPUT)) != 0){ //set as input
			print_stderr("pigpio failed to set GPIO%d to input\n", nINT_GPIO); irq_failed = true;
		}
		
		if (!irq_failed && (err = gpioSetPullUpDown(nINT_GPIO, PI_PUD_UP)) != 0){ //set pull up
			print_stderr("pigpio failed to set PullUp for GPIO%d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioGlitchFilter(nINT_GPIO, 100)) != 0){ //glitch filter to avoid bounce
			print_stderr("pigpio failed to set glitch filter for GPIO%d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetAlertFunc(nINT_GPIO, gpio_callback)) != 0){ //callback setup
			print_stderr("pigpio failed to set callback for GPIO%d\n", nINT_GPIO); irq_failed = true;
		} else {
			print_stdout("using pigpio IRQ on GPIO%d\n", nINT_GPIO);
			gpioSetSignalFunc(SIGINT, tty_signal_handler); //ctrl-c
			gpioSetSignalFunc(SIGtty, tty_signal_handler); //SIGtty from htop or other
			irq_enable = true;
		}
	#endif
	}

    fprintf(stderr, "Press '^C' to quit...\n");
	
	if (irq_enable){
		while (!kill_resquested){ //sleep until app close requested
			shm_update();
			usleep((useconds_t)(shm_update_interval * 1000000));
		}
	} else {
		if (!i2c_poll_rate_disable){print_stdout("pollrate: digital:%dhz, adc:%dhz\n", i2c_poll_rate, i2c_poll_rate / i2c_adc_poll);} else {print_stdout("poll speed not limited\n");}
		while (!kill_resquested){
			i2c_poll_joystick();

			if (shm_clock_start < -0.1) {shm_clock_start = poll_clock_start;} //used for shm interval
			if (debug_adv && poll_benchmark_clock_start < -0.1) {poll_benchmark_clock_start = poll_clock_start;} //benchmark

			//shm
			if (poll_clock_start - shm_clock_start > shm_update_interval){
				shm_update();
				shm_clock_start = -1.;
			}

			//poll rate implement
			if (kill_resquested) break;
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

	if (uhid_js_left_enable || uhid_js_right_enable || uhid_js_left_external_enable || uhid_js_right_external_enable){
		print_stdout("Detected ADC limits:\n");
		logs_write("Detected ADC limits:\n");
		for (uint8_t i=0; i<4; i++){
			if (adc_data[i].raw_min != INT_MAX){
				print_stdout("ADC%d: min:%d max:%d\n", i, adc_data[i].raw_min, adc_data[i].raw_max);
				logs_write("-ADC%d: min:%d, max:%d\n", i, adc_data[i].raw_min, adc_data[i].raw_max);
			}
		}
	}

	program_close();

	return main_return;
}
