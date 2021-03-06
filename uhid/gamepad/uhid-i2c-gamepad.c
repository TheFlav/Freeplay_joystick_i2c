/*
FreeplayTech UHID gamepad driver

This program sets up a gamepad device in the sytem using UHID-kernel interface.
Current version is mainly meant to be used with FreeplayTech gen2 device (embedded ATTINY controller for IO/ADC management).

Notes when using Pi Zero 2 W and willing to use WiringPi for interrupt:
You may need to clone and compile for unofficial github repository as official WiringPi ended development, please refer to: https://github.com/PinkFreud/WiringPi

Notes about ADCs (MCU or external), comments relative to current state of development, may change in the future:
- Driver/Diagnostic program are designed to only handle unsigned ADC values.
- Driver should be able to work upto 32bits(unsigned) ADC resolution.
- Diagnostic program is limited to 16bits(unsigned) because of terminal size limitations (mainly thinked to work on 640x480 screen, 80 cols by 30 lines).

Notes specific to driver part:
- At driver start, file shm_path(driver_main.h)/status will be created.
    This file content will be set to "1" at driver start and "0" when driver closes.
    If content set to "2" by external program, driver will enter lock state, a last report will set all inputs to "no pressed" (center value for analog).
    To exit lock state, set back content to "1".
- Automatically reload configuation file if its modification time changes (doesn't affect I2C related settings).

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
#include <dirent.h>
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

#ifdef ALLOW_EXT_ADC //external ADC
    #include "driver_adc_external.h"
#endif
#ifndef ALLOW_MCU_SEC_I2C
    #undef USE_SHM_REGISTERS //can't use shm register "bridge" with MCU secondary feature
#endif
#ifdef DIAG_PROGRAM
    #undef USE_WIRINGPI
    #undef USE_GPIOD
    #undef USE_SHM_REGISTERS
#else
    #include "driver_hid_desc.h"
#endif

#if defined(USE_WIRINGPI) && defined(USE_GPIOD) //irq
    #error "Only one kind of IRQ can be enabled at once, please refer to README.md for more informations."
#elif defined(USE_WIRINGPI)
    #include <wiringPi.h>
#elif defined(USE_GPIOD)
    #include <gpiod.h>
	struct gpiod_chip *gpiod_chip;
	struct gpiod_line *gpiod_input_line;
    int gpiod_fd = -1;
    char gpiod_consumer_name[128];
    #ifdef ALLOW_MCU_SEC_I2C
        struct gpiod_line *gpiod_lowbatt_line;
        int gpiod_lowbatt_fd = -1;
        char gpiod_consumer_lowbatt_name[128];
    #endif
#endif

#include "driver_main.h"

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
double get_time_double(void){ //get time in double (seconds), takes around 82 microseconds to run
    struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
    if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
    return -1.; //failed
}

//UHID related functions
#ifndef DIAG_PROGRAM
static int uhid_create(int fd){ //create uhid device
    if (!io_fd_valid(fd)){return -EIO;}
    struct uhid_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = UHID_CREATE2;

    char buffer[strlen(uhid_device_name) + int_digit_count(uhid_device_id) + 2];
    sprintf(buffer, "%s %d", uhid_device_name, uhid_device_id);
    strcpy((char*)ev.u.create2.name, buffer);

    memcpy(ev.u.create2.rd_data, hid_descriptor, sizeof(hid_descriptor));
    ev.u.create2.rd_size = sizeof(hid_descriptor);
    ev.u.create2.bus = BUS_USB;
    ev.u.create2.vendor = hid_vendor;
    ev.u.create2.product = hid_product;
    ev.u.create2.version = 0;
    ev.u.create2.country = 0;

    return uhid_write(fd, &ev);
}

static void uhid_destroy(int fd){ //close uhid device
    if (!io_fd_valid(fd)){return;} //already closed
    struct uhid_event ev; memset(&ev, 0, sizeof(ev));
    ev.type = UHID_DESTROY;
    int ret = uhid_write(fd, &ev);
    if (ret < 0){print_stderr("failed to destroy uhid device, errno:%d (%s)\n", -ret, strerror(-ret));}
    print_stderr("uhid device destroyed\n");
}

static int uhid_write(int fd, const struct uhid_event* ev){ //write data to uhid device
    //if (!io_fd_valid(fd)){return -EIO;} //already done in uhid_send_event()
    ssize_t ret = write(fd, ev, sizeof(*ev));
    if (ret < 0){print_stderr("write to uhid device failed with errno:%d (%m)\n", -ret);
    } else if (ret != sizeof(*ev)){
        print_stderr("wrong size wrote to uhid device: %zd != %zu\n", ret, sizeof(ev));
        return -EFAULT;
    }
    return ret;
}
#endif

int uhid_send_event(int fd){ //send event to uhid device
    if (uhid_disabled){return 0;}
    if (!io_fd_valid(fd)){return -EIO;}

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

    #ifdef uhid_buttons_misc_enabled //defined in driver_config.h
        ev.u.input2.data[index++] = gamepad_report.buttonsmisc; //digital misc
    #endif

    ev.u.input2.size = index;

    #ifndef DIAG_PROGRAM
        return uhid_write(fd, &ev);
    #else
        return write(fd, &ev, sizeof(ev));
    #endif
}


//I2C related
int i2c_check_bus(int bus, int* bus_found){ //check I2C bus, return errno, set bus_found to NULL to disable bus searching
    char fd_path[strlen(def_i2c_bus_path_format)+5];
    bool quiet_mode_back = quiet_mode; quiet_mode = true; //disable output

    if (bus < 0 || bus > 255){bus = 0;}
    int bus_tmp = -1, fd = -1, tmp_addr_main = 0, tmp_addr_sec = 0;

    //test provided bus num first
    sprintf(fd_path, def_i2c_bus_path_format, bus);
    fd = open(fd_path, O_RDWR);
    if (io_fd_valid(fd) && mcu_search_i2c_addr(bus, &tmp_addr_main, &tmp_addr_sec) == 0){bus_tmp = bus;}
    close(fd);

    if (bus_tmp == -1 && bus_found != NULL){ //given num failed, search
        double start = get_time_double();
        for (int i=0; i<256; i++){
            if (get_time_double() - start > 10.){quiet_mode = quiet_mode_back; print_stderr("stopped because timed out, bus:%d\n", i-1); break;} //timeout
            sprintf(fd_path, def_i2c_bus_path_format, i);
            fd = open(fd_path, O_RDWR);
            if (io_fd_valid(fd) && mcu_search_i2c_addr(i, &tmp_addr_main, &tmp_addr_sec) == 0){close(fd); bus_tmp = i; break;}
            close(fd);
        }
    }

    if (bus_found != NULL){*bus_found = bus_tmp;}

    quiet_mode = quiet_mode_back; //restore output
    if (bus_tmp == -1){print_stderr("failed to found proper I2C bus\n"); errno = ENOENT; return -1;
    } else {print_stderr("I2C bus '%d' found\n", bus_tmp);}

    return 0;
}

int i2c_open_dev(int* fd, int bus, int addr){ //open I2C device, return 0 on success, -1:bus, -2:addr, -3:generic error
    if (bus < 0){print_stderr("invalid I2C bus:%d\n", bus); errno = ENOENT; return -1;} //invalid bus
    if (addr < 0 || addr > 127){print_stderr("invalid I2C address:0x%02X (%d)\n", addr, addr); errno = EREMOTEIO; return -2;} //invalid address

    char fd_path[strlen(def_i2c_bus_path_format)+4]; sprintf(fd_path, def_i2c_bus_path_format, bus);
    close(*fd); *fd = open(fd_path, O_RDWR);
    if (!io_fd_valid(*fd)){print_stderr("failed to open '%s', errno:%d (%s)\n", fd_path, -*fd, strerror(-*fd)); *fd = -1; return -1;}

    if (ioctl(*fd, i2c_ignore_busy ? I2C_SLAVE_FORCE : I2C_SLAVE, addr) < 0){ //invalid address
        close(*fd); *fd = -1;
        print_stderr("ioctl failed for address 0x%02X, errno:%d (%m)\n", addr, errno);
        return -2;
    }

    if (i2c_smbus_read_byte_data(*fd, 0) < 0){ //invalid address
        i2c_allerrors_count++; close(*fd); *fd = -2;
        print_stderr("failed to read from address 0x%02X, errno:%d (%m)\n", addr, errno);
        return -2;
    }

    print_stderr("I2C address:0x%02X opened\n", addr);
    return 0;
}

void i2c_close_all(void){ //close all I2C files
    int* fd_array[] = {&mcu_fd,
    #ifdef ALLOW_MCU_SEC_I2C
        &mcu_fd_sec,
    #endif
    #ifdef ALLOW_EXT_ADC
        &adc_fd[0], &adc_fd[1], &adc_fd[2], &adc_fd[3],
    #endif
    };
    int* addr_array[] = {&mcu_addr,
    #ifdef ALLOW_MCU_SEC_I2C
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
            print_stderr("I2C handle for address:0x%02X closed\n", *addr_array[i]);
            *fd_array[i] = -1;
        }
    }
}

void adc_data_compute(int adc_index){ //compute adc max value, flat in/out
    if (!adc_params[adc_index].enabled){return;}

    adc_params[adc_index].res_limit = 0xFFFFFFFF >> (32 - adc_params[adc_index].res); //compute adc limit
    if(adc_params[adc_index].max > adc_params[adc_index].res_limit) {
        print_stderr("WARNING: adc%d_max (%d) over ADC resolution (%dbits:%u), limited to said resolution\n", adc_index, adc_params[adc_index].max, adc_params[adc_index].res, adc_params[adc_index].res_limit);
        adc_params[adc_index].max = adc_params[adc_index].res_limit;
    } else {print_stderr("ADC%d resolution: %dbits (%u)\n", adc_index, adc_params[adc_index].res, adc_params[adc_index].res_limit);}

    unsigned int adc_halfres = (adc_params[adc_index].res_limit / 2);
    int_constrain(&adc_params[adc_index].flat_in, 0, 35); int_constrain(&adc_params[adc_index].flat_out, 0, 35); //limit flat to 0-35
    adc_params[adc_index].flat_in_comp = adc_halfres * adc_params[adc_index].flat_in / 100; //compute inside flat
    adc_params[adc_index].flat_out_comp = (adc_halfres * (100 + adc_params[adc_index].flat_out) / 100) - adc_halfres; //compute outside flat
    if (adc_params[adc_index].flat_in_comp < 0){adc_params[adc_index].flat_in_comp = 0;}
    if (adc_params[adc_index].flat_out_comp < 0){adc_params[adc_index].flat_out_comp = 0;}
    print_stderr("ADC%d computed flats: inside:%d, outside:%d\n", adc_index, adc_params[adc_index].flat_in_comp, adc_params[adc_index].flat_out_comp);

    if(adc_params[adc_index].autocenter){adc_params[adc_index].offset = adc_params[adc_index].raw - (adc_params[adc_index].res_limit / 2); //auto center
    } else {adc_params[adc_index].offset = (((adc_params[adc_index].max - adc_params[adc_index].min) / 2) + adc_params[adc_index].min) - (adc_params[adc_index].res_limit / 2);}
    print_stderr("ADC%d computed offset:%d\n", adc_index, adc_params[adc_index].offset);

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

int adc_correct_offset_center(int adc_resolution, int adc_value, int adc_min, int adc_max, int adc_offset, int flat_in, int flat_out){ //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent)
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

void i2c_poll_joystick(bool force_update){ //poll data from i2c device
    poll_clock_start = get_time_double();

    uint32_t inputs;
    bool update_digital = true, update_adc = false;

    if (!i2c_disabled){
        uint8_t *mcu_registers_ptr = (uint8_t*)&i2c_joystick_registers; //pointer to i2c store
        uint8_t read_limit = input_registers_count;
        bool irq_triggered = true;

        if (irq_enable && !(force_update || i2c_poll_rate_disable)){
            #ifdef USE_WIRINGPI
                if(digitalRead(irq_gpio) == HIGH){irq_triggered = false;} //no button pressed
            #elif defined(USE_GPIOD)
	            struct gpiod_line_event gpiod_event;
                if (gpiod_fd >= 0 && gpiod_line_event_read_fd(gpiod_fd, &gpiod_event) >= 0 && gpiod_event.event_type != GPIOD_LINE_EVENT_FALLING_EDGE){irq_triggered = false;}
            #endif
        }
        
        if (!irq_triggered){
            mcu_registers_ptr += read_limit; read_limit = 0; //shift register
            update_digital = false;
        }

        if ((mcu_adc_read[0] || mcu_adc_read[1]) && (force_update || i2c_adc_poll_loop == i2c_adc_poll || i2c_poll_rate_disable)){update_adc = true; i2c_adc_poll_loop = 0;}

        if (update_adc){
            if (update_digital){
                if (mcu_adc_read[1]){read_limit += 6;} else if (mcu_adc_read[0]){read_limit += 3;}
            } else {
                if (mcu_adc_read[0]){read_limit += 3;} else {mcu_registers_ptr += 3;}
                if (mcu_adc_read[1]){read_limit += 3;}
            }
        }
        
        if (read_limit > 0){
            if (i2c_smbus_read_i2c_block_data(mcu_fd, mcu_registers_ptr - (uint8_t*)&i2c_joystick_registers, read_limit, mcu_registers_ptr) < 0){
                i2c_errors_count++; i2c_allerrors_count++;
                if (errno == 6){print_stderr("FATAL: i2c_smbus_read_i2c_block_data() failed, errno:%d (%m)\n", errno); kill_requested = true;}
                if (i2c_errors_count >= i2c_errors_report){print_stderr("WARNING: I2C requests failed %d times in a row\n", i2c_errors_count); i2c_errors_count = 0;}
                i2c_last_error = errno; return;
            }
            if (i2c_errors_count > 0){print_stderr("last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error)); i2c_last_error = i2c_errors_count = 0;} //report last i2c error if high error count
            if (update_digital){
                inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0; //merge to ease work
                if (inputs != mcu_input_digital_prev){mcu_input_digital_prev = inputs;} else {update_digital = false;} //only update if needed
            }
        }
    } else { //uhid stress mode
        uint8_t tmp; update_adc = true;
        if (poll_stress_loop > 0){tmp=0xFF; poll_stress_loop=0;} else {tmp=0; poll_stress_loop++;} //toogle all data between 0 and 255
        for (int i=0; i<=input_registers_count+6; i++){((uint8_t *)&i2c_joystick_registers)[i] = tmp;} //write data to registers struct
    }
    
    if (update_digital){
        //dpad lookup
        //   U+L   UP    U+R
        //      8   1   2
        // LEFT 7   0   3 RIGHT
        //      6   5   4
        //   D+L  DOWN   D+R
        const uint8_t dpad_lookup[16] = {0/*0:none*/, 1/*1:up*/, 5/*2:down*/, 1/*3:up+down*/, 7/*4:left*/, 8/*5:up+left*/, 6/*6:down+left*/, 1/*7:up+down+left*/, 3/*8:right*/, 2/*9:up+right*/, 4/*10:down+right*/, 1/*11:up+down+right*/, 7/*12:left+right*/, 1/*13:up+left+right*/, 5/*14:down+left+right*/, 0/*15:up+down+left+right*/};
        gamepad_report.hat0 = dpad_lookup[(uint8_t)(~(inputs >> mcu_input_dpad_start_index) & 0x0F)];

        //digital: reorder raw input to ev mapping
        uint16_t input_report_digital = 0xFFFF;
        #ifdef uhid_buttons_misc_enabled //defined in driver_config.h
            uint8_t input_report_digital_misc = 0xFF;
        #endif
        for (int i=0; i<mcu_input_map_size; i++){
            int16_t curr_input = mcu_input_map[i];
            if (curr_input != -127 && ~(inputs >> i) & 0b1){
                if (curr_input >= BTN_GAMEPAD && curr_input < BTN_THUMBR + 1){input_report_digital &= ~(1U << (abs(curr_input - BTN_GAMEPAD)));} //gamepad
                #ifdef uhid_buttons_misc_enabled
                else if (curr_input >= BTN_MISC && curr_input < BTN_9 + 1){input_report_digital_misc &= ~(1U << (abs(curr_input - BTN_MISC)));} //misc
                #endif
            }
        }
        gamepad_report.buttons7to0 = ~(input_report_digital & 0xFF);
        gamepad_report.buttons15to8 = ~(input_report_digital >> 8);
        #ifdef uhid_buttons_misc_enabled
            gamepad_report.buttonsmisc = ~input_report_digital_misc;
        #endif
    }

    //analog
    if (update_adc){
        update_adc = false;
        for (int i=0; i<4; i++){ //adc loop
            if (adc_params[i].enabled){
                bool adc_failed = false;
#ifdef ALLOW_EXT_ADC
                if (!adc_fd_valid[i]){ //read mcu adc value
#endif
                    uint8_t *tmpPtr = (uint8_t*)&i2c_joystick_registers; //pointer to i2c store
                    uint8_t tmpPtrShift = i, tmpMask = 0xF0, tmpShift = 0; //pointer offset, lsb mask, lsb bitshift
                    if (i < 2){tmpPtr += input_registers_count/*mcu_i2c_register_adc0*/;} else {tmpPtr += input_registers_count + 3/*mcu_i2c_register_adc2*/; tmpPtrShift -= 2;} //update pointer
                    if (tmpPtrShift == 0){tmpMask = 0x0F; tmpShift = 4;} //adc0-2 lsb
                    adc_params[i].raw = ((*(tmpPtr + tmpPtrShift) << 8) | (*(tmpPtr + 2) & tmpMask) << tmpShift) >> (16 - adc_params[i].res); //char to word
#ifdef ALLOW_EXT_ADC
                } else if (adc_type_funct_read[adc_type[i]](adc_fd[i], &adc_params[i]) < 0){adc_failed = true;} //external adc value, read failed
#endif
                if (!adc_failed){
                    if (adc_firstrun){adc_data_compute(i); adc_params[i].raw_prev = adc_params[i].raw;} //compute adc max value, flat in/out, offset

                    if(adc_params[i].raw < adc_params[i].raw_min){adc_params[i].raw_min = adc_params[i].raw;} //update min value
                    if(adc_params[i].raw > adc_params[i].raw_max){adc_params[i].raw_max = adc_params[i].raw;} //update max value

                    adc_params[i].value = adc_defuzz(adc_params[i].raw, adc_params[i].raw_prev, adc_params[i].fuzz); adc_params[i].raw_prev = adc_params[i].raw; //defuzz
                    adc_params[i].value = adc_correct_offset_center(adc_params[i].res_limit, adc_params[i].value, adc_params[i].min, adc_params[i].max, adc_params[i].offset, adc_params[i].flat_in_comp, adc_params[i].flat_out_comp); //re-center adc value, apply flats and extend to adc range
                    if(adc_params[i].reversed){adc_params[i].value = abs(adc_params[i].res_limit - adc_params[i].value);} //reverse value

                    adc_params[i].value <<= 16 - adc_params[i].res; //convert to 16bits value for report
                    if (adc_params[i].value < 1){adc_params[i].value = 1;} else if (adc_params[i].value > 0xFFFF-1){adc_params[i].value = 0xFFFF-1;} //Reicast overflow fix

                    if(adc_map[i] > -1){ //adc mapped to -1 should be disabled during runtime if not in diagnostic mode
                        *js_values[adc_map[i]] = (uint16_t)adc_params[i].value; update_adc = true;
                    }
                }
            }
        }
        adc_firstrun = false;
    }
    i2c_adc_poll_loop++;

    //report
    i2c_poll_duration = get_time_double() - poll_clock_start;
    #ifndef DIAG_PROGRAM
        if (update_digital || update_adc){uhid_send_event(uhid_fd);} //uhid update
    #endif
}

//MCU related functions
int mcu_search_i2c_addr(int bus, int* addr_main, int* addr_sec){ //search mcu on given i2c bus, return -1 on failure, 0 on success
    *addr_main = *addr_sec = -1;
    double start = get_time_double();

    if (bus < 0){print_stderr("invalid I2C bus:%d\n", bus); return -1;} //invalid bus
    char fd_path[strlen(def_i2c_bus_path_format)+4]; sprintf(fd_path, def_i2c_bus_path_format, bus);
    int fd = open(fd_path, O_RDWR);
    if (!io_fd_valid(fd)){print_stderr("failed to open '%s', errno:%d (%s)\n", fd_path, -fd, strerror(-fd)); return -1;}

    int tmp_reg_manuf_main = offsetof(struct i2c_joystick_register_struct, manuf_ID) / sizeof(uint8_t); //main register manuf_ID
    #ifdef ALLOW_MCU_SEC_I2C
        int tmp_reg_manuf_sec = offsetof(struct i2c_secondary_address_register_struct, manuf_ID) / sizeof(uint8_t); //secondary register manuf_ID
    #endif

    for (int addr=0; addr <=127; addr++){
        if (get_time_double() - start > 5.){print_stderr("stopped because timed out, bus:%d, addr:0x%02X\n", bus, addr-1); break;} //timeout
        #ifdef ALLOW_MCU_SEC_I2C
            if (ioctl(fd, I2C_SLAVE_FORCE, addr) < 0){print_stderr("ioctl failed for address 0x%02X, errno:%d (%m)\n", addr, errno); continue;} //invalid address
            if (i2c_smbus_read_byte_data(fd, tmp_reg_manuf_sec) != mcu_manuf){continue;} //invalid manuf_id
            int ret = i2c_smbus_read_byte_data(fd, mcu_sec_register_secondary_i2c_addr); if (ret != addr){continue;} //register content needs to match secondary address
            *addr_sec = addr;
            ret = i2c_smbus_read_byte_data(fd, mcu_sec_register_joystick_i2c_addr); if (ret < 0 || ret > 127){*addr_sec = -1; continue;} //return not in valid i2c range
            *addr_main = ret;
        #else
            *addr_main = addr;
        #endif

        //check if addr_main valid
        if (ioctl(fd, I2C_SLAVE_FORCE, *addr_main) < 0){print_stderr("ioctl failed for address 0x%02X (addr_main), errno:%d (%m)\n", *addr_main, errno);
        } else if (i2c_smbus_read_byte_data(fd, tmp_reg_manuf_main) == mcu_manuf){break;}
        *addr_sec = -1; *addr_main = -1;
    }

    close(fd);
    
    bool mcu_sec_enabled = false;
    #ifdef ALLOW_MCU_SEC_I2C
        mcu_sec_enabled = true;
    #endif

    if (*addr_main == -1 || (mcu_sec_enabled && *addr_sec == -1)){print_stderr("failed to detect MCU address\n"); return -1;
    } else if (!diag_mode_init && !quiet_mode){
        print_stderr("detected possible MCU adress, main:0x%02X", *addr_main);
        if (mcu_sec_enabled){fprintf(stderr, ", secondary:0x%02X", *addr_sec);}
        fputc('\n', stderr);
    }
    return 0;
}

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
    print_stderr("I2C device detected, signature:0x%02X, id:%d, version:%d\n", mcu_signature, mcu_id, mcu_version);
    logs_write("I2C device: signature:0x%02X, id:%d, version:%d\n\n", mcu_signature, mcu_id, mcu_version);

    if (mcu_version != mcu_version_even){
        print_stderr("WARNING: program register version (%d) mismatch MCU version (%d)\n", mcu_version_even, mcu_version);
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

    if (mcu_config0.bits == (uint8_t)ret){return 0;} //nothing changed
    ret = i2c_smbus_write_byte_data(mcu_fd, mcu_i2c_register_config0, mcu_config0.bits); //update i2c config
    if (ret < 0){
        i2c_allerrors_count++;
        print_stderr("FATAL: writing MCU config0 (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_config0, -ret);
        return -1;
    }
    return 0;
}

#if defined(ALLOW_MCU_SEC_I2C) && !defined(DIAG_PROGRAM)
int mcu_update_power_control(){ //read/update power_control register, return 0 on success, -1 on error
    int power_control_ret = i2c_smbus_read_byte_data(mcu_fd_sec, mcu_sec_register_power_control);
    if (power_control_ret < 0){
        i2c_allerrors_count++;
        print_stderr("FATAL: reading MCU sec power_control (register:0x%02X) failed, errno:%d (%m)\n", mcu_sec_register_power_control, -power_control_ret);
        return -1;
    }
    
    #ifndef USE_SHM_REGISTERS
        mcu_power_control_t mcu_power_control = {.bits=(uint8_t)power_control_ret}, mcu_power_control_back = {.bits=mcu_power_control.bits};
    #endif

    //low_batt gpio check
    int gpio_state = -1;
    if (battery_gpio_enable){ 
        #ifdef USE_WIRINGPI
            gpio_state = digitalRead(lowbattery_gpio);
            if (lowbattery_gpio_invert){gpio_state = !gpio_state;}
        #elif defined(USE_GPIOD)
            if (gpiod_lowbatt_fd >= 0){
                gpio_state = gpiod_line_get_value(gpiod_lowbatt_line);
                if (gpio_state >= 0 && lowbattery_gpio_invert){gpio_state = !gpio_state;}
            }
        #endif
        if (gpio_state > -1){
            #ifndef USE_SHM_REGISTERS
                mcu_power_control.vals.low_batt_mode = gpio_state;
            #else
                if (power_control_low_batt_mode != gpio_state){power_control_low_batt_mode = gpio_state;}
            #endif
        }
    }

    #ifndef USE_SHM_REGISTERS
        if (mcu_power_control.bits == mcu_power_control_back.bits){return 0;} //nothing changed
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect_disable) < 0){return -1;} //disable write protection
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_power_control, mcu_power_control.bits) < 0){return -1;} //update register
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect_enable) < 0){return -1;} //enable write protection
    #endif

    return 0;
}

int mcu_update_battery_capacity(){ //read/update battery_capacity register, return 0 on success, -1 on error
    struct stat battery_stat; int percent = 255;  FILE *filehandle;
    if (stat(battery_rsoc_file, &battery_stat) == 0){
        filehandle = fopen(battery_rsoc_file, "r");
        if (filehandle != NULL && !ferror(filehandle)){
            char buffer[5]; fgets(buffer, 5, filehandle);
            percent = atoi(buffer); if (percent < 0 || percent > 255){percent = 255;}
        }
        fclose(filehandle);
        if (percent != 255){i2c_secondary_registers.battery_capacity = (uint8_t)percent;}
    }

    #ifndef USE_SHM_REGISTERS
        static int percent_prev = -1;
        if (percent == percent_prev){return 0;} //nothing changed
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect_disable) < 0){return -1;} //disable write protection
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_battery_capacity, (uint8_t)percent) < 0){return -1;} //update register
        if (i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect_enable) < 0){return -1;} //enable write protection
        percent_prev = percent;
    #endif

    return 0;
}
#endif

int mcu_update_register(int* fd, uint8_t reg, uint8_t value, bool check){ //update and check register, return 0 on success, -1 on error
    if(!io_fd_valid(*fd)){return -1;}
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
    } else if (tmp_adc_res == 0){print_stderr("WARNING: MCU ADC is currently disabled, please refer to documentation for more informations\n");
    } else {print_stderr("MCU ADC resolution: %dbits\n", tmp_adc_res);}

    //mcu analog config
    int ret = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_adc_conf);
    if (ret < 0){print_stderr("FATAL: reading MCU ADC configuration (register:0x%02X) failed, errno:%d (%m)\n", mcu_i2c_register_adc_conf, -ret); i2c_allerrors_count++; return -2;}

    uint8_t mcu_adc_config_old = (uint8_t)ret;
    uint8_t mcu_adc_config_new = mcu_adc_config_old & 0xF0; //copy enable bits
    bool mcu_adc_used[4] = {0};

    for (int i=0; i<4; i++){ //mcu adc loop
        int_constrain(&adc_map[i], -1, 3); //avoid overflow
        mcu_adc_enabled[i] = (mcu_adc_config_old >> (i+4)) & 0b1; //mcu adc enabled
        if (adc_map[i] > -1 || diag_mode){
            #ifdef DIAG_PROGRAM
                if (diag_first_run){adc_map[i] = -1; adc_params[i].enabled = true;} //set adcs in a "default" state for "first run" mode
            #endif

            if (!adc_fd_valid[i]){ //external adc not used
                #ifdef ALLOW_EXT_ADC
                    adc_init_err[i] = -1;
                #endif
                adc_params[i].res = tmp_adc_res; //mcu adc resolution
                if (!mcu_adc_enabled[i]){adc_params[i].enabled = false; //mcu adc fully disable
                } else if (adc_params[i].enabled){mcu_adc_config_new |= 1U << i; mcu_adc_used[i] = true;} //mcu adc used
            }
            #ifdef ALLOW_EXT_ADC
                else { //external
                    int_constrain(&adc_type[i], 0, adc_type_count-1); //avoid overflow
                    if (adc_type_funct_init[adc_type[i]](adc_fd[i], &adc_params[i]) < 0){adc_init_err[i] = -3; //init external adc failed
                    } else if (!adc_params[i].enabled){adc_init_err[i] = -1; //not enabled
                    } else {adc_init_err[i] = 0;} //ok
                }
            #endif

            #ifdef DIAG_PROGRAM
                if (diag_first_run){ //set adcs in a "default" state for "first run" mode
                    adc_params[i].res_limit = 0xFFFFFFFF >> (32 - adc_params[i].res); //compute adc limit
                    adc_params[i].min = 0;
                    adc_params[i].max = adc_params[i].res_limit;
                }
            #endif
        } else {adc_params[i].enabled = false;} //disable adc because map is invalid
        *js_values[i] = (uint16_t)0x7FFF; //reset uhid axis value, mainly used in diag part

        //report
        if(!diag_mode_init && !quiet_mode){
            print_stderr("ADC%d: %s", i, adc_params[i].enabled?"enabled":"disabled");
            fputs(", ", stderr);
            if (mcu_adc_used[i]){fputs("MCU", stderr);
#ifdef ALLOW_EXT_ADC
            } else if (adc_fd_valid[i]){fprintf(stderr, "External(0x%02X)", adc_addr[i]);
#endif
            } else {fputs("None", stderr);}
            fprintf(stderr, ", mapped to %s(%d)\n", js_axis_names[adc_map[i]+1], adc_map[i]);
        }
    }
    
    mcu_adc_read[0] = mcu_adc_used[0] || mcu_adc_used[1]; //mcu adc0-1 used
    mcu_adc_read[1] = mcu_adc_used[2] || mcu_adc_used[3]; //mcu adc2-3 used

    if (mcu_adc_config_old != mcu_adc_config_new){ //mcu adc config needs update
        ret = i2c_smbus_write_byte_data(mcu_fd, mcu_i2c_register_adc_conf, mcu_adc_config_new);
        if (ret < 0){print_stderr("failed to set new MCU ADC configuration (0x%02X), errno:%d (%m)\n", mcu_i2c_register_adc_conf, -ret); i2c_allerrors_count++; return -2;
        } else { //read back wrote value by safety
            ret = i2c_smbus_read_byte_data(mcu_fd, mcu_i2c_register_adc_conf);
            if (ret != mcu_adc_config_new){print_stderr("failed to update MCU ADC configuration, should be 0x%02X but is 0x%02X\n", mcu_adc_config_new, ret); return -2;
            } else {print_stderr("MCU ADC configuration updated, is now 0x%02X, was 0x%02X\n", ret, mcu_adc_config_old);}
        }
    }
    return 0;
}

//TTY related functions
static void tty_signal_handler(int sig){ //handle signal func
    if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
    if (term_backup.c_cflag){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
    kill_requested = true;
}

//SHM related functions
static int file_read(char* path, char* bufferptr, int buffersize){ //read file
    int ret = 0;
    FILE *filehandle = fopen(path, "r");
    if (filehandle != NULL) {
        fgets(bufferptr, buffersize, filehandle);
        if (ferror(filehandle)){if (debug){print_stderr("failed to read '%s'\n", path);} ret = -EFAULT;}
        fclose(filehandle);
    } else {if (debug){print_stderr("failed to read '%s'\n", path);} return -EFAULT;}
    return ret;
};

static int file_write(char* path, char* content){ //write file
    FILE *filehandle = fopen(path, "w");
    if (filehandle != NULL) {
        fprintf(filehandle, "%s", content);
        fclose(filehandle);
    } else {if (debug){print_stderr("failed to write '%s'\n", path);} return -EFAULT;}
    return 0;
}

static int folder_create(char* path, int rights, int uid, int gid) { //create folder(s), set rights/uid/gui if not -1. Return number of folder created, -errno on error
    int ret, depth = 0; //security
    struct stat file_stat = {0};
    char curr_path[strlen(path)+1], sub_path[strlen(path)+2]; sub_path[0]='\0';
    int tmp_rights = (rights==-1)?0755:rights;

    strcpy(curr_path, path);
    if(curr_path[strlen(curr_path)-1] == '/'){curr_path[strlen(curr_path)-1] = '\0';}

    char *tmpPtr = strtok (curr_path, "/"); //split path
    while (tmpPtr != NULL){
        strcat(sub_path, "/"); strcat(sub_path, tmpPtr);

        if (stat(sub_path, &file_stat) == -1){
            ret = mkdir(sub_path, tmp_rights);
            if (ret < 0){if (debug){print_stderr("failed to create directory '%s', errno:%d (%m)\n", sub_path, -ret);} return ret;}
            print_stderr("directory '%s' created\n", sub_path);
            if (chmod(sub_path, tmp_rights) < 0 && debug){print_stderr("failed to set directory '%s' rights, err: %m\n", sub_path);}
            if (uid != -1 || gid != -1){if (chown(sub_path, uid, gid) < 0 && debug){print_stderr("failed to set directory '%s' owner, err: %m\n", sub_path);}}
        } else if (debug){print_stderr("directory '%s' already exist\n", sub_path);}

        depth++; if(depth > 10){if (debug){print_stderr("something gone very wrong, depth:10 hit, break\n");} break;}
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
    char cwd_back[PATH_MAX]; getcwd(cwd_back, PATH_MAX); chdir(shm_fullpath); //backup cwd and switch to shm path

    if (first){ //initial run, skip i2c part
        if (folder_create(shm_fullpath, 0755, user_uid, user_gid) < 0){return;} //recursive folder create

        //pid file
        int pid = (int)getpid();
        if (pid > 0){
            sprintf(pid_path, "%s/pid.txt", shm_fullpath);
            FILE *filehandle = fopen(pid_path, "w");
            if (filehandle != NULL) {
                fprintf(filehandle, "%d", pid); fclose(filehandle);
                print_stderr("%s set to '%d'\n", pid_path, pid);
            } else {pid_path[0] = '\0';} //failed to write pid file
        }

        //todo: log part needs rework
        char shm_logs[strlen(shm_fullpath)+13]; //current path with additionnal 255 chars for filename
        sprintf(shm_logs, "%s/driver.log", shm_fullpath); //log file
        logs_fh = fopen(shm_logs, "w+");
        if (logs_fh != NULL){
            chmod(shm_logs, 0666); chown(shm_logs, (uid_t) user_uid, (gid_t) user_gid);
            print_stderr("logs: %s\n", shm_logs);
        } else {print_stderr("failed to open '%s' (%m)\n", shm_logs);}

        shm_enable=true;
    } else if (shm_enable){
        #ifndef DIAG_PROGRAM
            char buffer[5]; sprintf(buffer, "%d", shm_status); 
            if (file_write(shm_status_path_ptr, buffer) == 0){ //status file
                print_stderr("'%s' content set to '%s'\n", shm_status_path_ptr, buffer);
                chmod(shm_status_path_ptr, 0666); chown(shm_status_path_ptr, (uid_t) user_uid, (gid_t) user_gid);
            }
        #endif

        #ifdef USE_SHM_REGISTERS //registers to files "bridge"
            for (int i=0; i<shm_vars_arr_size; i++){
                int ret = i2c_smbus_read_byte_data(*(shm_vars[i].fd), shm_vars[i].reg); //read register value
                if (ret < 0){print_stderr("failed to read register 0x%02X, errno:%d (%m)\n", shm_vars[i].reg, -ret); ret = 0;}
                for (int j=0; j<shm_vars[i].fields_count; j++){ //field loop
                    char* path = shm_vars[i].fields[j].filename;
                    if (path != NULL && strlen(path)){ //"valid" filename
                        uint8_t fields_val = (uint8_t)ret;
                        if (shm_vars[i].fields[j].bit_count == 0){shm_vars[i].fields[j].bit_count = 8;}
                        fields_val = bit_extract_uint8(fields_val, shm_vars[i].fields[j].bit_pos, shm_vars[i].fields[j].bit_count);
                        *(shm_vars[i].fields[j].var_ptr) = shm_vars[i].fields[j].var_back = fields_val;
                        sprintf(buffer, "%d", fields_val);
                        if (file_write(path, buffer) == 0){ //write file
                            chmod(path, 0666); chown(path, (uid_t) user_uid, (gid_t) user_gid); //right and owner
                            struct stat file_stat = {0}; stat(path, &file_stat); shm_vars[i].fields[j].mtime = file_stat.st_mtim; //backup file modification time
                            print_stderr("'%s' content set to '%s'\n", path, buffer);
                        }
                    }
                }
            }
        #endif
    }
    chdir(cwd_back); //restore working folder
}

#ifndef DIAG_PROGRAM
static void shm_update(void){ //update registers/files linked to shm things
    if (shm_enable){
        char cwd_back[PATH_MAX]; getcwd(cwd_back, PATH_MAX); chdir(shm_fullpath); //backup cwd and switch to shm path
        struct stat file_stat = {0};

        //check shm_path/status update
        if (stat(shm_status_path_ptr, &file_stat) == -1){
            char buffer[3]; sprintf(buffer, "%d", shm_status); 
            if (file_write(shm_status_path_ptr, buffer) == 0 && debug){print_stderr("missing '%s' content set to '%s'\n", shm_status_path_ptr, buffer);} //recreate missing status file
        } else {
            char buffer[3]; file_read(shm_status_path_ptr, buffer, 2);
            int shm_status_back = shm_status; if (buffer != NULL){shm_status = atoi(buffer);}
            if (shm_status != shm_status_back){driver_lock = (shm_status>1)?true:false;}
        }

        #ifdef USE_SHM_REGISTERS
            for (int i=0; i<shm_vars_arr_size; i++){
                int register_value = i2c_smbus_read_byte_data(*(shm_vars[i].fd), shm_vars[i].reg); //read register value
                if (register_value < 0){if (debug){print_stderr("failed to read register 0x%02X, errno:%d (%m)\n", shm_vars[i].reg, -register_value);} register_value = 0;}
                bool register_update = false;

                for (int j=0; j<shm_vars[i].fields_count; j++){
                    char* path = shm_vars[i].fields[j].filename, buffer[128];
                    struct timespec* mtime = &shm_vars[i].fields[j].mtime;
                    uint8_t bit_pos = shm_vars[i].fields[j].bit_pos, bit_count = shm_vars[i].fields[j].bit_count, *var_ptr = shm_vars[i].fields[j].var_ptr;
                    bool file_update = false, reg_update = false, var_updated = (shm_vars[i].fields[j].var_back != *var_ptr);
                    uint8_t tmp_reg = bit_extract_uint8((uint8_t)register_value, bit_pos, bit_count); //current register field value

                    if (var_updated || stat(path, &file_stat) != 0){file_update = true; reg_update = true; //var->reg,file
                    } else if (memcmp(mtime, &file_stat.st_mtim, sizeof(struct timespec)) != 0){ //file->var
                        if (shm_vars[i].reg_rw){ //allow update
                            if (file_read(path, buffer, 127) == 0){ //build updated value from file->reg,var
                                int tmp_value = atoi(buffer);
                                if (int_constrain(&tmp_value, 0, 0xFF >> (8 - bit_count)) != 0){file_update = true;} //file value outside of range, force update file with proper value
                                *var_ptr = tmp_value; *mtime = file_stat.st_mtim; reg_update = true;
                                if(debug){print_stderr("'%s' var set to '%d'\n", path, tmp_value);}
                            }
                        } else {file_update = true;} //restore file value
                    } else if (*var_ptr != tmp_reg){*var_ptr = tmp_reg; file_update = true;} //reg->var,file

                    if (file_update){ //var->file
                        sprintf(buffer, "%d", *var_ptr);
                        if (file_write(path, buffer) == 0){ //write file
                            chmod(path, 0666); chown(path, (uid_t) user_uid, (gid_t) user_gid); //right and owner
                            stat(path, &file_stat); *mtime = file_stat.st_mtim; //backup file modification time
                            if(debug){print_stderr("'%s' content set to '%s'\n", path, buffer);}
                        }
                    }

                    if (reg_update){register_update = true; register_value = bit_clear_uint8(register_value, bit_pos, bit_count); register_value |= *var_ptr << bit_pos;} //update register value
                    shm_vars[i].fields[j].var_back = *var_ptr;
                }

                if (register_update){ //var,file->reg
                    if (shm_vars[i].reg_wp != NULL){i2c_smbus_write_byte_data(*(shm_vars[i].fd), shm_vars[i].reg_wp->reg, shm_vars[i].reg_wp->disable);} //disable write protection
                    int ret = i2c_smbus_write_byte_data(*(shm_vars[i].fd), shm_vars[i].reg, (uint8_t)register_value);
                    if (shm_vars[i].reg_wp != NULL){i2c_smbus_write_byte_data(*(shm_vars[i].fd), shm_vars[i].reg_wp->reg, shm_vars[i].reg_wp->enable);} //enable write protection
                    if (debug && ret >= 0){
                        char binbuffer[9]={'\0'}; int_bin_chararray(register_value, 8, binbuffer);
                        print_stderr("register 0x%02X set to '%d'(%s)\n", shm_vars[i].reg, register_value, binbuffer);
                    }
                }
            }
        #endif
        chdir(cwd_back); //restore working folder
    }
}
#endif

static void shm_close(void){ //close shm related things
    #ifndef DIAG_PROGRAM
        if (shm_enable){
            char curr_path[strlen(shm_fullpath)+10]; //current path with additionnal 255 chars for filename
            sprintf(curr_path, "%s/status", shm_fullpath); //current path
            if (file_write (curr_path, "0") == 0){print_stderr("'%s' content set to 0\n", curr_path);}
        }
    #endif
    if (logs_fh != NULL){fclose(logs_fh);}
}


//bit/binary specific
static uint8_t bit_extract_uint8(uint8_t value, uint8_t bit_pos, uint8_t bit_count){ //warning, no security
    return (uint8_t)(value << (8 - bit_pos - bit_count)) >> (8 - bit_count);
}

static uint8_t bit_clear_uint8(uint8_t value, uint8_t bit_pos, uint8_t bit_count){ //warning, no security
    for (uint8_t i = bit_pos; i < bit_pos+bit_count; i++){value &= ~(1<<i);} return value;
}

static char* int_bin_chararray(int val, int bits, char* var){ //int to binary format char array
    char buffer[bits+1];
    for(int i=0; i<bits; i++){buffer[i]=((val >> (bits-i-1)) & 0b1)+'0';} buffer[bits]='\0';
    return strcpy(var, buffer);
}


//integer manipulation functs
void int_rollover(int* val, int min, int max){ //rollover int value between (incl) min and max, work both way
    if (*val < min){*val = max;} else if (*val > max){*val = min;}
}

int int_constrain(int* val, int min, int max){ //limit int value to given (incl) min and max value, return 0 if val within min and max, -1 under min, 1 over max
    int ret = 0;
    if (*val < min){*val = min; ret = -1;} else if (*val > max){*val = max; ret = 1;}
    return ret;
}

int int_digit_count(int num){ //number of digit of a integer, negative sign is consider as a digit
    char buffer[12];
    sprintf (buffer, "%d", num);
    return strlen(buffer);
}

int in_array_int16(int16_t* arr, int16_t value, int arr_size){ //search in value in int16 array, return index or -1 on failure
    for (int i=0; i < arr_size; i++) {if (arr[i] == value) {return i;}}
    return -1;
}


//IO specific
bool io_fd_valid(int fd){ //check if a file descriptor is valid
    int errno_back = errno; //backup previous errno
    bool valid = fcntl(fd, F_GETFD) >= 0 || errno != EBADF;
    errno = errno_back; //restore errno to avoid bad descriptor on all errors
    return valid;
}

//Generic functions
static void program_close(void){ //regroup all close functs
    if (already_killed){return;}
    if (strlen(pid_path) > 0){remove(pid_path);} //delete pid file
    if (term_backup.c_cflag){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
    #ifndef DIAG_PROGRAM
        uhid_destroy(uhid_fd);
    #endif
    i2c_close_all();
    shm_close();
    //if (cfg_delete){remove(config_path);}
    already_killed = true;
}


//main functions
#ifndef MULTI_INSTANCES
static int program_instances_count(char* path, char* program){ //scan /proc/ to check if program is already running, return number of instances found
    DIR *proc_ptr; if ((proc_ptr = opendir("/proc/")) == NULL){print_stderr("failed to open folder /proc/\n"); return 0;} //failed to open /proc/
    char fullpath[strlen(path) + strlen(program) + 2]; sprintf(fullpath, "%s/%s", path, program); //full path to current program
    struct dirent *dir_entry; struct stat file_stat; int found = 0;
    while ((dir_entry = readdir(proc_ptr)) != NULL){
        char* proc_id = dir_entry->d_name; if (!isdigit(proc_id[0])){continue;} //skip any that can't be process id
        char buffer_path[strlen(proc_id) + 13]; sprintf(buffer_path, "/proc/%s/exe", proc_id); //build full path to potencial exe symlink
        if (lstat(buffer_path, &file_stat) == 0 && S_ISLNK(file_stat.st_mode)){ //exist and symlink
            char resolved_path[PATH_MAX] = {'\0'};
            realpath(buffer_path, resolved_path);
            if (strlen(resolved_path) > 0){
                if (strcmp(fullpath, resolved_path) == 0){found++; //matching
                } else if (strlen(resolved_path) > 11){ //fix for dev, real path ends by " (deleted)" if link "broken" by new compilation
                    resolved_path[strlen(resolved_path) - 10] = '\0';
                    if (strcmp(fullpath, resolved_path) == 0){found++;}
                }
            }
        }
    }
    closedir(proc_ptr);
    return found;
}
#endif

static void program_get_path(char** args, char* path, char* program){ //get current program path based on program argv or getcwd if failed
    char tmp_path[PATH_MAX], tmp_subpath[PATH_MAX];
    struct stat file_stat = {0};
    strcpy(tmp_path, args[0]); if (args[0][0]=='.'){strcpy(path, ".\0");}
    char *tmpPtr = strtok(tmp_path, "/");
    while (tmpPtr != NULL) {
        sprintf(tmp_subpath, "%s/%s", path, tmpPtr);
        if (stat(tmp_subpath, &file_stat) == 0){
            if (S_ISDIR(file_stat.st_mode) != 0){strcpy(path, tmp_subpath); //folder
            } else {strcpy(program, tmpPtr);} //file
        }
        tmpPtr = strtok(NULL, "/");
    }
    if (strcmp(path, "./.") == 0){getcwd(path, PATH_MAX);}
    chdir(path); //useful?
    if (debug){print_stderr("program path:'%s'\n", path);}
}

static void program_usage(void){ //display help
    if (!quiet_mode){
        fprintf(stderr, "Path: %s\n", program_path);
        fprintf(stderr, "Version: %s\n", program_version);
        fprintf(stderr, "MCU register version: %d\n", mcu_version_even);
        fprintf(stderr, "Dev: %s\n\n", dev_webpage);
        fprintf(stderr, "Since it needs to access/write to system device, program has to run as root.\n\n");

        #if defined(ALLOW_MCU_SEC_I2C) || defined(USE_SHM_REGISTERS) || defined(ALLOW_EXT_ADC) || defined(USE_WIRINGPI) || defined(USE_GPIOD)
            fprintf(stderr, "Enabled feature(s) (at compilation time):\n"
            #ifdef ALLOW_MCU_SEC_I2C
                "\t-MCU secondary features, on-the-fly I2C address update, backlight control, ...\n"
            #endif
            #ifdef USE_SHM_REGISTERS
                "\t-SHM to MCU bridge, allow to direct update some registers using file system.\n"
            #endif
            #ifdef ALLOW_EXT_ADC
                "\t-External ADCs.\n"
            #endif
            #ifdef USE_WIRINGPI
                "\t-MCU digital input interrupt pin poll using WiringPi.\n"
            #endif
            #ifdef USE_GPIOD
                "\t-MCU digital input interrupt pin poll using libGPIOd.\n"
            #endif
                "\n"
            );
        #endif

        //fprintf(stderr, "Example : %s -configset debug=1\n", program);
        fprintf(stderr, "Arguments:\n\t-h or -help: show arguments list.\n");
    #ifndef DIAG_PROGRAM
        fprintf(stderr,
        "(*) : close program after function executed (incl failure).\n"
        "\t-confignocreate : disable creation of configuration file if not exist, returns specific errno if is along -closeonwarn.\n"
        "\t-configreset : reset configuration file to default (*).\n"
        "\t-configset : set custom configuration variable with specific variable, format: 'VAR=VALUE' (e.g. debug=1) (*).\n"
        "\t-configlist : list all configuration variables (*).\n"
        "\t-noi2c : disable IRQ, I2C polls and pollrate, generate garbage data for UHID, mainly used for benchmark. (can crash EV monitoring softwares).\n"
        "\t-nouhid : disable IRQ, UHID reports and pollrate, mainly used for benchmark. (can crash EV monitoring softwares).\n"
        "\t-closeonwarn : close program on major warnings.\n"
        "\t-inputsearch : check MCU input for pressed button, use input-event-codes.h numbering, example for (START) and (SELECT):'-inputsearch 0x13b 0x13a'. Needs to be defined last. Program returns 0 if none pressed, 1 for first input, 2 for second, 3 for both.\n"
        "\t-quiet or --quiet : disable all tty outputs.\n"
        );
    #else
        fprintf(stderr,
        "\t-init : allow easier detection of analog settings, does discard ADC mapping and min/max values.\n"
        "\t-noinputs : disable MCU inputs to navigate.\n"
        "\t-postmessagetest : display 'first run' post save/skip message and closes. Require '" diag_post_init_message_filename "' file to exist.\n"
        );
    #endif
    }
}


int main(int argc, char** argv){
    program_start_time = get_time_double(); //program start time, used for detailed outputs

    #ifndef DIAG_PROGRAM
        if (argc > 0 && (strcmp(argv[argc-1],"--quiet") == 0 || strcmp(argv[argc-1],"-quiet") == 0)){quiet_mode = true;} //needs to be checked as soon as possible, disable all outputs if set
    #endif

    program_get_path(argv, program_path, program_name); //get current program path and filename
    if (getuid() != 0){program_usage(); return EXIT_FAILED_GENERIC;} //show help if not running as root

    if (argc > 2 && strcmp(argv[1],"-config") == 0){
        if (argv[2][0] == '/'){strcpy(config_path, argv[2]); //absolute path
        } else {sprintf(config_path, "%s/%s", program_path, argv[2]);} //relative path
    }

    if (strlen(config_path) == 0){sprintf(config_path, "%s/%s", program_path, cfg_filename);} //no custom path, convert default config relative to full path
    print_stderr("Config file: %s\n", config_path);

    #ifdef ALLOW_EXT_ADC //build config adc type description based on driver_adc_external.h
        strcat(adc_type_desc, ": ");
        for (int i=0; i<adc_type_count; i++){
            char buffer[strlen(adc_type_name[i])+6];
            sprintf(buffer, "%d:%s", i, adc_type_name[i]);
            if (i < adc_type_count-1){strcat(buffer, ", ");}
            strcat(adc_type_desc, buffer);
        }
    #endif

    #ifndef DIAG_PROGRAM
        bool input_searching = false;
        int input_search[2] = {-1, -1}; //input to search
    #endif

    int main_return = EXIT_SUCCESS; //program return
    bool cfg_no_create = false; //disable creation of config file
    bool program_close_on_warn = false; //close program on major warnings
    bool cfg_deleted = false; //program arg prohibe create of a config file but one was created

    //program arguments parse
    for(int i=1; i<argc; ++i){
        if (strcmp(argv[i],"-h") == 0 || strcmp(argv[i],"-help") == 0){program_usage(); return EXIT_SUCCESS;
#ifndef DIAG_PROGRAM
        } else if (strcmp(argv[i],"-confignocreate") == 0){cfg_no_create = true;
        } else if (strcmp(argv[i],"-configreset") == 0){return config_save(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, true); //reset config file
        } else if (strcmp(argv[i],"-configset") == 0){ //set custom config var
            if (++i<argc){return config_set(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, true, argv[i]);
            } else {
                print_stderr("FATAL: -configset defined with invalid argument, format: -configset VAR=VALUE\n");
                print_stderr("Run program with -h or -help for usage\n");
                return EXIT_FAILED_CONFIG;
            }
        } else if (strcmp(argv[i],"-configlist") == 0){config_list(cfg_vars, cfg_vars_arr_size); return 0; //list config vars
        } else if (strcmp(argv[i],"-noi2c") == 0){i2c_disabled = true; //disable i2c, pollrate, garbage data to uhid for benchmark
        } else if (strcmp(argv[i],"-nouhid") == 0){uhid_disabled = true; //disable irq, uhid for benchmark
        } else if (strcmp(argv[i],"-closeonwarn") == 0){program_close_on_warn = true;
        } else if (strcmp(argv[i],"-inputsearch") == 0){ //check if specific input pressed
            input_searching = true;
            for (int j=0; j<2; j++){
                int tmp_index = i + j + 1;
                if (tmp_index < argc){
                    if (strchr(argv[tmp_index], 'x') != NULL || strchr(argv[tmp_index], 'X') != NULL){sscanf(argv[tmp_index], "0x%X", &input_search[j]); //hex value
                    } else {input_search[j] = atoi(argv[tmp_index]);} //assume as int
                    input_search[j] = in_array_int16(mcu_input_map, input_search[j], input_registers_size); //replace button code by its index in mcu_input_map
                }
            }
            break;
        }
#else
        } else if (strcmp(argv[i],"-init") == 0){diag_first_run = true; //force first run mode
        } else if (strcmp(argv[i],"-noinputs") == 0){diag_noinputs = true; //disable mcu inputs in menu
        } else if (strcmp(argv[i],"-postmessagetest") == 0){diag_postmessagetest = true;} //only output "first run" post message and close
#endif
    }

    #ifndef MULTI_INSTANCES
        if (program_instances_count(program_path, program_name) > 1){ //check if program already running
            print_stderr("Program is already running\n");
            return EXIT_FAILED_ALREADY_RUN;
        }
    #endif

    //config
    //if (cfg_no_create){struct stat file_stat = {0}; if (stat(config_path, &file_stat) != 0){cfg_delete = true;}} //config file not exist
    int tmp_ret = config_parse(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid); //parse config file, create if needed
    if (tmp_ret < 0){print_stderr("failed to parse config file, errno:%d (%s)\n", -tmp_ret, strerror(-tmp_ret)); return EXIT_FAILED_CONFIG; //config parse failed
    } else if (tmp_ret == 1){ //config file created
        diag_first_run = true;
        if (cfg_no_create){remove(config_path); cfg_deleted = true;} //args prohibe creation of config file
    }
    
    //shm
    sprintf(shm_fullpath, "%s%s%d", shm_path, shm_path[strlen(shm_path)-1]=='/'?"":"/", uhid_device_id); //build shm path incl driver id
    char shm_status_path[strlen(shm_fullpath)+10]; sprintf(shm_status_path, "%s/status", shm_fullpath); shm_status_path_ptr = shm_status_path; //status file path
    shm_init(true); //init shm path and files

    //tty signal handling
    signal(SIGINT, tty_signal_handler); //ctrl-c
    signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle
    signal(SIGABRT, tty_signal_handler); //failure

    atexit(program_close); at_quick_exit(program_close); //run on program exit

/*
    #ifndef DIAG_PROGRAM
        driver_start:; //jump point
    #endif
*/

    //open i2c devices
    if (!i2c_disabled){
        bool mcu_failed = false;
        int tmp_bus = i2c_bus;
        if (i2c_check_bus(i2c_bus, mcu_search ? &tmp_bus : NULL) == 0){ //proper bus found
            if (tmp_bus >= 0 && i2c_bus != tmp_bus){ //bus number needs to be updated
                print_stderr("provided I2C bus corrected, was %d, now is %d\n", i2c_bus, tmp_bus);
                i2c_bus = tmp_bus;
                config_save(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, false); //save config to update bus
            }
        } else if (!diag_mode){return EXIT_FAILED_I2C;} //bus not found

        if (i2c_open_dev(&mcu_fd, i2c_bus, mcu_addr) != 0){mcu_failed = true; print_stderr("Failed to open MCU main address\n");} //main failed

        #ifdef ALLOW_MCU_SEC_I2C
            if (i2c_open_dev(&mcu_fd_sec, i2c_bus, mcu_addr_sec) != 0){mcu_failed = true; print_stderr("Failed to open MCU secondary address\n");} //secondary failed
        #endif

        if (mcu_failed && !diag_mode){
            int tmp_addr_main = 0,  tmp_addr_sec = 0;
            mcu_search_i2c_addr(i2c_bus, &tmp_addr_main, &tmp_addr_sec); //search for mcu address

            if (mcu_search){
                if (!io_fd_valid(mcu_fd) && tmp_addr_main != -1){
                    if (i2c_open_dev(&mcu_fd, i2c_bus, tmp_addr_main) == 0){print_stderr("WARNING, MCU main address set to 0x%02X, but 0x%02X found\n", mcu_addr, tmp_addr_main);}
                }

                #ifdef ALLOW_MCU_SEC_I2C
                    if (!io_fd_valid(mcu_fd_sec) && tmp_addr_sec != -1){
                        if (i2c_open_dev(&mcu_fd_sec, i2c_bus, tmp_addr_sec) == 0){print_stderr("WARNING, MCU secondary address set to 0x%02X, but 0x%02X found\n", mcu_addr_sec, tmp_addr_sec);}
                    }
                #endif
            }

            if (!io_fd_valid(mcu_fd)){
                diag_mode_init = true; //disable additionnal prints
                return EXIT_FAILED_I2C;
            }
        }

        print_stderr("MCU detected registers: adc_conf_bits:0x%02X, adc_res:0x%02X, config0:0x%02X\n", mcu_i2c_register_adc_conf, mcu_i2c_register_adc_res, mcu_i2c_register_config0);

        if (mcu_check_manufacturer() < 0 && !diag_mode){return EXIT_FAILED_MANUF;} //invalid manufacturer
        if (mcu_version_even > mcu_version && !diag_mode && program_close_on_warn){return EXIT_FAILED_VERSION;} //outdated mcu version
    }

    if (cfg_deleted && program_close_on_warn){return EXIT_FAILED_CONFIG;} //config file was created but prohibed by args, done in this order to allow mcu related error before missing config error

    if (!i2c_disabled){
        #ifndef DIAG_PROGRAM
            if (io_fd_valid(mcu_fd) && input_searching){ //check for specific mcu input
                if (i2c_smbus_read_i2c_block_data(mcu_fd, 0, input_registers_count, (uint8_t *)&i2c_joystick_registers) >= 0){ //read input register
                    int ret = 0;
                    uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0; //merge to ease work
                    if (input_search[0] >= 0){ret += ~(inputs >> input_search[0]) & 0b1;} //check button 0
                    if (input_search[1] >= 0){ret += (~(inputs >> input_search[1]) & 0b1)*2;} //check button 1
                    print_stderr("MCU input search return:%d\n", ret);
                    return ret;
                }
            }
        #endif

        #ifdef ALLOW_EXT_ADC
            for (int i=0; i<4; i++){ //external adcs device loop
                if (adc_params[i].enabled || diag_mode){adc_fd_valid[i] = (i2c_open_dev(&adc_fd[i], i2c_bus, adc_addr[i]) == 0);}
            }
        #endif

        if (mcu_update_config0() != 0 && !diag_mode){return EXIT_FAILED_MCU;} //read/update of config0 register failed
        if (init_adc() < 0 && !diag_mode){return EXIT_FAILED_MCU;} //init adc data failed

        #ifdef ALLOW_MCU_SEC_I2C
            //advanced features if mcu secondary address valid
            //if (io_fd_valid(mcu_fd_sec)){;}
        #endif
    }

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
            i2c_poll_rate_disable = true; /*irq_enable = false;*/
            print_stderr("running in a specific mode (no-i2c:%d, no-uhid:%d), pollrate disabled\n", i2c_disabled, uhid_disabled);
            if (irq_gpio >= 0){print_stderr("IRQ disabled\n");}
        }

        shm_init(false); //init shm i2c things
        
        //uhdi defs
        if (!uhid_disabled){ //don't create uhid device is -nouhid set
            uhid_fd = open(uhid_device_path, O_RDWR | O_CLOEXEC);
            if (!io_fd_valid(uhid_fd)){print_stderr("failed to open uhid device '%s', errno:%d (%s)\n", uhid_device_path, -uhid_fd, strerror(-uhid_fd)); return EXIT_FAILED_GENERIC;}
            print_stderr("uhid device '%s' opened\n", uhid_device_path);

            int ret = uhid_create(uhid_fd);
            if (ret < 0){return EXIT_FAILED_GENERIC;}
            print_stderr("new uhid device created\n");
        }

        //interrupt/low battery gpio
        #ifdef ALLOW_MCU_SEC_I2C
            battery_gpio_enable = (lowbattery_gpio >= 0);
        #endif

        if ((irq_gpio >= 0 && !i2c_disabled && !uhid_disabled) || battery_gpio_enable){
            irq_enable = battery_gpio_enable = false;
            #ifdef USE_WIRINGPI
                #define WIRINGPI_CODES 1 //allow error code return
                int err;
                if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
                    print_stderr("failed to initialize wiringPi, errno:%d\n", -err);
                } else {
                    if (irq_gpio >= 0){
                        pinMode(irq_gpio, INPUT);
                        print_stderr("using wiringPi to poll GPIO%d (IRQ))\n", irq_gpio);
                        irq_enable = true;
                    }

                    #ifdef ALLOW_MCU_SEC_I2C
                        if (lowbattery_gpio >= 0){
                            pinMode(lowbattery_gpio, INPUT);
                            print_stderr("using wiringPi to poll GPIO%d (LOW BATT)\n", lowbattery_gpio);
                            battery_gpio_enable = true;
                        }
                    #endif
                }
            #elif defined(USE_GPIOD)
                if ((gpiod_chip = gpiod_chip_open_lookup("0")) == NULL){
                    print_stderr("gpiod_chip_open_lookup failed\n");
                } else {
                    if (irq_gpio >= 0){
                        sprintf(gpiod_consumer_name, "%s %d IRQ", uhid_device_name, uhid_device_id);
                        if ((gpiod_input_line = gpiod_chip_get_line(gpiod_chip, irq_gpio)) == NULL){
                            print_stderr("gpiod_chip_get_line failed, pin:%d, consumer:'%s'\n", irq_gpio, gpiod_consumer_name);
                        } else if (gpiod_line_request_both_edges_events(gpiod_input_line, gpiod_consumer_name) < 0){
                            print_stderr("gpiod_line_request_both_edges_events failed. chip:%s(%s), consumer:'%s'\n", gpiod_chip_name(gpiod_chip), gpiod_chip_label(gpiod_chip), gpiod_consumer_name);
                        } else if ((gpiod_fd = gpiod_line_event_get_fd(gpiod_input_line)) < 0){
                            print_stderr("gpiod_line_event_get_fd failed. errno:%d, consumer:'%s'\n", -gpiod_fd, gpiod_consumer_name);
                        } else {
                            fcntl(gpiod_fd, F_SETFL, fcntl(gpiod_fd, F_GETFL, 0) | O_NONBLOCK); //set gpiod fd to non blocking
                            print_stderr("using libGPIOd to poll GPIO%d, chip:%s(%s), consumer:'%s'\n", irq_gpio, gpiod_chip_name(gpiod_chip), gpiod_chip_label(gpiod_chip), gpiod_consumer_name);
                            irq_enable = true;
                        }
                    }

                    #ifdef ALLOW_MCU_SEC_I2C
                        if (lowbattery_gpio >= 0){
                            sprintf(gpiod_consumer_lowbatt_name, "%s %d Lbatt", uhid_device_name, uhid_device_id);
                            if ((gpiod_lowbatt_line = gpiod_chip_get_line(gpiod_chip, lowbattery_gpio)) == NULL){
                                print_stderr("gpiod_chip_get_line failed, pin:%d, consumer:'%s'\n", lowbattery_gpio, gpiod_consumer_lowbatt_name);
                            } else if (gpiod_line_request_both_edges_events(gpiod_lowbatt_line, gpiod_consumer_lowbatt_name) < 0){
                                print_stderr("gpiod_line_request_both_edges_events failed. chip:%s(%s), consumer:'%s'\n", gpiod_chip_name(gpiod_chip), gpiod_chip_label(gpiod_chip), gpiod_consumer_lowbatt_name);
                            } else if ((gpiod_lowbatt_fd = gpiod_line_event_get_fd(gpiod_lowbatt_line)) < 0){
                                print_stderr("gpiod_line_event_get_fd failed. errno:%d, consumer:'%s'\n", -gpiod_lowbatt_fd, gpiod_consumer_lowbatt_name);
                            } else {
                                fcntl(gpiod_lowbatt_fd, F_SETFL, fcntl(gpiod_lowbatt_fd, F_GETFL, 0) | O_NONBLOCK); //set gpiod fd to non blocking
                                print_stderr("using libGPIOd to poll GPIO%d, chip:%s(%s), consumer:'%s'\n", lowbattery_gpio, gpiod_chip_name(gpiod_chip), gpiod_chip_label(gpiod_chip), gpiod_consumer_lowbatt_name);
                                battery_gpio_enable = true;
                            }
                        }
                    #endif
                }
            #endif
        }
    #endif

    //initial poll
    if (i2c_poll_rate < 1 || debug_adv){i2c_poll_rate_disable = true; i2c_poll_rate = 0;} else {i2c_poll_rate_time = 1. / i2c_poll_rate;} //disable pollrate limit, poll interval in sec
    if (i2c_adc_poll < 1){i2c_adc_poll = 1;} i2c_adc_poll_loop = i2c_adc_poll;

    i2c_poll_joystick(true);

    #ifndef DIAG_PROGRAM //driver program
/*
        if (allow_diag_run){ //redirect to diag program if needed
            bool diag_run = false;
            if ((gamepad_report.buttons15to8 >> 3) & 0b1){diag_run = true;} //start button pressed -> diag
            if ((gamepad_report.buttons15to8 >> 2) & 0b1){diag_first_run = true;} //select button pressed -> diag "first run" mode
            if (diag_run || diag_first_run){
                char diag_program_path[strlen(program_path) + strlen(diag_program_filename) + 2];
                sprintf(diag_program_path, "%s/%s", program_path, diag_program_filename); //build full path to diag program
                struct stat file_stat = {0};
                if (stat(diag_program_path, &file_stat) == 0){
                    program_close(); //cleanup as driver will stay on until diag program closes
                    print_stderr("starting setup/diagnostic program\n");

                    //build program command
                    char* cmd_format = "exec %s %s <> %s >&0 2>&1"; //sudo setsid sh -c 'exec %s %s <> %s >&0 2>&1'
                    char* tty_curr = (ttyname(STDIN_FILENO)!=NULL) ? ttyname(STDIN_FILENO):"/dev/tty1"; //redirect to pts0 if get tty failed, mainly because running as a daemon or from rc.local
                    char diag_command[strlen(cmd_format) + strlen(diag_program_path) + strlen(tty_curr) + strlen (diag_first_run_command) + 2];
                    sprintf(diag_command, cmd_format, diag_program_path, diag_first_run ? "-init" : "", tty_curr);

                    int tmp_ret = system(diag_command); //start diag program
                    
                    if (tmp_ret != 0){print_stderr("something went wrong, errno:%d (%s)\n", tmp_ret, strerror(tmp_ret));}
                } else {print_stderr("can't start setup/diagnostic, program is missing:\n%s\n", diag_program_path);}
                allow_diag_run = diag_first_run = false;
                goto driver_start;
            }
        }
*/
        if(!quiet_mode){
            fprintf(stdout, "Press '^C' to gracefully close driver\n");
    
            if (!i2c_poll_rate_disable){
                print_stderr("pollrate: digital:");
                if (irq_enable){fprintf(stderr, "interrupt");} else {fprintf(stderr, "%dhz", i2c_poll_rate);}
                fprintf(stderr, ", adc:%dhz\n", i2c_poll_rate / i2c_adc_poll);
            } else {print_stderr("poll speed not limited\n");}
        }
        
        bool driver_lock_back = driver_lock;
        if (shm_enable){print_stderr("driver can be put in lock state by setting %s to 2, event report will be disabled, restore by setting to 1\n", shm_status_path_ptr);}

        driver_config_reload_jmp:; //jump point after config reload
        struct stat config_stat = {0}; if (stat(config_path, &config_stat) == 0){config_mtime = config_stat.st_mtim;} //backup config file mtime

        if (config_reload){
            if (mcu_update_config0() != 0 || init_adc() < 0){print_stderr("critical failure after configuration reload\n"); return EXIT_FAILED_MCU;} //read/update of config0 register or init adc data failed
            adc_firstrun = true; i2c_poll_joystick(true);
            print_stderr("new configuration reloaded\n"); config_reload = false;
        }

        while (!kill_requested){
            if (shm_enable && driver_lock != driver_lock_back){
                driver_lock_back = driver_lock;
                if (driver_lock){ //driver enter "lock" state
                    struct gamepad_report_t null_report = {0,0,0,0,0x7FFF,0x7FFF,0x7FFF,0x7FFF}; gamepad_report = null_report; uhid_send_event(uhid_fd); //no input, centered adc report
                    print_stderr("driver entered lock state, no report will happen until %s to back to 1\n", shm_status_path_ptr);
                } else {print_stderr("driver lock state reset\n");}
            }

            if (!driver_lock){
                i2c_poll_joystick(false); //poll and report if driver not locked

                //check config update
                if (config_check_start < -0.1){config_check_start = poll_clock_start;} //interval reset
                if (poll_clock_start - config_check_start > config_check_interval){
                    if (stat(config_path, &config_stat) == 0 && memcmp(&config_mtime, &config_stat.st_mtim, sizeof(struct timespec)) != 0){ //file modif time changed
                        config_mtime = config_stat.st_mtim;
                        kill_requested = config_reload = true;
                    }
                    config_check_start = -1.;
                }
            } else {poll_clock_start = get_time_double();} //poll start time to avoid locking related thing
            
            #ifdef ALLOW_MCU_SEC_I2C
                //battery
                if (poll_clock_start - battery_clock_start > (double)battery_interval){
                    mcu_update_battery_capacity(); //update rsoc <> register if possible
                    mcu_update_power_control(); //low_batt gpio
                    battery_clock_start = poll_clock_start;
                }
            #endif

            //shm
            if (shm_clock_start < -0.1){shm_clock_start = poll_clock_start;} //interval reset
            if (poll_clock_start - shm_clock_start > shm_update_interval){shm_update(); shm_clock_start = -1.;} //shm update

            if (kill_requested){break;}

            if (!i2c_poll_rate_disable){ //pollrate
                if (i2c_poll_duration > i2c_poll_duration_warn){print_stderr ("WARNING: extremely long loop duration: %dms\n", (int)(i2c_poll_duration*1000));}
                if (i2c_poll_duration < 0){i2c_poll_duration = 0;} //hum, how???
                if (i2c_poll_duration < i2c_poll_rate_time){usleep((useconds_t) ((double)(i2c_poll_rate_time - i2c_poll_duration) * 1000000));} //need to sleep to match poll rate. note: doesn't account for uhid_write_duration as uhid write duration way faster than i2c transaction
            }

            if (i2c_poll_rate_disable && debug_adv){ //benchmark mode
                if (poll_benchmark_clock_start < -0.1){poll_benchmark_clock_start = poll_clock_start;} //interval reset
                poll_benchmark_loop++;
                if ((poll_clock_start - poll_benchmark_clock_start) > 2.){ //report every seconds
                    print_stderr("poll loops per sec (2secs samples) : %ld\n", poll_benchmark_loop / 2);
                    poll_benchmark_loop = 0; poll_benchmark_clock_start = -1.;
                }
            }
        }

        if (config_reload){ //config file changed
            kill_requested = false;
            print_stderr("config file changed, reloading it\n");
            if (config_parse(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid) < 0){print_stderr("failed to parse new config file\n"); //parse/create config file failed
            } else {print_stderr("new config file reloaded\n"); goto driver_config_reload_jmp;}
        }

        if (i2c_last_error != 0) {print_stderr("last detected I2C error: %d (%s)\n", -i2c_last_error, strerror(i2c_last_error));}

        if (adc_params[0].enabled || adc_params[1].enabled || adc_params[2].enabled || adc_params[3].enabled){
            print_stderr("Detected ADC limits:\n");
            logs_write("Detected ADC limits:\n");
            for (uint8_t i=0; i<4; i++){
                if (adc_params[i].raw_min != INT_MAX){
                    print_stderr("ADC%d: min:%d max:%d\n", i, adc_params[i].raw_min, adc_params[i].raw_max);
                    logs_write("-ADC%d: min:%d, max:%d\n", i, adc_params[i].raw_min, adc_params[i].raw_max);
                }
            }
        }

        #ifdef USE_GPIOD
            if (irq_enable){gpiod_line_release(gpiod_input_line);}
            #ifdef ALLOW_MCU_SEC_I2C
                if (battery_gpio_enable){gpiod_line_release(gpiod_lowbatt_line);}
            #endif
        #endif
    #else //diag program
        struct stat status_stat = {0}; char status_buffer[3];

        if (stat(shm_status_path_ptr, &status_stat) == 0){ //lock driver
            file_read(shm_status_path_ptr, status_buffer, 2);
            if (status_buffer != NULL && atoi(status_buffer) != 0 && file_write(shm_status_path_ptr, "2") == 0){print_stderr("driver locked\n");}
        }

        i2c_poll_rate_disable = true;
        program_diag_mode(); //diagnostic mode

        if (stat(shm_status_path_ptr, &status_stat) == 0){ //unlock driver
            file_read(shm_status_path_ptr, status_buffer, 2);
            if (status_buffer != NULL && atoi(status_buffer) == 2 && file_write(shm_status_path_ptr, "1") == 0){print_stderr("driver unlocked\n");}
        }
    #endif

    return main_return;
}
