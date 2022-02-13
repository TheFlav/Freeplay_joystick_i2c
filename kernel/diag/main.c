


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
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#include <termios.h>


#define XSTR(s) STR(s)
#define STR(s) #s


//program related
const char programversion[] = "0.1a"; //program version
bool kill_resquested = false; //program kill requested, stop main loop in a smooth way
#define buffer_size 1024 //char array buffer size


//debug related
bool debug = true; //debug outputs

static void debug_print_binary_int_term(int line, int col, int val, int bits, char* var){ //print given var in binary format at given term position
	printf("\e[%d;%dH\e[1;100m%s : ", line, col, var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\e[0m");
}

//DToverlay related
typedef struct dtoverlay_driver_struct { //converted from freeplay-joystick-overlay.dts
    int bus, addr; //i2c bus, address //TODO UPDATE DTOVERLAY STRUCT
    int interrupt; //interrupt pin
    //int analogsticks; //TODO UPDATE DTOVERLAY STRUCT, analog stricks count, 0:none, 1:left, 2:left-right
    int digitalbuttons; //digital buttons count
    bool dpads; //dpad enabled

    char joy0_x_params[buffer_size], joy0_y_params[buffer_size]; //joystick0 config: min,max,fuzz,flat,inverted. default:"0 0xFFF 32 300 0"
    int joy0_x_min, joy0_x_max, joy0_x_fuzz, joy0_x_flat; bool joy0_x_inverted;
    int joy0_y_min, joy0_y_max, joy0_y_fuzz, joy0_y_flat; bool joy0_y_inverted;
    bool joy0_swapped_x_y; //xy swapped. default:0
    bool joy0_enabled; //TODO UPDATE DTOVERLAY STRUCT

    char joy1_x_params[buffer_size], joy1_y_params[buffer_size]; //joystick1 config: min,max,fuzz,flat,inverted. default:"0 0xFFF 32 300 0"
    int joy1_x_min, joy1_x_max, joy1_x_fuzz, joy1_x_flat; bool joy1_x_inverted;
    int joy1_y_min, joy1_y_max, joy1_y_fuzz, joy1_y_flat; bool joy1_y_inverted;
    bool joy1_swapped_x_y; //xy swapped. default:0
    bool joy1_enabled; //TODO UPDATE DTOVERLAY STRUCT
} dtoverlay_driver_t;

dtoverlay_driver_t driver_user = {0}, driver_back = {0}, driver_default = { //dtoverlay default settings
    .bus=1, .addr=0x30, .interrupt=40, /*.analogsticks=0,*/ .digitalbuttons=11, .dpads=false,
    .joy0_x_min=0, .joy0_x_max=0xFFF, .joy0_x_fuzz=32, .joy0_x_flat=300, .joy0_x_inverted=false,
    .joy0_y_min=0, .joy0_y_max=0xFFF, .joy0_y_fuzz=32, .joy0_y_flat=300, .joy0_y_inverted=false,
    .joy0_swapped_x_y=false, .joy0_enabled=false,
    .joy1_x_min=0, .joy1_x_max=0xFFF, .joy1_x_fuzz=32, .joy1_x_flat=300, .joy1_x_inverted=false,
    .joy1_y_min=0, .joy1_y_max=0xFFF, .joy1_y_fuzz=32, .joy1_y_flat=300, .joy1_y_inverted=false,
    .joy1_swapped_x_y=false, .joy1_enabled=false,
};




//I2C/ADC related
const uint8_t i2c_dev_manuf = 0xED; //MCU manufacturer signature, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
uint8_t i2c_dev_sig = 0, i2c_dev_id = 0, i2c_dev_minor = 0;
struct i2c_joystick_register_struct { //mcu main registers, direct copy from ino file
    uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
    uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
    uint8_t input2;          // Reg: 0x03 - INPUT port 2 (extended digital buttons)     BTN_Z and BTN_C among other things
    uint8_t a0_msb;          // Reg: 0x04 - ADC0 most significant 8 bits
    uint8_t a1_msb;          // Reg: 0x05 - ADC1 most significant 8 bits
    uint8_t a1a0_lsb;        // Reg: 0x06 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
    uint8_t a2_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
    uint8_t a3_msb;          // Reg: 0x08 - ADC2 most significant 8 bits
    uint8_t a3a2_lsb;        // Reg: 0x09 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
    uint8_t adc_conf_bits;   // Reg: 0x09 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                             //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                             //             (but can only turn ON ADCs that are turned on in the high nibble.)
    uint8_t config0;         // Reg: 0x0A - config register (turn on/off PB4 resistor ladder)  //maybe allow PA4-7 to be digital inputs connected to input2  config0[7]=use_extended_inputs
    uint8_t adc_res;         // Reg: 0x0B - current ADC resolution (maybe settable?)
    uint8_t rfu0;            // Reg: 0x0C - reserved for future use (or device-specific use)
    uint8_t manuf_ID;        // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
    uint8_t device_ID;       // Reg: 0x0E -
    uint8_t version_ID;      // Reg: 0x0F - 
} volatile i2c_joystick_registers;

int i2c_fd = -1, i2c_bus_prev = 0, i2c_addr_prev = 0;
int i2c_mcu_register_js0 = offsetof(struct i2c_joystick_register_struct, a0_msb) / sizeof(i2c_joystick_registers.a0_msb);
int i2c_mcu_register_js1 = offsetof(struct i2c_joystick_register_struct, a2_msb) / sizeof(i2c_joystick_registers.a2_msb);
int i2c_mcu_register_adc_conf = offsetof(struct i2c_joystick_register_struct, adc_conf_bits) / sizeof(i2c_joystick_registers.adc_conf_bits);
int i2c_mcu_register_adc_res = offsetof(struct i2c_joystick_register_struct, adc_res) / sizeof(i2c_joystick_registers.adc_res);
bool i2c_failed_bus = true, i2c_failed_dev = true, i2c_failed_sig = true, i2c_failed = true;
int button_count_limit = 19;

typedef union {
    struct {uint8_t use0:1, use1:1, use2:1, use3:1, en0:1, en1:1, en2:1, en3:1;} vals;
    uint8_t bits;
} mcu_adc_conf_t;
mcu_adc_conf_t mcu_conf_current;
bool adc_reg_enable[4]={0};
bool adc_reg_used[4]={0}, adc_reg_used_prev[4]={0}/*, adc_reg_used_backup[4]={0}*/;
bool adc_use_raw_min[4] = {0}, adc_use_raw_max[4] = {0};
int adc_res = 10, adc_res_limit = 1023;

typedef struct adc_data_struct_t {
    char name[32]; //name of current axis
	int raw, raw_prev, raw_min, raw_max; //store raw values for min-max report
	int offset; //center offset, compute during runtime
	int value; //current value
} adc_data_t;

adc_data_t adc_data[4] = {
    {"X1", -1,-1,INT_MAX,INT_MIN, 0, 0x7FFF},
    {"Y1", -1,-1,INT_MAX,INT_MIN, 0, 0x7FFF},
    {"X2", -1,-1,INT_MAX,INT_MIN, 0, 0x7FFF},
    {"Y2", -1,-1,INT_MAX,INT_MIN, 0, 0x7FFF},
};

typedef struct adc_settings_struct_t {
	int *min, *max; //current value, min/max limits
	int *fuzz, *flat; //fuzz, flat
	bool *reversed; //reverse reading
} adc_settings_t;

adc_settings_t adc_settings[4] = {
    {&driver_user.joy0_x_min, &driver_user.joy0_x_max, &driver_user.joy0_x_fuzz, &driver_user.joy0_x_flat, &driver_user.joy0_x_inverted},
    {&driver_user.joy0_y_min, &driver_user.joy0_y_max, &driver_user.joy0_y_fuzz, &driver_user.joy0_y_flat, &driver_user.joy0_y_inverted},
    {&driver_user.joy1_x_min, &driver_user.joy1_x_max, &driver_user.joy1_x_fuzz, &driver_user.joy1_x_flat, &driver_user.joy1_x_inverted},
    {&driver_user.joy1_y_min, &driver_user.joy1_y_max, &driver_user.joy1_y_fuzz, &driver_user.joy1_y_flat, &driver_user.joy1_y_inverted},
};
bool *adc_axis_swap[2] = {&driver_user.joy0_swapped_x_y, &driver_user.joy1_swapped_x_y};
bool *js_enabled[2] = {&driver_user.joy0_enabled, &driver_user.joy1_enabled}, js_enabled_prev[2]={0};

adc_settings_t adc_settings_back[4] = {
    {&driver_back.joy0_x_min, &driver_back.joy0_x_max, &driver_back.joy0_x_fuzz, &driver_back.joy0_x_flat, &driver_back.joy0_x_inverted},
    {&driver_back.joy0_y_min, &driver_back.joy0_y_max, &driver_back.joy0_y_fuzz, &driver_back.joy0_y_flat, &driver_back.joy0_y_inverted},
    {&driver_back.joy1_x_min, &driver_back.joy1_x_max, &driver_back.joy1_x_fuzz, &driver_back.joy1_x_flat, &driver_back.joy1_x_inverted},
    {&driver_back.joy1_y_min, &driver_back.joy1_y_max, &driver_back.joy1_y_fuzz, &driver_back.joy1_y_flat, &driver_back.joy1_y_inverted},
};
bool *adc_axis_swap_backup[2] = {&driver_back.joy0_swapped_x_y, &driver_back.joy1_swapped_x_y};
bool *js_enabled_backup[2] = {&driver_back.joy0_enabled, &driver_back.joy1_enabled};

adc_settings_t adc_settings_default[4] = {
    {&driver_default.joy0_x_min, &driver_default.joy0_x_max, &driver_default.joy0_x_fuzz, &driver_default.joy0_x_flat, &driver_default.joy0_x_inverted},
    {&driver_default.joy0_y_min, &driver_default.joy0_y_max, &driver_default.joy0_y_fuzz, &driver_default.joy0_y_flat, &driver_default.joy0_y_inverted},
    {&driver_default.joy1_x_min, &driver_default.joy1_x_max, &driver_default.joy1_x_fuzz, &driver_default.joy1_x_flat, &driver_default.joy1_x_inverted},
    {&driver_default.joy1_y_min, &driver_default.joy1_y_max, &driver_default.joy1_y_fuzz, &driver_default.joy1_y_flat, &driver_default.joy1_y_inverted},
};
bool *adc_axis_swap_default[2] = {&driver_default.joy0_swapped_x_y, &driver_default.joy1_swapped_x_y};
bool *js_enabled_default[2] = {&driver_default.joy0_enabled, &driver_default.joy1_enabled};


static int i2c_init(int* fd, int bus, int addr){
    i2c_failed_bus = true; i2c_failed_dev = true; i2c_failed_sig = true; i2c_failed = true;

    if (*fd){close(*fd);} //close already opened fd
    if (bus < 0 || addr < 0 || addr > 127){return -1;}

    char fd_path[13]; sprintf(fd_path, "/dev/i2c-%d", bus);
    *fd = open(fd_path, O_RDWR); if (*fd < 0){return -1;} else {i2c_failed_bus = false;}
    if (ioctl(*fd, I2C_SLAVE_FORCE, addr) < 0){close(*fd); return -1;}
    if (i2c_smbus_read_byte_data(*fd, 0) < 0){close(*fd); return -1;} else {i2c_failed_dev = false;}

    int ret = i2c_smbus_read_byte_data(*fd, offsetof(struct i2c_joystick_register_struct, manuf_ID) / sizeof(i2c_joystick_registers.manuf_ID)); //check signature
    i2c_dev_sig = (uint8_t)ret; if (i2c_dev_sig != i2c_dev_manuf){close(*fd); return -1;} else {i2c_failed_sig = false;}

    ret = i2c_smbus_read_word_data(*fd, offsetof(struct i2c_joystick_register_struct, device_ID) / sizeof(i2c_joystick_registers.device_ID)); //get version
    if (ret < 0){close(*fd); return -1;} else {i2c_dev_id = ret & 0xFF; i2c_dev_minor = (ret >> 8) & 0xFF;} //device id, version

    ret = i2c_smbus_read_byte_data(*fd, i2c_mcu_register_adc_res); //current ADC resolution
    if (ret < 0){close(*fd); return -1;
    } else {
        adc_res = ret; adc_res_limit = 0xFFFFFFFF >> (32 - adc_res);

        ret = i2c_smbus_read_byte_data(*fd, i2c_mcu_register_adc_conf); //current analog config
        if (ret < 0){close(*fd); return -1;
        } else {
            mcu_conf_current.bits = (uint8_t)ret;
            for (uint8_t i=0; i<4; i++){
                if(*adc_settings[i].max > adc_res_limit){*adc_settings[i].max = adc_res_limit;}
                if(*adc_settings_back[i].max > adc_res_limit){*adc_settings_back[i].max = adc_res_limit;}
                if(*adc_settings_default[i].max > adc_res_limit){*adc_settings_default[i].max = adc_res_limit;}

                adc_data[i].raw_min = adc_res_limit; adc_data[i].raw_max = 0;

                adc_reg_enable[i] = (bool)((mcu_conf_current.bits >> i+4) & 0b1);
                if (adc_reg_enable[i]){adc_reg_used[i] = adc_reg_used_prev[i] = (bool)((mcu_conf_current.bits >> i) & 0b1);}
            }
            if (!(adc_reg_used[0] && adc_reg_used[1])){*js_enabled[0] = false;} if (!(adc_reg_used[2] && adc_reg_used[3])){*js_enabled[1] = false;}
            js_enabled_prev[0] = *js_enabled[0]; js_enabled_prev[1] = *js_enabled[1];
            i2c_failed = false;
            return 0;
        }
    }
    return -1;
}

static int adc_defuzz(int value, int old_val, int fuzz){ //defuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56
	if (fuzz) {
		if (value > old_val - fuzz / 2 && value < old_val + fuzz / 2){return old_val;}
		if (value > old_val - fuzz && value < old_val + fuzz){return (old_val * 3 + value) / 4;}
		if (value > old_val - fuzz * 2 && value < old_val + fuzz * 2){return (old_val + value) / 2;}
	}
	return value;
}

static int adc_correct_offset_center(int adc_resolution, int adc_value, int adc_min, int adc_max, int adc_offset, int flat_in, int flat_out){ //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent), from uhid driver
	int max = adc_resolution / 2, offset = max + adc_offset;
	adc_value -= offset; //center adc value to 0

	int dir = 1, limit;
	if (adc_value < 0){dir = -1; adc_value *= -1; limit = (adc_min - offset) * -1; //convert adc value to positive
	} else {limit = adc_max - offset;}
    if(limit==0){limit=1;}

	adc_value = adc_value * (max + flat_out) / limit; //convert to 0->adc_min/adc_max to 0->adc_resolution+(outside flat)
	if (flat_in > 0){adc_value = (adc_value - flat_in) * (max + flat_out) / ((max + flat_out) - flat_in);} //convert to (-inside flat)->adc_resolution+(outside flat) to 0->adc_resolution+(outside flat)

	if(adc_value < 0){adc_value = 0;} adc_value *= dir; adc_value += max; //reapply proper direction and recenter value to adc_resolution/2
	if(adc_value < 0){return 0;} else if (adc_value > adc_resolution){return adc_resolution;} //limit return from 0 to adc_resolution
    
	return adc_value;
}


//time related
double program_start_time = 0.; //program start time

static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return -1.; //failed
}


//print related
#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr


//array manipulation related
void array_fill(char* arr, int size, char chr){for (int i=0;i<size;i++){arr[i]=chr;} arr[size]='\0';} //fill array with given character, works with '\0' for full reset, last char set to '\0'

int array_pad(char* arr, int arr_len, int size, char pad, int align){ //pad a array with 'pad', 'align': 0:center 1:left 2:right, 'size':final array size, return padding length
    if (size < arr_len || pad == '\0' || arr[0] == '\0'){return 0;} //no padding to do
    char arr_backup[arr_len+1]; strcpy(arr_backup, arr); //backup original array
    
    int pad_len = size - arr_len; //padding length
    if (align==0){if (pad_len % 2 != 0){pad_len++;} pad_len /= 2;} //align to center, do necessary to have equal char count on both side

    char pad_buffer[pad_len + 1]; array_fill(pad_buffer, pad_len, pad); //generate padding array

    if (align != 1){
        array_fill(arr, arr_len, '\0'); //fully reset original array
        strcpy(arr, pad_buffer); strcat(arr, arr_backup);
        if (align == 0 && size-arr_len-pad_len >= 0){pad_buffer[size - arr_len - pad_len] = '\0'; strcat(arr, pad_buffer);}
    } else {strcat(arr, pad_buffer);}
    return pad_len;
}

static int strcpy_noescape(char *dest, char *src, int limit){ //strcpy "clone" that partialy ignore escape code, dest=src or dest=NULL to only return length, limit:defined limit of escape code (w/o "\e["). warnings: no size check, broken if badly formated escape, only check for h,l,j,m ending
    if(!src){return 0;}
    int ret = 0; char *ptrdest = NULL, *ptrsrc = src;
    if (!dest || dest != src){ptrdest = dest;} //valid dest pointer
    while (*ptrsrc != '\0'){
        if (*ptrsrc == '\e' && *++ptrsrc == '['){ //escape code begin
            char* ptrsrc_back = ++ptrsrc; bool ending = false; //backup start position, ending found?
            for (int i=0; i<limit; i++){
                char tmpchar = *ptrsrc; ptrsrc++;
                if (tmpchar == 'm' || tmpchar == 'h' || tmpchar == 'l' || tmpchar == 'j' || tmpchar == '\0'){ending = true; break;}
            }
            if (!ending){ptrsrc = ptrsrc_back;} //escape code failed before "limit"
        }
        if (*ptrsrc == '\0'){break;}
        if (!ptrdest){ptrsrc++;} else {*ptrdest++ = *ptrsrc++;}
        ret++;
    }
    return ret;
}

static void int_rollover(int* val, int min, int max){if(*val < min){*val = max;} else if(*val > max){*val = min;}}


//terminal related
struct winsize ws; //terminal size
struct termios term_backup; //original terminal state backup

bool save_boot_resquested = false, save_text_resquested = false, default_resquested = false, reset_resquested = false; //footer buttons bools

const int term_esc_col_normal = 97; //terminal normal color escape code
const int term_esc_col_disabled = 90; //terminal disabled color escape code
const int term_esc_col_error = 91; //terminal error color escape code

const int term_adc_width = 30; //ADC section width
const int term_adc_vertspacing = 9; //vertical spacing between each horizontal ADC elements

const int term_footer_buttons_width = 15; //footer button width

#define term_selectible_count 255 //absolute limit to selectible elements
int select_index_current = 0, select_index_last = -1, select_limit = 0; //current element selected, last selected
typedef struct term_select_struct { //selectible terminal elements data
    struct {int x, y, size;} position; //position pointers
    int type; //0:int, 1:bool, 2:bool(visual toogle), 3:hex
    struct {int min, max; char *ptrchar; bool *ptrbool; int *ptrint; bool force_update;} value;
    struct {int y; char *str;} hint; //hint data
    struct {int y; bool *ptrbool; int *ptrint;} defval; //default data
    bool disabled;
} term_select_t;
term_select_t term_select[term_selectible_count] = {0};

typedef struct term_input_struct {bool up, down, left, right, plus, minus, tab, enter, escape;} term_input_t;
term_input_t term_input = {0};

typedef struct term_pos_generic_struct {int x, y, w;} term_pos_generic_t;
typedef struct term_pos_string_struct {int x, y, w; char str[buffer_size];} term_pos_string_t;

char* term_hint_i2c_failed[]={ //follow i2c_failed_bus, i2c_failed_dev, i2c_failed_sig, i2c_failed bool order
    "Invalid I2C bus\0",
    "Invalid I2C address\0",
    "Invalid I2C device signature\0",
    "I2C reading failed\0",
};

char* term_hint_generic[]={
    "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m or \e[1m(^)(v)\e[0m to navigate\0",
    "Press \e[1m[LEFT]\e[0m,\e[1m[RIGHT]\e[0m or \e[1m(<)(>)\e[0m to change value, \e[1m[-]\e[0m,\e[1m[+]\e[0m for big increment\0",
    "Save new configuration to \e[1m/boot/config.txt\e[0m\0",
    "Save new configuration to \e[1m*program_path*/config.txt\e[0m\0",
    "Reset values to default\0",
    "Discard current modifications\0",
    "Return to main screen\0",
    "Close without saving\0",
};

char* term_hint_main[]={
    "Bus to use, change with caution\0",
    "Address of the device\0",
    "GPIO pin used for interrupt, -1 to disable\0",
    "Amount of reported digital buttons (excl Dpad)\0",
    "Enable/disable Dpad report\0",
    "Change enable ADCs, limits, fuzz, flat values\0",
};

char* term_hint_adc[]={
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to enable or disable\0",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to switch axis direction\0",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to set as MIN limit value\0",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to set as MAX limit value\0",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to enable axis swap, apply only on driver\0",
};


static void term_user_input(term_input_t* input){ //process terminal key inputs
    char term_read_char; char last_key[100] = {'\0'}; //debug, last char used
    memset(input, 0, sizeof(term_input_t)); //reset input struct
    if (read(STDIN_FILENO, &term_read_char, 1) > 0){
        if (term_read_char == '\t'){if(debug){strcpy(last_key, "TAB");} input->tab = true;}
        else if (term_read_char == '\n'){if(debug){strcpy(last_key, "ENTER");} input->enter = true;}
        else if (term_read_char == '-'){if(debug){strcpy(last_key, "MINUS");} input->minus = true;}
        else if (term_read_char == '+'){if(debug){strcpy(last_key, "PLUS");} input->plus = true;}
        else if (term_read_char == '\e'){ //escape
            if (read(STDIN_FILENO, &term_read_char, 1) > 0){
                if (term_read_char == '[' && read(STDIN_FILENO, &term_read_char, 1) > 0){ //escape sequence
                    if (term_read_char == 'A'){if(debug){strcpy(last_key, "UP");} input->up = true;} //up key
                    else if (term_read_char == 'B'){if(debug){strcpy(last_key, "DOWN");} input->down = true;} //down key
                    else if (term_read_char == 'D'){if(debug){strcpy(last_key, "LEFT");} input->left = true;} //left key
                    else if (term_read_char == 'C'){if(debug){strcpy(last_key, "RIGHT");} input->right = true;} //right key
                }
            } else {if(debug){strcpy(last_key, "ESC");} input->escape = true;} //esc key
        } else if (debug){sprintf(last_key, "'%c'(%d), no used", term_read_char, term_read_char);} //debug
        tcflush(STDIN_FILENO, TCIOFLUSH); //flush STDIN, useful?
        if (debug){fprintf(stdout, "\e[1;25H\e[0K\e[100mDEBUG last key: %s\e[0m\n", last_key);} //print last char to STDIN if debug
    }
}

static void term_select_update(term_select_t* store, int* index, int* index_last, int index_limit, term_input_t* input, int tty_width, int tty_height){ //update selectible elements
    bool update = false;
    if (input->up){(*index)--;} else if (input->down || input->tab){(*index)++;}
    int limit = index_limit-1; int_rollover(index, 0, limit); 
    if (*index != *index_last){ //selected index changed
        int_rollover(index_last, 0, limit);
        for (int i=0; i < 10; i++){ //deal with shifting or disabled elements
            bool valid = store[*index].position.size && !store[*index].disabled;
            int type = store[*index].type;
            if (valid){ //size set, not disabled
                if (store[*index].value.ptrbool && (type == 1 || type == 2)){ //bool prt and proper type (1,2)
                } else if (store[*index].value.ptrint && (type == 0 || type == 3)){ //int prt and proper type (0,3)
                } else {valid = false;}
            }
            if (valid){update = true; break;
            } else {if (input->up){(*index)--;} else {(*index)++;} int_rollover(index, 0, limit);}
        }

        if (update){
            //default, min, max values
            if (store[*index].defval.y > 0 && store[*index].position.size){
                char buffer[buffer_size] = {0}, buffer1[buffer_size] = {0}, buffer2[buffer_size] = {0};
                int type = store[*index].type;
                
                if (store[*index].defval.ptrbool && (type == 1 || type == 2)){ //bool
                    if (type == 1){strcpy(buffer1, *(store[*index].defval.ptrbool)?"Enabled":"Disabled"); //bool
                    } else if (type == 2){strcpy(buffer1, *(store[*index].defval.ptrbool)?"X":" ");} //toogle
                } else if (store[*index].defval.ptrint && (type == 0 || type == 3)){ //int
                    if (type == 0){sprintf(buffer1, "%d", *store[*index].defval.ptrint); //int
                    } else if (type == 3){sprintf(buffer1, "0x%X", *store[*index].defval.ptrint);} //hex
                }

                if (strlen(buffer1)){sprintf(buffer, "Default:\e[1;4m%s\e[0m", buffer1);}

                if (store[*index].value.min != store[*index].value.max && (type == 0 || type == 3)){
                    if (type == 0){sprintf(buffer1, "min:\e[1;4m%d\e[0m, max:\e[1;4m%d\e[0m", store[*index].value.min, store[*index].value.max); //int
                    } else if (type == 3){sprintf(buffer1, "min:\e[1;4m0x%X\e[0m, max:\e[1;4m0x%X\e[0m", store[*index].value.min, store[*index].value.max);} //hex
                    if (strlen(buffer) && strlen(buffer1)){strcat(buffer, ", ");}
                    if (strlen(buffer1)){strcat(buffer, buffer1);}
                }

                if (strlen(buffer)){
                    int tmpcol = (tty_width - strcpy_noescape(NULL, buffer, 20)) / 2;
                    fprintf(stdout, "\e[%d;%dH\e[2K%s", store[*index].defval.y, tmpcol, buffer);
                }
            } else if (store[*index_last].defval.y > 0){
                fprintf(stdout, "\e[%d;0H\e[2K", store[*index_last].defval.y);
            }

            //hint
            if (store[*index].hint.y > 0 && store[*index].hint.str && store[*index].position.size){
                int tmpcol = (tty_width - strcpy_noescape(NULL, store[*index].hint.str, 20)) / 2;
                fprintf(stdout, "\e[%d;%dH\e[2K%s", store[*index].hint.y, tmpcol, store[*index].hint.str);
            } else if (store[*index_last].hint.y > 0 && store[*index_last].hint.str && store[*index_last].position.size){
                fprintf(stdout, "\e[%d;0H\e[2K", store[*index_last].hint.y);
            }
        }
    }
    *index_last = *index;

    bool selected, valid;
    for (int i=0; i<index_limit; i++){
        selected = (i==*index);
        if (store[i].position.size){
            if (selected && !store[i].disabled){
                if (input->enter && store[i].value.ptrbool && (store[i].type == 1 || store[i].type == 2)){ //set button pressed while selected element is bool
                    *store[i].value.ptrbool = !(*store[i].value.ptrbool); update = true; //toggle bool
                } else if ((input->left || input->right || input->minus || input->plus) && store[i].value.ptrint && (store[i].type == 0 || store[i].type == 3)){ //minus/plus button pressed while selected element is int
                    int increment = 1;
                    if (input->minus || input->plus){increment = 50;}
                    if (input->left || input->minus){increment*=-1;}
                    int tmpval = *store[i].value.ptrint + increment;
                    if (tmpval <= store[i].value.min){tmpval = store[i].value.min;} else if (tmpval >= store[i].value.max){tmpval = store[i].value.max;} //clamp
                    *store[i].value.ptrint = tmpval; update = true;
                }
            }

            if (update || store[i].value.force_update){
                int tmpcol_bg = 100, tmpcol_txt = 97, tmpcol_style = 0;
                if (selected){tmpcol_bg = 47; tmpcol_txt = 30; tmpcol_style = 1;}
                if (store[i].disabled){tmpcol_bg = 100; tmpcol_txt = 37; tmpcol_style = 0;}
                char* tmpptr = NULL; char buffer[store[i].position.size + 1];
                if (store[i].value.ptrchar){tmpptr = store[i].value.ptrchar; //char
                } else if (store[i].value.ptrint){ //int/hex
                    char fmtbuffer[10];
                    if (store[i].type == 3){
                        char buffer1[128]; sprintf(buffer1, "0x%X", *store[i].value.ptrint);
                        sprintf(fmtbuffer, "%%%ds", store[i].position.size); sprintf(buffer, fmtbuffer, buffer1);
                    } else {sprintf(fmtbuffer, "%%%dd", store[i].position.size); sprintf(buffer, fmtbuffer, *store[i].value.ptrint);}
                    tmpptr = buffer;
                } else if (store[i].type == 2 && store[i].value.ptrbool){sprintf(buffer, "%s", *(store[i].value.ptrbool)?"X":" "); tmpptr = buffer;} //toogle bool
                if (tmpptr){fprintf(stdout, "\e[%d;%dH\e[%d;%d;%d;4m%s\e[0m", store[i].position.y, store[i].position.x, tmpcol_style, tmpcol_bg, tmpcol_txt, tmpptr);}
            }
        }
    }
}

//term screen functions
#define term_screen_count 255
int term_screen_current = 0, term_screen_last = -1;
bool term_screen_update = false;

static void term_screen_main(int tty_line, int tty_last_width, int tty_last_height){ //main screen
    char buffer[buffer_size], buffer1[buffer_size];

    int hint_line = tty_last_height - 6, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal; 

    strcpy(buffer, "Freeplay I2C Joystick");
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(buffer))/2, tmp_esc_col, buffer);
    if (!i2c_failed){ //display device id/version if i2c not failed
        sprintf(buffer, "device id:0x%02X version:%d", i2c_dev_id, i2c_dev_minor);
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(buffer))/2, tmp_esc_col, buffer);
    }
    tty_line+=2;

    //i2c bus
    if (i2c_failed_bus){tmp_esc_col = term_esc_col_error;}
    sprintf(buffer, "%s", i2c_failed_bus?term_hint_i2c_failed[0]:term_hint_main[0]);
    fprintf(stdout, "\e[%d;%dH\e[%dmI2C bus:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+8, .y=tty_line, .size=3}, .type=0, .value={.min=0, .max=255, .ptrint=&driver_user.bus}, .defval={.ptrint=&driver_default.bus, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    if (i2c_failed_bus){goto i2c_failed_jump;} //skip other settings if i2c failed
    tty_line+=2;

    //i2c address
    if (i2c_failed_dev){tmp_esc_col = term_esc_col_error;}
    sprintf(buffer, "%s", i2c_failed_dev?term_hint_i2c_failed[1]:term_hint_main[1]);
    fprintf(stdout, "\e[%d;%dH\e[%dmI2C address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+12, .y=tty_line, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&driver_user.addr}, .defval={.ptrint=&driver_default.addr, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    if (i2c_failed_dev){goto i2c_failed_jump;} //skip other settings if i2c failed
    tty_line++;

    //report i2c signature
    if (!i2c_failed_sig){sprintf(buffer, "Detected device signature: 0x%02X", i2c_dev_sig);
    } else {tmp_esc_col = term_esc_col_error; sprintf(buffer, "Wrong device signature, was expecting 0x%02X but got 0x%02X", i2c_dev_manuf, i2c_dev_sig);}
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    if (i2c_failed_sig){goto i2c_failed_jump;}
    tty_line+=2;

    //interrupt pin
    fprintf(stdout, "\e[%d;%dH\e[%dmInterrupt:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main[2]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+10, .y=tty_line, .size=3}, .type=0, .value={.min=-1, .max=127, .ptrint=&driver_user.interrupt}, .defval={.ptrint=&driver_default.interrupt, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    tty_line+=2;

    //buttons no
    fprintf(stdout, "\e[%d;%dH\e[%dmButtons:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main[3]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+8, .y=tty_line, .size=3}, .type=0, .value={.min=0, .max=button_count_limit, .ptrint=&driver_user.digitalbuttons}, .defval={.ptrint=&driver_default.digitalbuttons, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    tty_line+=2;

    //dpad enabled
    fprintf(stdout, "\e[%d;%dH\e[%dmDpad:_ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main[4]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+5, .y=tty_line, .size=3}, .type=2, .value={.ptrbool=&driver_user.dpads}, .defval={.ptrbool=&driver_default.dpads, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    tty_line+=2;

    //adc configuration
    bool term_go_adc_config = false;
    char* term_buttons_adc_config = " Analog Configuration \0";
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col, .y=tty_line, .size=strlen(term_buttons_adc_config)}, .type=1, .value={.ptrchar=term_buttons_adc_config, .ptrbool=&term_go_adc_config}, .hint={.y=hint_line, .str=term_hint_main[5]}};

    i2c_failed_jump:; //jump point for i2c failure

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-5, (tty_last_width - strcpy_noescape(NULL, term_hint_generic[0], 20)) / 2, term_hint_generic[0]); //nav hint

    //buttons
    const int term_buttons0_footer_count = 2;
    int term_buttons0_pad = (tty_last_width - term_footer_buttons_width * term_buttons0_footer_count) / (term_buttons0_footer_count + 1);
    term_pos_string_t term_buttons0_footer_string[2] = {0};
    char* term_buttons0_name[] = {"Discard\0", "Default\0"};
    bool* term_buttons0_bool[] = {&reset_resquested, &default_resquested};
    const int term_buttons0_hint[] = {5, 4};
    for (int i = 0; i < term_buttons0_footer_count; i++){
        int x = term_buttons0_pad + (term_footer_buttons_width + term_buttons0_pad) * i;
        strcpy(buffer, term_buttons0_name[i]); array_pad(buffer, strlen(buffer), term_footer_buttons_width, ' ', 0); strcpy(term_buttons0_footer_string[i].str, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-3, .size=term_footer_buttons_width}, .type=1, .value={.ptrchar=term_buttons0_footer_string[i].str, .ptrbool=term_buttons0_bool[i]}, .hint={.y=hint_line, .str=term_hint_generic[term_buttons0_hint[i]]}};
    }

    const int term_buttons1_footer_count = 3;
    int term_buttons1_pad = (tty_last_width - term_footer_buttons_width * term_buttons1_footer_count) / (term_buttons1_footer_count + 1);
    term_pos_string_t term_buttons1_footer_string[3] = {0};
    char* term_buttons1_name[] = {"Save config\0", "Save file\0", "Close\0"};
    bool* term_buttons1_bool[] = {&save_boot_resquested, &save_text_resquested, &kill_resquested};
    bool term_buttons1_bool_disabled[] = {i2c_failed, i2c_failed, false};
    const int term_buttons1_hint[] = {2, 3, 7};
    for (int i = 0; i < term_buttons1_footer_count; i++){
        int x = term_buttons1_pad + (term_footer_buttons_width + term_buttons1_pad) * i;
        strcpy(buffer, term_buttons1_name[i]); array_pad(buffer, strlen(buffer), term_footer_buttons_width, ' ', 0); strcpy(term_buttons1_footer_string[i].str, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_buttons1_bool_disabled[i], .value={.ptrchar=term_buttons1_footer_string[i].str, .ptrbool=term_buttons1_bool[i]}, .hint={.y=hint_line, .str=term_hint_generic[term_buttons1_hint[i]]}};
    }

    select_index_last = -1; //force selectible element update on first loop
    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_go_adc_config){term_screen_current = 1; goto funct_end;} //return to adc configuration screen requested
        if (save_boot_resquested || save_text_resquested){kill_resquested = true; goto funct_end;}

        if (reset_resquested){ //restore backup requested
            driver_user.bus = driver_back.bus; driver_user.addr = driver_back.addr;
            driver_user.interrupt = driver_back.interrupt; driver_user.digitalbuttons = driver_back.digitalbuttons; driver_user.dpads = driver_back.dpads;
        }

        if (default_resquested){ //restore defaults requested
            driver_user.bus = driver_default.bus; driver_user.addr = driver_default.addr;
            driver_user.interrupt = driver_default.interrupt; driver_user.digitalbuttons = driver_default.digitalbuttons; driver_user.dpads = driver_default.dpads;
        }

        if (driver_user.bus != i2c_bus_prev || driver_user.addr != i2c_addr_prev || reset_resquested || default_resquested){ //i2c bus or address changed
            i2c_bus_prev = driver_user.bus; i2c_addr_prev = driver_user.addr;
            i2c_init(&i2c_fd, driver_user.bus, driver_user.addr);
            if (reset_resquested || default_resquested){select_index_current = 0; reset_resquested = false; default_resquested = false;}
            term_screen_update = true; goto funct_end; //force full redraw
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (10000);
    }
    funct_end:; //jump point for fast exit
}


static void term_screen_adc(int tty_line, int tty_last_width, int tty_last_height){ //display adc screen
    term_pos_generic_t term_adc_raw[4] = {0}, term_adc_output[4] = {0};
    term_pos_string_t term_adc_string[4] = {0};

    char buffer[buffer_size], buffer1[buffer_size];
    int term_adc_pad = (tty_last_width - term_adc_width * 2) / 3; //padding between each ADC sections
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1;

    for(int x_loop=0, adc_loop=0; x_loop<2; x_loop++){
        int term_left = term_adc_pad + (term_adc_width + term_adc_pad) * x_loop, term_right = term_left + term_adc_width, tmp_line = tty_line, tmp_line_last = tty_line; //left/right border of current adc
        int x, x1, x2, w;
        bool js_used = adc_reg_used[2*x_loop] && adc_reg_used[2*x_loop+1];

        //enable joystick
        sprintf(buffer, "Joystick %d enabled:-", x_loop);
        x = 1 + term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        if (js_used){term_select[select_limit] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=js_enabled[x_loop]}, .defval={.y=hint_def_line, .ptrbool=js_enabled_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc[0]}};}
        select_limit++; tmp_line++;

        for(int y_loop=0; y_loop<2; y_loop++){
            int term_esc_col = term_esc_col_disabled;
            bool adc_used = adc_reg_used[adc_loop], adc_enabled = adc_reg_enable[adc_loop];
            tmp_line++;

            //adc "title"
            if (adc_used){sprintf (buffer1, "%dbits", adc_res); term_esc_col = term_esc_col_normal;} else if (adc_enabled){sprintf (buffer1, "available");} else {sprintf (buffer1, "disabled");}
            sprintf(buffer, "ADC%d(%s)(%s%s)", adc_loop, adc_data[adc_loop].name, buffer1, (adc_enabled && (!js_used || !(*js_enabled[x_loop])))?",unused":""); strcpy(term_adc_string[adc_loop].str, buffer);
            x = term_left + array_pad(buffer, strlen(buffer), term_adc_width, '_', 0); w = strlen(buffer1);
            fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tmp_line, term_left, term_esc_col, buffer);
            if (adc_enabled){term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=w}, .type=1, .value={.ptrchar=term_adc_string[adc_loop].str, .ptrbool=&adc_reg_used[adc_loop]}, .hint={.y=hint_line, .str=term_hint_adc[0]}};}
            tmp_line++;

            //limits
            x = term_right - 17; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmlimits\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_res_limit, .force_update=true, .ptrint=adc_settings[adc_loop].min}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].min}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_res_limit, .force_update=true, .ptrint=adc_settings[adc_loop].max}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].max}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
            }
            tmp_line+=2;

            //raw, output
            term_adc_raw[adc_loop].x = term_left + 4; term_adc_raw[adc_loop].y = tmp_line; term_adc_raw[adc_loop].w = 6;
            term_adc_output[adc_loop].x = term_right - 4; term_adc_output[adc_loop].y = tmp_line; term_adc_output[adc_loop].w = 4;
            fprintf(stdout, "\e[%d;%dH\e[%dmraw:------\e[0m", tmp_line, term_adc_raw[adc_loop].x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmoutput:----\e[0m", tmp_line, term_adc_output[adc_loop].x - 7, term_esc_col);
            tmp_line++;

            //reverse, raw min/max
            x = term_left + 7; x1 = term_right - 17; x2 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dminvert:-\e[0m", tmp_line, x - 7, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x2 - 4, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=adc_settings[adc_loop].reversed}, .defval={.y=hint_def_line, .ptrbool=adc_settings_default[adc_loop].reversed}, .hint={.y=hint_line, .str=term_hint_adc[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_min[adc_loop], .ptrint=&adc_data[adc_loop].raw_min}, .hint={.y=hint_line, .str=term_hint_adc[2]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_max[adc_loop], .ptrint=&adc_data[adc_loop].raw_max}, .hint={.y=hint_line, .str=term_hint_adc[3]}};
            }
            tmp_line+=2;

            //flat, fuzz
            x = term_left + 5; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmflat:------\e[0m", tmp_line, x - 5, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmfuzz:------\e[0m", tmp_line, x1 - 5, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.min=0, .max=adc_res_limit/2, .ptrint=adc_settings[adc_loop].flat}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].flat}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.min=0, .max=adc_res_limit/2, .ptrint=adc_settings[adc_loop].fuzz}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].fuzz}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
            }
            tmp_line += 2; tmp_line_last = tmp_line; adc_loop++;
        }

        //swap xy axis
        sprintf(buffer, "Swap %s/%s axis:-", adc_data[adc_loop-2].name, adc_data[adc_loop-1].name);
        x = term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line_last, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        if (js_used){term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=adc_axis_swap[x_loop]}, .defval={.y=hint_def_line, .ptrbool=adc_axis_swap_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc[4]}};}
    }

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_generic[0], 20)) / 2, term_hint_generic[0]); //nav hint

    //buttons
    bool term_back_mainscreen = false;
    const int term_buttons_footer_count = 3;
    term_pos_string_t term_buttons_footer_string[3] = {0};
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_buttons_footer_count) / (term_buttons_footer_count + 1);
    char* term_buttons_name[] = {"Discard\0", "Default\0", "Back\0"};
    bool* term_buttons_bool[] = {&reset_resquested, &default_resquested, &term_back_mainscreen};
    bool term_buttons_bool_disabled[] = {i2c_failed, i2c_failed, false};
    const int term_buttons_hint[] = {5, 4, 6};
    for (int i=0; i<term_buttons_footer_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        strcpy(buffer, term_buttons_name[i]); array_pad(buffer, strlen(buffer), term_footer_buttons_width, ' ', 0); strcpy(term_buttons_footer_string[i].str, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_buttons_bool_disabled[i] ,.value={.ptrchar=term_buttons_footer_string[i].str, .ptrbool=term_buttons_bool[i]}, .hint={.y=hint_line, .str=term_hint_generic[term_buttons_hint[i]]}};
    }

    select_index_last = -1; //force selectible element update on first loop
    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        if (!i2c_failed){
            adc_data[0].raw = adc_data[1].raw = adc_data[2].raw = adc_data[3].raw = adc_res_limit / 2; //reset raw to midpoint

            if (adc_reg_enable[0] || adc_reg_enable[1]){ //read adc0-1
                if (i2c_smbus_read_i2c_block_data(i2c_fd, i2c_mcu_register_js0, 3, (uint8_t *)&i2c_joystick_registers+i2c_mcu_register_js0) == 3){
                    adc_data[0].raw = ((i2c_joystick_registers.a0_msb << 8) | (i2c_joystick_registers.a1a0_lsb & 0x0F) << 4) >> (16 - adc_res);
                    adc_data[1].raw = ((i2c_joystick_registers.a1_msb << 8) | (i2c_joystick_registers.a1a0_lsb & 0xF0)) >> (16 - adc_res);
                }
            }

            if (adc_reg_enable[2] || adc_reg_enable[3]){ //read adc2-3
                if (i2c_smbus_read_i2c_block_data(i2c_fd, i2c_mcu_register_js1, 3, (uint8_t *)&i2c_joystick_registers+i2c_mcu_register_js1) == 3){
                    adc_data[2].raw = ((i2c_joystick_registers.a2_msb << 8) | (i2c_joystick_registers.a3a2_lsb & 0x0F) << 4) >> (16 - adc_res);
                    adc_data[3].raw = ((i2c_joystick_registers.a3_msb << 8) | (i2c_joystick_registers.a3a2_lsb & 0xF0)) >> (16 - adc_res);
                }
            }

            for(int i=0; i<4; i++){
                if (adc_reg_enable[i] && adc_reg_used[i]){
                    if(adc_data[i].raw < adc_data[i].raw_min){adc_data[i].raw_min = adc_data[i].raw;} //update raw min value
                    if(adc_data[i].raw > adc_data[i].raw_max){adc_data[i].raw_max = adc_data[i].raw;} //update raw max value

                    sprintf(buffer, "\e[%d;%dH\e[1;4;%dm%%%dd\e[0m", term_adc_raw[i].y, term_adc_raw[i].x, term_esc_col_normal, term_adc_raw[i].w);
                    fprintf(stdout, buffer, adc_data[i].raw); //raw

                    adc_data[i].value = adc_defuzz(adc_data[i].raw, adc_data[i].raw_prev, *adc_settings[i].fuzz); //defuzz
                    adc_data[i].offset = (((*adc_settings[i].max - *adc_settings[i].min) / 2) + *adc_settings[i].min) - (adc_res_limit / 2);
                    adc_data[i].value = adc_correct_offset_center(adc_res_limit, adc_data[i].value, *adc_settings[i].min, *adc_settings[i].max, adc_data[i].offset, *adc_settings[i].flat/2, *adc_settings[i].flat/2);
                    if (*adc_settings[i].reversed){adc_data[i].value = abs(adc_res_limit - adc_data[i].value);} //reversed axis

                    int adc_value_percent = (((double)adc_data[i].value / adc_res_limit) * 202) - 101; //adc position to -101+101
                    if (adc_value_percent < -99){adc_value_percent = -100;} else if (adc_value_percent > 99){adc_value_percent = 100;} //bypass rounding problems
                    sprintf(buffer, "\e[%d;%dH\e[%d;4;%dm%%%dd\e[0m", term_adc_output[i].y, term_adc_output[i].x, (adc_value_percent==-100||adc_value_percent==0||adc_value_percent==100)?7:1, term_esc_col_normal, term_adc_output[i].w);
                    fprintf(stdout, buffer, adc_value_percent); //output
                }
            }
        }

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_input.escape){term_back_mainscreen = true;}
        if (term_back_mainscreen){term_screen_current = 0; goto funct_end;} //return to main screen requested

        if (reset_resquested){ //restore backup requested
            for (int i=0; i<4; i++){*adc_settings[i].min = *adc_settings_back[i].min; *adc_settings[i].max = *adc_settings_back[i].max; *adc_settings[i].fuzz = *adc_settings_back[i].fuzz; *adc_settings[i].flat = *adc_settings_back[i].flat; *adc_settings[i].reversed = *adc_settings_back[i].reversed;}
            *js_enabled[0] = *js_enabled_backup[0]; *js_enabled[1] = *js_enabled_backup[1]; //js enabled
            *adc_axis_swap[0] = *adc_axis_swap_backup[0]; *adc_axis_swap[1] = *adc_axis_swap_backup[1]; //axis swap
            select_index_current = 0; reset_resquested = false; term_screen_update = true; goto funct_end; //force full redraw
        }

        if (default_resquested){ //restore defaults requested
            for (int i=0; i<4; i++){*adc_settings[i].min = *adc_settings_default[i].min; *adc_settings[i].max = *adc_settings_default[i].max; *adc_settings[i].fuzz = *adc_settings_default[i].fuzz; *adc_settings[i].flat = *adc_settings_default[i].flat; *adc_settings[i].reversed = *adc_settings_default[i].reversed;}
            *js_enabled[0] = *js_enabled_default[0]; *js_enabled[1] = *js_enabled_default[1]; //js enabled
            *adc_axis_swap[0] = *adc_axis_swap_default[0]; *adc_axis_swap[1] = *adc_axis_swap_default[1]; //axis swap
            select_index_current = 0; default_resquested = false; term_screen_update = true; goto funct_end; //force full redraw
        }

        if (!i2c_failed){
            //non selectible elements or specific update
            bool mcu_conf_changed = false;
            for(int i=0; i<4; i++){ //adc loop
                if (adc_reg_enable[i]){ //enabled
                    if (adc_reg_used[i] != adc_reg_used_prev[i]){ //adc configuration update
                        mcu_conf_current.bits ^= 1U << i; //toggle needed bit
                        adc_reg_used_prev[i] = adc_reg_used[i]; mcu_conf_changed = true;
                    }
                    if (adc_use_raw_min[i]){*adc_settings[i].min = adc_data[i].raw_min; adc_use_raw_min[i] = false;} //set raw min as min limit
                    if (adc_use_raw_max[i]){*adc_settings[i].max = adc_data[i].raw_max; adc_use_raw_max[i] = false;} //set raw max as max limit
                    adc_data[i].raw_prev = adc_data[i].raw;
                } else {adc_reg_used[i] = adc_reg_used_prev[i] = false;} //adc not enabled
            }

            //update joystick enabled based on adc enabled
            if (*js_enabled[0]){*js_enabled[0] = adc_reg_used[0] && adc_reg_used[1];}
            if (*js_enabled[1]){*js_enabled[1] = adc_reg_used[2] && adc_reg_used[3];}

            if (*js_enabled[0] != js_enabled_prev[0] || *js_enabled[1] != js_enabled_prev[1] || mcu_conf_changed){ //enabled joystick or adc changed
                js_enabled_prev[0] = *js_enabled[0]; js_enabled_prev[1] = *js_enabled[1];
                term_screen_update = true; goto funct_end; //force full redraw
            }
        } else { //i2c failed
            bool i2c_valid_bool[4] = {i2c_failed_bus, i2c_failed_dev, i2c_failed_sig, i2c_failed};
            for (int i=0; i<4; i++){
                if (i2c_valid_bool[i]){
                    sprintf(buffer, "Error: %s", term_hint_i2c_failed[i]);
                    fprintf(stdout, "\e[%d;%dH\e[2K\e[1;%dm%s\e[0m", tty_last_height-6, (tty_last_width - strlen(buffer)) / 2, term_esc_col_error, buffer);
                    break;
                }
            }
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30);
    }

    funct_end:; //jump point for fast exit
}

void (*term_screen_funct_ptr[term_screen_count])(int, int, int) = {term_screen_main, term_screen_adc}; //pointer to screen functions

static void tty_signal_handler(int sig){ //signal handle func
	if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
	kill_resquested = true;
}


//main
static void program_usage (char* program){ //program usage, obviously
	fprintf(stdout, "Version: %s\n", programversion);
	fprintf(stdout, "Example : %s TODO\n", program);
	fprintf(stdout, "Need to run as root.\n"
	"Arguments:\n"
	"\t-h or -help: show arguments list.\n"
    //TODO something...
	);
}

void program_close(void){
    if (&term_backup){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
    close(i2c_fd);
}

int main (int argc, char** argv){
    program_start_time = get_time_double();
	if (getuid() != 0) {print_stderr("FATAL: this program needs to run as root, current user:%d\n", getuid()); program_usage(argv[0]); return EXIT_FAILURE;} //not running as root

	for(int i=1; i<argc; ++i){ //program arguments parse
		if (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0){program_usage(argv[0]); return 0;} //-h -help argument
		else if (strcmp(argv[i],"-whatever") == 0){/*something*/}
	}

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle

    //dtoverlay
    memcpy(&driver_user, &driver_default, sizeof(driver_default));
//TODO parse dtoverlay arguments
    memcpy(&driver_back, &driver_user, sizeof(driver_user)); //backup detected dtoverlay settings

    //i2c
    i2c_bus_prev = driver_user.bus; i2c_addr_prev = driver_user.addr;
    i2c_init(&i2c_fd, driver_user.bus, driver_user.addr);

    //disable STDIN print
    struct termios term_new;
    if (tcgetattr(STDIN_FILENO, &term_backup) != 0){print_stderr("failed to backup current terminal data\n"); program_close(); return EXIT_FAILURE;}
    if (atexit(program_close) != 0){print_stderr("failed to set atexit() to restore terminal data\n"); program_close(); return EXIT_FAILURE;}
    if (tcgetattr(STDIN_FILENO, &term_new) != 0){print_stderr("failed to save current terminal data for updates\n"); program_close(); return EXIT_FAILURE;}
    term_new.c_lflag &= ~(ECHO | ECHONL | ICANON); //disable input characters, new line character echo, disable canonical input to avoid needs of enter press for user unput submit
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_new) != 0){print_stderr("tcsetattr term_new failed\n"); program_close(); return EXIT_FAILURE;}

    //start term
    tty_start:; //landing point if tty is resized or "screen" changed or bool trigger
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); //set stdin to non-blocking
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); int tty_last_width = ws.ws_col, tty_last_height = ws.ws_row, tty_start_line = 2; //tty size
    fprintf(stdout, "\e[?25l\e[2J"); //hide cursor, reset tty
    if (debug){fprintf(stdout, "\e[1;1H\e[100mtty:%dx%d, screen:%d\e[0m", tty_last_width, tty_last_height, term_screen_current);} //print tty size, 640x480 is 80cols by 30rows

    //reset selectible
    select_index_last = -1; select_limit = 0;
    memset(term_select, 0, sizeof(term_select));
    if (term_screen_current != term_screen_last){select_index_current = 0; term_screen_last = term_screen_current;} //screen changed, reset select index

    term_screen_funct_ptr[term_screen_current](tty_start_line, tty_last_width, tty_last_height); //current "screen" function
    if (term_screen_current != term_screen_last || term_screen_update){term_screen_update = false; goto tty_start;} //reset screen

    tcflush(STDOUT_FILENO, TCIOFLUSH); //flush STDOUT
    fprintf(stdout, "\e[0;0H\e[2J\e[?25h"); //reset tty, show cursor
    program_close(); //restore tty original state

    if (save_boot_resquested){ //save to boot requested
        fprintf(stdout, "TODO SAVE boot/config.txt\n");
    } else if(save_text_resquested){ //save to text requested
        fprintf(stdout, "TODO SAVE text file\n");
    } else {
        fprintf(stdout, "TODO Close without saving\n");
    }

    return 0;
}
