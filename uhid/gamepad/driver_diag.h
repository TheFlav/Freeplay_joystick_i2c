/*
* FreeplayTech UHID gamepad driver
* Diagnostic part header
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
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <termios.h>
#include <assert.h>

#define buffer_size 1024 //char array buffer size

typedef struct term_select_struct { //selectible terminal elements data
    struct {int x, y, size;} position; //position pointers
    int type; //0:int, 1:bool, 2:bool(visual toogle), 3:hex
    struct {int min, max; char *ptrchar; bool *ptrbool; int *ptrint; bool force_update;} value;
    struct {int y; char *str;} hint; //hint data
    struct {int y; bool *ptrbool; int *ptrint;} defval; //default data
    bool disabled; //is disabled
} term_select_t;

typedef struct term_input_struct {bool up, down, left, right, plus, minus, tab, enter, enter_hold, escape;} term_input_t; //terminal key input structure
extern term_input_t term_input;

typedef struct term_pos_generic_struct { //generic terminal position pointer structure
    int x, y, w; //col, line, width
} term_pos_generic_t;

typedef struct term_pos_string_struct {
    int x, y, w; //col, line, width
    char str[buffer_size]; //char array
} term_pos_string_t;

#define term_pos_button_buffer_size 128
typedef struct term_pos_button_struct {
    int x, y, w; //col, line, width
    char str[term_pos_button_buffer_size]; //char array pointer
    bool *ptrbool; //bool pointer to toogle
    char *ptrhint; //hint char array pointer
    bool disabled; //is disabled
} term_pos_button_t;

//prototypes
static void array_fill(char* /*arr*/, int /*size*/, char /*chr*/); //fill array with given character, works with '\0' for full reset, last char set to '\0'
static int array_pad(char* /*arr*/, int /*arr_len*/, int /*size*/, char /*pad*/, int /*align*/); //pad a array with 'pad', 'align': 0:center 1:left 2:right, 'size':final array size, return padding length
static void str_trim_whitespace(char** /*ptr*/); //update pointer to skip leading pointer, set first trailing space to null char
static int strcpy_noescape(char* /*dest*/, char* /*src*/, int /*limit*/); //strcpy "clone" that ignore terminal escape code, set dest=src or dest=NULL to only return "noescape" char array length. Current limitations:defined limit of escape code (w/o "\e["). warnings: no size check, broken if badly formated escape, only check for h,l,j,m ending

static void term_user_input(term_input_t* /*input*/); //process terminal key inputs
static void term_select_update(term_select_t* /*store*/, int* /*index*/, int* /*index_last*/, int /*index_limit*/, term_input_t* /*input*/, int /*tty_width*/, int /*tty_height*/); //update selectible elements

static int term_print_path_multiline(char* /*str*/, int /*line*/, int /*col*/, int /*width_limit*/, int /*esc_color*/); //print a multiple line if needed, return no of lines

int term_init(void); //init terminal related vars
int program_diag_mode(void); //main diag mode function

void vars_main_default(void); //reset all main config vars to default
void vars_adc_default(void); //reset all adc config vars to default
void vars_cfg_reload(void); //reload config file

void term_screen_main(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_adc(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_digital(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_save(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_advanced(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);

//extern funct
extern void int_rollover(int* /*val*/, int /*min*/, int /*max*/); //rollover int value between (incl) min and max, work both way
extern void int_constrain(int* /*val*/, int /*min*/, int /*max*/); //limit int value to given (incl) min and max value
extern void vars_main_default(void); //reset all main config vars to default
extern void vars_adc_default(void); //reset all adc config vars to default

extern int i2c_check_bus(int /*bus*/); //check I2C bus, return 0 on success, -1:addr
extern int i2c_open_dev(int* /*fd*/, int /*bus*/, int /*addr*/); //open I2C device, return 0 on success, -1:bus, -2:addr, -3:generic error
extern void i2c_close_all(void); //close all I2C file

extern int mcu_check_manufacturer(void); //check device manufacturer, fill signature,id,version, return 0 on success, -1 on wrong manufacturer, -2 on i2c error
extern int mcu_update_config0(void); //read/update config0 register, return 0 on success, -1 on error
extern int init_adc(void); //init adc data, return 0 on success, -1 on resolution read fail, -2 on adc conf
extern void uhid_joystick_swap(void); //uhid joystick/axis swap

extern void i2c_poll_joystick(bool /*force_update*/); //poll data from i2c device

extern void adc_data_compute(int /*adc_index*/); //compute adc max value, flat in/out, offset


//diagnostic part
int term_esc_col_normal = 97; //normal color escape code
int term_esc_col_disabled = 90; //disabled color escape code
int term_esc_col_error = 91; //error color escape code
int term_esc_col_success = 92; //success color escape code

int term_footer_buttons_width = 15; //footer button width

int term_adc_width = 32; //ADC section width
int term_adc_vertspacing = 9; //vertical spacing between each horizontal ADC elements

int select_index_current = 0, select_index_last = -1; //current element selected, last selected

int term_screen_current = 2, term_screen_last = -1; //start "screen", last screen
int term_screen_update = false; //"screen" require update

void (*term_screen_funct_ptr[])(int, int, int) = {term_screen_main, term_screen_adc, term_screen_digital, term_screen_save, term_screen_advanced}; //pointer to screen functions

struct winsize ws; //terminal size
term_input_t term_input = {0}; //contain user inputs

char* term_hint_nav_str[]={
    "\e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate",
    "\e[1m[LEFT]\e[0m,\e[1m[RIGHT]\e[0m to change value, \e[1m[-]\e[0m,\e[1m[+]\e[0m for plus/minus 50",
};

char* buttons_dpad_names[] = {"Dpad_UP", "Dpad_DOWN", "Dpad_LEFT", "Dpad_RIGHT"};
char* buttons_gamepad_names[uhid_buttons_count] = {"A","B","C","X","Y","Z","TL","TR","TL2","TR2","SELECT","START","MODE","THUMBL","THUMBR"};
char* buttons_misc_names[uhid_buttons_misc_count] = {"BTN_0","BTN_1","BTN_2","BTN_3"};


//main program
extern bool kill_requested; //allow clean close
extern struct termios term_backup; //original terminal state backup
extern bool debug, debug_adv;  //enable debug output

extern char* js_axis_names[]; //joystick axis names, virtually start at index -1


//i2c
bool i2c_bus_failed = false; //bus failure
int i2c_bus_err = 121, i2c_main_err = 121, i2c_sec_err = 121; //backup detected i2c errors

extern int i2c_fd, i2c_fd_sec; //mcu i2c fd
extern bool i2c_adc_fd_valid[]; //are external fd valid
extern int i2c_bus, i2c_addr, i2c_addr_sec; //I2C bus, mcu main/sec address
int i2c_bus_back = -1, i2c_addr_back = -1, i2c_addr_sec_back = -1;

extern int i2c_poll_rate, i2c_adc_poll; //Driver pollrate in hz. Poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on

extern struct i2c_joystick_register_struct i2c_joystick_registers;
extern struct i2c_secondary_address_register_struct i2c_secondary_registers;


//mcu
extern uint8_t i2c_dev_sig, i2c_dev_id, i2c_dev_minor; //device device signature, id, version
extern int digital_debounce; //debounce filtering to mitigate possible pad false contact, default:5, max:7, 0 to disable
extern bool mcu_adc_enabled[]; //adc enabled on mcu, set during runtime

//adc
extern int adc_map[]; //adc to joystick maps for uhid report

#ifdef ALLOW_EXT_ADC 
    extern int i2c_addr_adc[]; //external address
    extern int i2c_adc_fd[]; //external fd
    int i2c_addr_adc_back[] = {-1, -1, -1, -1}; //external address backup
    int i2c_adc_err[] = {121, 121, 121, 121}; //backup external detected i2c errors
#endif

extern adc_data_t adc_params[];
adc_data_t adc_params_default[4] = {
	{-1,-1,INT_MAX,INT_MIN, def_adc0_res,0xFF, 0x7FFF,def_adc0_min,def_adc0_max,0, def_adc0_fuzz,def_adc0_flat,def_adc0_flat, 0,0, def_adc0_enabled,def_adc0_reversed,def_adc0_autocenter},
	{-1,-1,INT_MAX,INT_MIN, def_adc1_res,0xFF, 0x7FFF,def_adc1_min,def_adc1_max,0, def_adc1_fuzz,def_adc1_flat,def_adc1_flat, 0,0, def_adc1_enabled,def_adc1_reversed,def_adc1_autocenter},
	{-1,-1,INT_MAX,INT_MIN, def_adc2_res,0xFF, 0x7FFF,def_adc2_min,def_adc2_max,0, def_adc2_fuzz,def_adc2_flat,def_adc2_flat, 0,0, def_adc2_enabled,def_adc2_reversed,def_adc2_autocenter},
	{-1,-1,INT_MAX,INT_MIN, def_adc3_res,0xFF, 0x7FFF,def_adc3_min,def_adc3_max,0, def_adc3_fuzz,def_adc3_flat,def_adc3_flat, 0,0, def_adc3_enabled,def_adc3_reversed,def_adc3_autocenter},
};

bool adc_enabled_back[] = {def_adc0_enabled, def_adc1_enabled, def_adc2_enabled, def_adc3_enabled};


//irq
extern int irq_gpio; //gpio pin used for IRQ, limited to 31 for pigpio, set to -1 to disable


//config
extern cfg_vars_t cfg_vars[]; //config store
extern const unsigned int cfg_vars_arr_size; //config store size

