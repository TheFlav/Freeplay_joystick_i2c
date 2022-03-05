/*
* UHID driver diagnostic file
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
int program_diag_mode(void);


void term_screen_main(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_adc(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_digital(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);
void term_screen_save(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/);



//extern
extern void int_rollover(int* /*val*/, int /*min*/, int /*max*/); //rollover int value between (incl) min and max, work both way
extern void int_constrain(int* /*val*/, int /*min*/, int /*max*/); //limit int value to given (incl) min and max value

extern bool kill_resquested;
extern int term_esc_col_normal, term_esc_col_disabled, term_esc_col_error, term_esc_col_success; //color escape codes
extern int term_adc_width, term_adc_vertspacing; //ADC section width, vertical spacing between each horizontal ADC elements
extern int term_footer_buttons_width; //footer button width
extern int select_index_current, select_index_last; //current element selected, last selected
extern int term_screen_current, term_screen_last; //start "screen", last screen
extern bool term_screen_update; //"screen" require update

#ifndef input_registers_count
#define input_registers_count 3
#endif

#ifndef input_registers_size
#define input_registers_size input_registers_count*8
#endif

extern struct winsize ws; //terminal size
extern struct termios term_backup; //original terminal state backup

//extern config
extern bool debug, debug_adv, i2c_poll_rate_disable;
extern bool debug_backup, debug_adv_backup; //backup debug bools
extern int i2c_poll_rate, i2c_adc_poll;
extern int i2c_bus, i2c_addr, i2c_addr_sec, i2c_addr_adc[];
extern int irq_gpio, digital_debounce;
extern bool mcu_js_enable[];
extern bool uhid_js_swap, uhid_js_swap_axis[];

typedef struct adc_data_struct_t adc_data_t;
extern adc_data_t* adc_params_ptr;

