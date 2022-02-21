/*
* Freeplaytech I2C Joystick Kernel driver setup/diagnostic tool
* Notes:
* - This include file main here to "offload" diag.c file.
* - Struct definitions and variables not meant to be changed by users are defined here.
*/

//program related
const char programname[] = "Freeplay I2C Joystick"; //program name
const char programversion[] = "0.1c"; //program version
char program_path[PATH_MAX] = {'\0'}; //full path to this program
bool kill_resquested = false; //program kill requested, stop main loop in a smooth way
#define buffer_size 1024 //char array buffer size
double program_start_time = 0.; //program start time

//dtoverlay related
typedef struct dtoverlay_driver_struct { //FPJS dtoverlay file structure, convert from freeplay-joystick-overlay.dts. To be updated if dts file changed.
    int bus, addr; //i2c bus, address //TODO UPDATE DTOVERLAY STRUCT
    int interrupt; //interrupt pin
    //int analogsticks; //TODO UPDATE DTOVERLAY STRUCT, analog stricks count, 0:none, 1:left, 2:left-right
    int digitalbuttons; //digital buttons count
    bool dpads; //dpad enabled

    //char joy0_x_params[buffer_size], joy0_y_params[buffer_size]; //joystick0 config: min,max,fuzz,flat,inverted. default:"0 0xFFF 32 300 0"
    int joy0_x_min, joy0_x_max, joy0_x_fuzz, joy0_x_flat; bool joy0_x_inverted/*, joy0_x_enabled*/; //TODO UPDATE DTOVERLAY STRUCT: enable
    int joy0_y_min, joy0_y_max, joy0_y_fuzz, joy0_y_flat; bool joy0_y_inverted/*, joy0_y_enabled*/; //TODO UPDATE DTOVERLAY STRUCT: enable
    bool joy0_swapped_x_y; //xy swapped. default:0
    bool joy0_enabled; //TODO UPDATE DTOVERLAY STRUCT

    //char joy1_x_params[buffer_size], joy1_y_params[buffer_size]; //joystick1 config: min,max,fuzz,flat,inverted. default:"0 0xFFF 32 300 0"
    int joy1_x_min, joy1_x_max, joy1_x_fuzz, joy1_x_flat; bool joy1_x_inverted/*, joy1_x_enabled*/; //TODO UPDATE DTOVERLAY STRUCT: enable
    int joy1_y_min, joy1_y_max, joy1_y_fuzz, joy1_y_flat; bool joy1_y_inverted/*, joy1_y_enabled*/; //TODO UPDATE DTOVERLAY STRUCT: enable
    bool joy1_swapped_x_y; //xy swapped. default:0
    bool joy1_enabled; //TODO UPDATE DTOVERLAY STRUCT
} dtoverlay_driver_t;
dtoverlay_driver_t driver_user = {0}, driver_back = {0}; //init

typedef struct dtoverlay_parser_store_struct { //dtoverlay argument to program "transaction" structure
	const char* name; //argument name
	const int type; //0:int, 1:hex, 2:bool. int and hex split for saving part
	const void* ptr; //notype pointer to stored value, "type" used for proper cast
} dtoverlay_parser_store_t;

char* dtoverlay_text_filename = "dtoverlay.txt";
//bool dtoverlay_incl_defaults = true;

//I2C/ADC related
uint8_t i2c_dev_sig = 0, i2c_dev_id = 0, i2c_dev_minor = 0; //device device signature, id, version
int i2c_fd = -1, i2c_bus_prev = 0, i2c_addr_prev = 0; //file definition, previous bus/address
int adc_res = 10, adc_res_limit = 1023; //default adc resolution, computed during runtime

typedef union { //adc configuration bitfield structure
    struct {uint8_t use0:1, use1:1, use2:1, use3:1, en0:1, en1:1, en2:1, en3:1;} vals;
    uint8_t bits;
} mcu_adc_conf_t;
mcu_adc_conf_t mcu_conf_current = {0};
bool adc_reg_enable[4]={0}; 
bool adc_reg_used[4]={0}, adc_reg_used_prev[4]={0}, adc_reg_used_backup[4]={0};

typedef struct adc_data_struct_t { //adc readed data structure
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

typedef struct adc_settings_struct_t { //adc configuration pointers structure
	int *min, *max; //current value, min/max limits
	int *fuzz, *flat; //fuzz, flat
	bool *reversed; //reverse reading
	/*bool *enabled;*/
} adc_settings_t;

adc_settings_t adc_settings[4] = { //current adc data pointers
    {&driver_user.joy0_x_min, &driver_user.joy0_x_max, &driver_user.joy0_x_fuzz, &driver_user.joy0_x_flat, &driver_user.joy0_x_inverted/*, &driver_user.joy0_x_enabled*/},
    {&driver_user.joy0_y_min, &driver_user.joy0_y_max, &driver_user.joy0_y_fuzz, &driver_user.joy0_y_flat, &driver_user.joy0_y_inverted/*, &driver_user.joy0_y_enabled*/},
    {&driver_user.joy1_x_min, &driver_user.joy1_x_max, &driver_user.joy1_x_fuzz, &driver_user.joy1_x_flat, &driver_user.joy1_x_inverted/*, &driver_user.joy1_x_enabled*/},
    {&driver_user.joy1_y_min, &driver_user.joy1_y_max, &driver_user.joy1_y_fuzz, &driver_user.joy1_y_flat, &driver_user.joy1_y_inverted/*, &driver_user.joy1_y_enabled*/},
};
bool *adc_axis_swap[2] = {&driver_user.joy0_swapped_x_y, &driver_user.joy1_swapped_x_y};
bool *js_enabled[2] = {&driver_user.joy0_enabled, &driver_user.joy1_enabled}, js_enabled_prev[2]={0};

adc_settings_t adc_settings_back[4] = { //backup adc data pointers
    {&driver_back.joy0_x_min, &driver_back.joy0_x_max, &driver_back.joy0_x_fuzz, &driver_back.joy0_x_flat, &driver_back.joy0_x_inverted/*, &driver_back.joy0_x_enabled*/},
    {&driver_back.joy0_y_min, &driver_back.joy0_y_max, &driver_back.joy0_y_fuzz, &driver_back.joy0_y_flat, &driver_back.joy0_y_inverted/*, &driver_back.joy0_y_enabled*/},
    {&driver_back.joy1_x_min, &driver_back.joy1_x_max, &driver_back.joy1_x_fuzz, &driver_back.joy1_x_flat, &driver_back.joy1_x_inverted/*, &driver_back.joy1_x_enabled*/},
    {&driver_back.joy1_y_min, &driver_back.joy1_y_max, &driver_back.joy1_y_fuzz, &driver_back.joy1_y_flat, &driver_back.joy1_y_inverted/*, &driver_back.joy1_y_enabled*/},
};
bool *adc_axis_swap_backup[2] = {&driver_back.joy0_swapped_x_y, &driver_back.joy1_swapped_x_y};
bool *js_enabled_backup[2] = {&driver_back.joy0_enabled, &driver_back.joy1_enabled};

//Terminal related
#define term_screen_count 255 //absolute limit of "screens". DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING, set 'debug' to true and check all available "screens" to update val
#define term_selectible_count 255 //absolute limit of selectible elements. DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING, set 'debug' to true and check all available "screens" selectibles elements to update val
const int term_esc_col_normal = 97; //normal color escape code
const int term_esc_col_disabled = 90; //disabled color escape code
const int term_esc_col_error = 91; //error color escape code
const int term_esc_col_success = 92; //success color escape code
const int term_adc_width = 30; //ADC section width
const int term_footer_buttons_width = 15; //footer button width

struct winsize ws; //terminal size
struct termios term_backup; //original terminal state backup
const int term_adc_vertspacing = 9; //vertical spacing between each horizontal ADC elements
int select_index_current = 0, select_index_last = -1, select_limit = 0; //current element selected, last selected

int term_screen_current = 2, term_screen_last = -1; //start "screen", last screen
bool term_screen_update = false; //"screen" require update

char* term_hint_generic_str[]={
    "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate",
    "Press \e[1m[LEFT]\e[0m,\e[1m[RIGHT]\e[0m to change value, \e[1m[-]\e[0m,\e[1m[+]\e[0m for plus/minus 50",
    "Save new configuration",
    "Reset values to default",
    "Discard current modifications",
    "Return to main screen",
    "Close without saving",
    "Close the program",
};

char* term_hint_i2c_failed_str[]={ //follow i2c_failed_bus, i2c_failed_dev, i2c_failed_sig, i2c_failed bool order
    "Invalid I2C bus",
    "Invalid I2C address",
    "Invalid I2C device signature",
    "I2C reading failed",
};

char* term_hint_main_str[]={
    "", //"Bus to use, change with caution",
    "Address of the device, bus used by default:1",
    "GPIO pin used for interrupt, -1 to disable",
    "Amount of reported digital buttons (excl Dpad)",
    "Enable/disable Dpad report",
    "Change enable ADCs, limits, fuzz, flat values",
    "Discard current modifications (exclude Analog Configuration)",
    "Reset values to default (exclude Analog Configuration)",
};

char* term_hint_adc_str[]={
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to enable or disable",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to switch axis direction",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to set as MIN limit value",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to set as MAX limit value",
    "Press \e[1m[ENTER]\e[0m or \e[1m(A)\e[0m to enable axis swap, apply only on driver",
    "Discard current modifications (exclude Main Settings)",
    "Reset values to default (exclude Main Settings)",
};

char* term_hint_save_str[]={
"Save all parameters including unchanged default values.",
"Warning, can be unsafe.",
"Risk to mess a needed system file if something goes wrong.",
"If you have direct/SSH access, go for following option.",
"File will be saved in program folder:",
"Regardless of selected option, previous file will backup as *FILE*.bak.fpjs",
"Saved successfully",
"Something went wrong, failed to save"
};

typedef struct term_select_struct { //selectible terminal elements data
    struct {int x, y, size;} position; //position pointers
    int type; //0:int, 1:bool, 2:bool(visual toogle), 3:hex
    struct {int min, max; char *ptrchar; bool *ptrbool; int *ptrint; bool force_update;} value;
    struct {int y; char *str;} hint; //hint data
    struct {int y; bool *ptrbool; int *ptrint;} defval; //default data
    bool disabled; //is disabled
} term_select_t;
term_select_t term_select[term_selectible_count] = {0};

typedef struct term_input_struct {bool up, down, left, right, plus, minus, tab, enter, escape;} term_input_t; //terminal key input structure
term_input_t term_input = {0};

typedef struct term_pos_generic_struct { //generic terminal position pointer structure
    int x, y, w; //col, line, width
} term_pos_generic_t;

typedef struct term_pos_string_struct {
    int x, y, w; //col, line, width
    char str[buffer_size]; //char array pointer
} term_pos_string_t;

typedef struct term_pos_button_struct {
    int x, y, w; //col, line, width
    char str[buffer_size]; //char array pointer
    bool *ptrbool; //bool pointer to toogle
    char *ptrhint; //hint char array pointer
    bool disabled; //is disabled
} term_pos_button_t;


//functions prototypes
static void debug_print_binary_int_term(int /*line*/, int /*col*/, int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format at given term position

static double get_time_double(void); //get time in double (seconds)

static void program_get_path(char** /*args*/, char* /*var*/); //get current program path


static void str_trim_whitespace(char** /*ptr*/); //update pointer to skip leading pointer, set first trailing space to null char

static int dtoverlay_parser_search_name(dtoverlay_parser_store_t* /*store*/, unsigned int /*store_size*/, char* /*value*/); //search name into dtoverlay parser store, return index on success, -1 on failure

static int dtoverlay_parser(char* /*filename*/, char* /*dtoverlay_name*/, dtoverlay_parser_store_t* /*store*/, unsigned int /*store_size*/); //parse file that content dtoverlay declaration, e.g /boot/config.txt

static void dtoverlay_generate(char* /*str*/, unsigned int /*strlen*/, char* /*dtoverlay_name*/, dtoverlay_parser_store_t* /*store*/, unsigned int /*store_size*/); //generate text line from given dtoverlay store

static int dtoverlay_save(char* /*filename*/, char* /*dtoverlay_name*/, dtoverlay_parser_store_t* /*store*/, unsigned int /*store_size*/); //save current dtoverlay data to file. return 0 on success, -1 on failure

static int dtoverlay_check(char* /*filename*/, char* /*dtoverlay_name*/); //check dtoverlay file. return detected line number on success, -1 on failure

static int i2c_init(int* /*fd*/, int /*bus*/, int /*addr*/); //open fd for I2C device, check device signature, get device id/version, ADC resolution, current device ADC configuration

static int adc_defuzz(int /*value*/, int /*old_val*/, int /*fuzz*/); //defuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56

static int adc_correct_offset_center(int /*adc_resolution*/, int /*adc_value*/, int /*adc_min*/, int /*adc_max*/, int /*adc_offset*/, int /*flat_in*/, int /*flat_out*/); //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent), direct copy from uhid driver


static void array_fill(char* /*arr*/, int /*size*/, char /*chr*/); //fill array with given character, works with '\0' for full reset, last char set to '\0'

static int array_pad(char* /*arr*/, int /*arr_len*/, int /*size*/, char /*pad*/, int /*align*/); //pad a array with 'pad', 'align': 0:center 1:left 2:right, 'size':final array size, return padding length


static int strcpy_noescape(char* /*dest*/, char* /*src*/, int /*limit*/); //strcpy "clone" that ignore terminal escape code, set dest=src or dest=NULL to only return "noescape" char array length. Current limitations:defined limit of escape code (w/o "\e["). warnings: no size check, broken if badly formated escape, only check for h,l,j,m ending


static void int_rollover(int* /*val*/, int /*min*/, int /*max*/); //rollover int value between (incl) min and max, work both way

static void int_constrain(int* /*val*/, int /*min*/, int /*max*/); //limit int value to given (incl) min and max value

static void term_user_input(term_input_t* /*input*/); //process terminal key inputs
static void term_select_update(term_select_t* /*store*/, int* /*index*/, int* /*index_last*/, int /*index_limit*/, term_input_t* /*input*/, int /*tty_width*/, int /*tty_height*/); //update selectible elements

static void term_screen_main(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/); //main screen:0
static void term_screen_adc(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/); //display adc screen:1
static void term_screen_save(int /*tty_line*/, int /*tty_last_width*/, int /*tty_last_height*/); //save screen:2







