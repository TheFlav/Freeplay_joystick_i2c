/*
* UHID driver file
*/

#pragma once

const char programversion[] = "0.1b"; //program version

//Prototypes
double get_time_double (void); //get time in double (seconds)

static int uhid_create (int /*fd*/); //create uhid device
static void uhid_destroy (int /*fd*/); //close uhid device
static int uhid_write (int /*fd*/, const struct uhid_event* /*ev*/); //write data to uhid device
static int uhid_send_event (int /*fd*/); //send event to uhid device

static int i2c_open (int* /*fd*/, int /*bus*/, int /*addr*/); //open I2C file
static void i2c_close_all (void); //close all I2C file
void adc_data_compute(void); //compute adc max value, flat in/out
static int adc_defuzz(int /*value*/, int /*old_val*/, int /*fuzz*/); //apply fuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56
static int adc_correct_offset_center (int /*adc_resolution*/, int /*adc_value*/, int /*adc_min*/, int /*adc_max*/, int /*adc_offset*/, int /*flat_in*/, int /*flat_out*/); //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent)
static void i2c_poll_joystick (void); //poll data from i2c device

static int file_read (char* /*path*/, char* /*bufferptr*/, int /*buffersize*/); //read file
static int file_write (char* /*path*/, char* /*content*/); //write file
static int logs_write (const char* /*format*/, ...); //write to log, return chars written or -1 on error
static int folder_create (char* /*path*/, int /*rights*/, int /*uid*/, int /*gid*/); //create folder(s), set rights/uid/gui if not -1. Return number of folder created, -errno on error
static void shm_init (bool /*first*/); //init shm related things, folders and files creation
static void shm_update (void); //update registers/files linked to shm things
static void shm_close (void); //close shm related things

static void program_close (void); //regroup all close functs
void program_get_path (char** /*args*/, char* /*var*/); //get current program path
static void tty_signal_handler (int /*sig*/); //handle signal func
static int in_array_int16 (int16_t* /*arr*/, int16_t /*value*/, int /*arr_size*/); //search in value in int16 array, return index or -1 on failure
void int_rollover(int* /*val*/, int /*min*/, int /*max*/); //rollover int value between (incl) min and max, work both way
void int_constrain(int* /*val*/, int /*min*/, int /*max*/); //limit int value to given (incl) min and max value
static void debug_print_binary_int (int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format

int mcu_check_manufacturer(void); //check device manufacturer, fill signature,id,version, return 0 on success, -1 on wrong manufacturer, -2 on i2c error
int mcu_update_config0(void); //read/update config0 register, return 0 on success, -1 on error
int mcu_init_adc(void); //init adc data, return 0 on success, -1 on resolution read fail, -2 on adc conf
void uhid_joystick_swap(void); //uhid joystick/axis swap

void vars_main_default(void); //reset all main config vars to default
void vars_adc_default(void); //reset all adc config vars to default



#ifdef USE_PIGPIO_IRQ
static void gpio_callback(int /*gpio*/, int /*level*/, uint32_t /*tick*/);
#elif defined(USE_WIRINGPI_IRQ)
static void mcu_irq_handler(void);
#endif


bool debug = def_debug; //enable debug output
bool debug_adv = def_debug_adv; //enable advanced debug output, benchmark
bool i2c_poll_rate_disable = def_i2c_poll_rate_disable; //allow full throttle update if true

int i2c_bus = def_i2c_bus; //I2C bus
int i2c_addr = def_i2c_addr; //main MCU I2C address
int i2c_addr_sec = def_i2c_addr_sec; //secondary MCU I2C address
int i2c_addr_adc[] = {def_i2c_addr_adc0, def_i2c_addr_adc1, def_i2c_addr_adc2, def_i2c_addr_adc3}; //external ADC I2C address, set to 0xFF to disable

int irq_gpio = def_irq_gpio; //gpio pin used for IRQ, limited to 31 for pigpio, set to -1 to disable

int i2c_poll_rate = def_i2c_poll_rate; //Driver pollrate in hz
int i2c_adc_poll = def_i2c_adc_poll; //poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on

int digital_debounce = def_digital_debounce; //debounce filtering to mitigate possible pad false contact, default:5, max:7, 0 to disable

bool uhid_js_swap = def_uhid_js_swap; //swap left and right joystick
bool uhid_js_swap_axis[] = {def_uhid_js0_swap_axis, def_uhid_js1_swap_axis}; //swap joystick XY axis














//IRQ
bool irq_enable = true; //is set during runtime, do not edit

//debug
bool i2c_disabled = false, uhid_disabled = false;

//Program related
char program_path[PATH_MAX] = {'\0'}; //full path to this program
bool kill_resquested = false, already_killed = false; //allow clean close
double program_start_time = 0.;
bool diag_mode = false, diag_mode_init = false, cfg_save = false;

//UHID related
int uhid_fd = -1;
bool mcu_js_enable[] = {def_js0_enable,def_js1_enable}; //used only to set adc register on device
bool mcu_js_used[] = {false,false}; //set during runtime
bool external_js_used[] = {false,false}; //set during runtime

int mcu_input_map_size = sizeof(mcu_input_map)/sizeof(mcu_input_map[0]);
//int16_t mcu_nINT_shared = BTN_Z; //button shared with nInt phisical pin, set to 0x8000 (-32768) to disable

struct gamepad_report_t {
	int8_t hat0;
	uint8_t buttons7to0;
	uint8_t buttons15to8;
	uint8_t buttonsmisc;
	uint16_t left_x;
	uint16_t left_y;
	uint16_t right_x;
	uint16_t right_y;
} gamepad_report = {0,0,0,0,0x7FFF,0x7FFF,0x7FFF,0x7FFF}, gamepad_report_prev;
uint16_t* js_values[4] = {&gamepad_report.left_x, &gamepad_report.left_y, &gamepad_report.right_x, &gamepad_report.right_y};


//I2C related
int i2c_fd = -1, i2c_fd_sec = -1, i2c_adc_fd[] = {-1, -1, -1,- 1};
const int i2c_errors_report = 10; //error count before report to tty
int i2c_errors_count = 0; //errors count that happen in a row
int i2c_last_error = 0; //last detected error
unsigned long i2c_allerrors_count = 0;
uint8_t i2c_dev_sig = 0, i2c_dev_id=0, i2c_dev_minor=0; //device device signature, id, version
uint8_t i2c_mcu_register_adc_conf = offsetof(struct i2c_joystick_register_struct, adc_conf_bits) / sizeof(uint8_t)/*0*/; //defined at runtime, based on i2c_joystick_registers, adc_conf_bits
uint8_t i2c_mcu_register_adc_res = offsetof(struct i2c_joystick_register_struct, adc_res) / sizeof(uint8_t)/*0*/; //defined at runtime, based on i2c_joystick_registers, adc_res
uint8_t i2c_mcu_register_config0 = offsetof(struct i2c_joystick_register_struct, config0) / sizeof(uint8_t)/*0*/; //defined at runtime, based on i2c_joystick_registers, config0

//const int input_regs_count = input_registers_count;
//const int input_regs_size = input_registers_size;

//MCU config
typedef union { //config0 regiter bitfield structure
    struct {uint8_t debounce_level:3, unused3:1, unused4:1, unused5:1, unused6:1, unused7:1;} vals;
    uint8_t bits;
} mcu_config0_t;
mcu_config0_t mcu_config0 = {0};

//ADC related
bool adc_firstrun = true;

typedef struct adc_data_struct_t { //adc readed data structure
	int raw, raw_prev, raw_min, raw_max; //store raw values for min-max report
	unsigned char res; unsigned int res_limit; //adc resolution/limit set during runtime
	int value, min, max, offset; //current value, min/max limits, computed offset
	int fuzz, flat_in, flat_out; //fuzz, inside/outside flat
	int flat_in_comp, flat_out_comp; //computed recurring values
	bool reversed, autocenter; //reverse reading, autocenter: check adc value once to set as offset
} adc_data_t;
adc_data_t adc_params[4] = {
	{-1,-1,INT_MAX,INT_MIN, def_adc0_res,0xFF, 0x7FFF,def_adc0_min,def_adc0_max,0, def_adc0_fuzz,def_adc0_flat,def_adc0_flat, 0,0, def_adc0_reversed,def_adc0_autocenter,},
	{-1,-1,INT_MAX,INT_MIN, def_adc1_res,0xFF, 0x7FFF,def_adc1_min,def_adc1_max,0, def_adc1_fuzz,def_adc1_flat,def_adc1_flat, 0,0, def_adc1_reversed,def_adc1_autocenter,},
	{-1,-1,INT_MAX,INT_MIN, def_adc2_res,0xFF, 0x7FFF,def_adc2_min,def_adc2_max,0, def_adc2_fuzz,def_adc2_flat,def_adc2_flat, 0,0, def_adc2_reversed,def_adc2_autocenter,},
	{-1,-1,INT_MAX,INT_MIN, def_adc3_res,0xFF, 0x7FFF,def_adc3_min,def_adc3_max,0, def_adc3_fuzz,def_adc3_flat,def_adc3_flat, 0,0, def_adc3_reversed,def_adc3_autocenter,},
};
adc_data_t* adc_params_ptr = adc_params;

int adc_map[4] = {0,1,2,3}; //used to swap axis/joystick

//Pollrate related
double i2c_poll_rate_time = 1.; //poll interval in sec
const double i2c_poll_duration_warn = 0.15; //warning if loop duration over this
double i2c_poll_duration = 0., uhid_write_duration = 0., poll_clock_start = 0., poll_benchmark_clock_start = -1.;
long poll_benchmark_loop = 0;
int poll_stress_loop = 0, i2c_adc_poll_loop = 0;


//SHM related
FILE *logs_fh;
bool shm_enable = false; //set during runtime
double shm_clock_start = -1., shm_update_interval = 0.25; //sec
//#define USE_SHM_REGISTERS 1

#ifdef USE_SHM_REGISTERS
	struct shm_vars_t {
		char path [PATH_MAX];
		char file [256];
		bool rw; //true: read/write, false:read only
		int* i2c_fd;
		int i2c_register;
		int* ptr; //pointer to var
	} shm_vars[] = {
		{
			"", "config_backlight", true, &i2c_fd_sec,
			offsetof(struct i2c_secondary_address_register_struct, config_backlight) / sizeof(i2c_secondary_registers.config_backlight),
			(int*)&(i2c_secondary_registers.config_backlight)
		},
		/*{
			"", "poweroff_control", false, &i2c_fd_sec,
			offsetof(struct i2c_secondary_address_register_struct, poweroff_control) / sizeof(i2c_secondary_registers.poweroff_control),
			(int*)&(i2c_secondary_registers.poweroff_control)
		},*/
	};
	const unsigned int shm_vars_arr_size = sizeof(shm_vars) / sizeof(shm_vars[0]); //shm array size
#endif

//Config related vars
const char debug_desc[] = "Enable debug outputs (0:disable, 1:enable).";
const char debug_adv_desc[] = "Enable debug outputs (0:disable, 1:enable).";
const char i2c_poll_rate_disable_desc[] = "Disable pollrate limitation (0:disable, 1:enable).";

const char pollrate_desc[] = "Driver pollrate (in hz), avoid going over 1000, default:125.";
const char adc_pollrate_desc[] = "ADC pollrate (every given poll loop), default:1.";

const char debounce_desc[] = "Debounce filtering to mitigate possible pad false contact, default:5, max:7 (0 to disable)";

const char js0_enable_desc[] = "Enable MCU joystick 0 ADC0-1 (i2c_address_adc0-1 will be ignored), please check the documentation, does require specific MCU configuration.";
const char js1_enable_desc[] = "Enable MCU joystick 1 ADC2-3 (i2c_address_adc2-3 will be ignored), please check the documentation, does require specific MCU configuration.";
const char js_swap_desc[] = "Swap joystick 0/1 reports (0:disable, 1:enable)";
const char js0_swap_axis_desc[] = "Swap joystick 0 XY axis reports (0:disable, 1:enable)";
const char js1_swap_axis_desc[] = "Swap joystick 1 XY axis reports (0:disable, 1:enable)";

const char i2c_bus_desc[] = "I2C bus to use, default:1.";
const char i2c_addr_desc[] = "Main I2C address, default:0x30.";
const char i2c_addr_sec_desc[] = "Secondary I2C address for device identification, default:0x40.";
const char i2c_addr_adc0_desc[] = "I2C address of ADC0 (0xFF to disable).";
const char i2c_addr_adc1_desc[] = "I2C address of ADC1 (0xFF to disable), needs to be set along i2c_address_adc0.";
const char i2c_addr_adc2_desc[] = "I2C address of ADC2 (0xFF to disable).";
const char i2c_addr_adc3_desc[] = "I2C address of ADC3 (0xFF to disable), needs to be set along i2c_address_adc2.";
const char irq_gpio_desc[] = "GPIO pin to use for interrupt, default:40 (-1 to disable)."; //gpio pin used for irq, limited to 31 for pigpio, set to -1 to disable

const char adc_res_desc[] = "External ADC resolution in bits, ignored if using MCU ADC.";
const char adc_min_desc[] = "Lowest ADC output limit (>= 0).";
const char adc_max_desc[] = "Highest ADC output limit (<= 65354).";
const char adc_fuzz_desc[] = "Fuzz value to smooth ADC value, default:24.";
const char adc_flat_desc[] = "Center deadzone (in percent).";
const char adc_flat_outside_desc[] = "Outside deadzone (in percent).";
const char adc_reversed_desc[] = "Reverse axis output (0:disable, 1:enable).";
const char adc_autocenter_desc[] = "Autodetect physical center (0:disable, 1:enable). Important: if enable, leave device alone during boot process to avoid messing detection.";

cfg_vars_t cfg_vars[] = {
	{"debug", debug_desc, 4, &debug},
	{"debug_adv", debug_adv_desc, 4, &debug_adv},
	{"disable_pollrate", i2c_poll_rate_disable_desc, 4, &i2c_poll_rate_disable},

	{"\npollrate", pollrate_desc, 0, &i2c_poll_rate},
	{"adc_pollrate", adc_pollrate_desc, 0, &i2c_adc_poll},

	{"\ni2c_bus", i2c_bus_desc, 0, &i2c_bus},
	{"i2c_address", i2c_addr_desc, 6, &i2c_addr},
	{"i2c_address_sec", i2c_addr_sec_desc, 6, &i2c_addr_sec},
	{"i2c_address_adc0", i2c_addr_adc0_desc, 6, &i2c_addr_adc[0]},
	{"i2c_address_adc1", i2c_addr_adc1_desc, 6, &i2c_addr_adc[1]},
	{"i2c_address_adc2", i2c_addr_adc2_desc, 6, &i2c_addr_adc[2]},
	{"i2c_address_adc3", i2c_addr_adc3_desc, 6, &i2c_addr_adc[3]},
	{"i2c_irq", irq_gpio_desc, 0, &irq_gpio},

	{"\ndigital_debounce", debounce_desc, 0, &digital_debounce},

	{"\nmcu_js0_enable", js0_enable_desc, 4, &mcu_js_enable[0]},
	{"mcu_js1_enable", js1_enable_desc, 4, &mcu_js_enable[1]},

	{"\njoystick_swap", js_swap_desc, 4, &uhid_js_swap},

	{"\nadc0_res", adc_res_desc, 0, &adc_params[0].res},
	{"adc0_min", adc_min_desc, 0, &adc_params[0].min},
	{"adc0_max", adc_max_desc, 0, &adc_params[0].max},
	{"adc0_fuzz", adc_fuzz_desc, 0, &adc_params[0].fuzz},
	{"adc0_flat", adc_flat_desc, 0, &adc_params[0].flat_in},
	{"adc0_flat_outside", adc_flat_outside_desc, 0, &adc_params[0].flat_out},
	{"adc0_reversed", adc_reversed_desc, 4,  &adc_params[0].reversed},
	{"adc0_autocenter", adc_autocenter_desc, 4, &adc_params[0].autocenter},

	{"\nadc1_res", adc_res_desc, 0, &adc_params[1].res},
	{"adc1_min", adc_min_desc, 0, &adc_params[1].min},
	{"adc1_max", adc_max_desc, 0, &adc_params[1].max},
	{"adc1_fuzz", adc_fuzz_desc, 0, &adc_params[1].fuzz},
	{"adc1_flat", adc_flat_desc, 0, &adc_params[1].flat_in},
	{"adc1_flat_outside", adc_flat_outside_desc, 0, &adc_params[1].flat_out},
	{"adc1_reversed", adc_reversed_desc, 4,  &adc_params[1].reversed},
	{"adc1_autocenter", adc_autocenter_desc, 4, &adc_params[1].autocenter},

	{"\njs0_axis_swap", js0_swap_axis_desc, 4, &uhid_js_swap_axis[0]},

	{"\nadc2_res", adc_res_desc, 0, &adc_params[2].res},
	{"adc2_min", adc_min_desc, 0, &adc_params[2].min},
	{"adc2_max", adc_max_desc, 0, &adc_params[2].max},
	{"adc2_fuzz", adc_fuzz_desc, 0, &adc_params[2].fuzz},
	{"adc2_flat", adc_flat_desc, 0, &adc_params[2].flat_in},
	{"adc2_flat_outside", adc_flat_outside_desc, 0, &adc_params[2].flat_out},
	{"adc2_reversed", adc_reversed_desc, 4,  &adc_params[2].reversed},
	{"adc2_autocenter", adc_autocenter_desc, 4, &adc_params[2].autocenter},

	{"\nadc3_res", adc_res_desc, 0, &adc_params[3].res},
	{"adc3_min", adc_min_desc, 0, &adc_params[3].min},
	{"adc3_max", adc_max_desc, 0, &adc_params[3].max},
	{"adc3_fuzz", adc_fuzz_desc, 0, &adc_params[3].fuzz},
	{"adc3_flat", adc_flat_desc, 0, &adc_params[3].flat_in},
	{"adc3_flat_outside", adc_flat_outside_desc, 0, &adc_params[3].flat_out},
	{"adc3_reversed", adc_reversed_desc, 4,  &adc_params[3].reversed},
	{"adc3_autocenter", adc_autocenter_desc, 4, &adc_params[3].autocenter},

	{"\njs1_axis_swap", js1_swap_axis_desc, 4, &uhid_js_swap_axis[1]},
};

const unsigned int cfg_vars_arr_size = sizeof(cfg_vars) / sizeof(cfg_vars[0]); //config array size

//terminal related
struct winsize ws; //terminal size
struct termios term_backup; //original terminal state backup
term_input_t term_input = {0};

int term_esc_col_normal, term_esc_col_disabled, term_esc_col_error, term_esc_col_success; //color escape codes
int term_adc_width, term_adc_vertspacing; //ADC section width, vertical spacing between each horizontal ADC elements
int term_footer_buttons_width; //footer button width

int select_index_current, select_index_last; //current element selected, last selected

int term_screen_current, term_screen_last; //start "screen", last screen
bool term_screen_update; //"screen" require update

bool debug_backup, debug_adv_backup; //backup debug bools


