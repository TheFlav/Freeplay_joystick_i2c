/*
* FreeplayTech UHID gamepad driver
* Main header
*/

#pragma once

const char program_version[] = "0.1g"; //program version
const char dev_webpage[] = "https://github.com/TheFlav/Freeplay_joystick_i2c"; //dev website

//Prototypes
double get_time_double(void); //get time in double (seconds), takes around 82 microseconds to run

int i2c_check_bus(int /*bus*/, int* /*bus_found*/); //check I2C bus, return errno, set bus_found to NULL to disable bus searching
int i2c_open_dev(int* /*fd*/, int /*bus*/, int /*addr*/); //open I2C device, return 0 on success, -1:bus, -2:addr, -3:generic error
void i2c_close_all(void); //close all I2C file
void adc_data_compute(int /*adc_index*/); //compute adc max value, flat in/out, offset
static int adc_defuzz(int /*value*/, int /*old_val*/, int /*fuzz*/); //apply fuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56
int adc_correct_offset_center (int /*adc_resolution*/, int /*adc_value*/, int /*adc_min*/, int /*adc_max*/, int /*adc_offset*/, int /*flat_in*/, int /*flat_out*/); //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent)
void i2c_poll_joystick (bool /*force_update*/); //poll data from i2c device

static void shm_init (bool /*first*/); //init shm related things, folders and files creation
static void shm_close (void); //close shm related things
static int logs_write (const char* /*format*/, ...); //write to log, return chars written or -1 on error
static int file_read(char* /*path*/, char* /*bufferptr*/, int /*buffersize*/); //read file
static int file_write(char* /*path*/, char* /*content*/); //write file
static int folder_create (char* /*path*/, int /*rights*/, int /*uid*/, int /*gid*/); //create folder(s), set rights/uid/gui if not -1. Return number of folder created, -errno on error

static void program_close (void); //regroup all close functs
static void program_get_path (char** /*args*/, char* /*path*/, char* /*program*/); //get current program path
static void program_usage(void); //display help
static void tty_signal_handler (int /*sig*/); //handle signal func

void int_rollover(int* /*val*/, int /*min*/, int /*max*/); //rollover int value between (incl) min and max, work both way
int int_constrain(int* /*val*/, int /*min*/, int /*max*/); //limit int value to given (incl) min and max value, return 0 if val within min and max, -1 under min, 1 over max
int int_digit_count(int /*num*/); //number of digit of a integer, negative sign is consider as a digit
int in_array_int16(int16_t* /*arr*/, int16_t /*value*/, int /*arr_size*/); //search in value in int16 array, return index or -1 on failure

static uint8_t bit_extract_uint8(uint8_t /*value*/, uint8_t /*bit_pos*/, uint8_t /*bit_count*/); //warning, no security
static uint8_t bit_clear_uint8(uint8_t /*value*/, uint8_t /*bit_pos*/, uint8_t /*bit_count*/); //warning, no security
static char* int_bin_chararray(int /*val*/, int /*bits*/, char* /*var*/); //int to binary format char array

//static void debug_print_binary_int (int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format
//static void debug_print_binary_int_term (int /*line*/, int /*col*/, int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format at given term position

int mcu_check_manufacturer(void); //check device manufacturer, fill signature,id,version, return 0 on success, -1 on wrong manufacturer, -2 on i2c error
int mcu_update_register(int* /*fd*/, uint8_t /*reg*/, uint8_t /*value*/, bool /*check*/); //update and check register, return 0 on success, -1 on error
int mcu_update_config0(void); //read/update config0 register, return 0 on success, -1 on error
int init_adc(void); //init adc data, return 0 on success, -1 on resolution read fail, -2 on adc conf
int mcu_search_i2c_addr(int /*bus*/, int* /*addr_main*/, int* /*addr_sec*/); //search mcu on given i2c bus, return -1 on failure, 0 on success

int uhid_send_event(int /*fd*/); //send event to uhid device, send to /dev/null in diag program

bool io_fd_valid(int /*fd*/); //check if a file descriptor is valid

#ifndef ALLOW_MCU_SEC_I2C
    #undef USE_SHM_REGISTERS //can't use shm register "bridge" with MCU secondary feature
#else
    #ifndef DIAG_PROGRAM
        int mcu_update_power_control(void); //read/update power_control register, return 0 on success, -1 on error
        int mcu_update_battery_capacity(void); //read/update battery_capacity register, return 0 on success, -1 on error
    #endif
#endif

#ifndef DIAG_PROGRAM
    static int uhid_create(int /*fd*/); //create uhid device
    static void uhid_destroy(int /*fd*/); //close uhid device
    static int uhid_write(int /*fd*/, const struct uhid_event* /*ev*/); //write data to uhid device
    static void shm_update(void); //update registers/files linked to shm things
    #define diag_mode false
#else
    extern int program_diag_mode(void); //main diag mode function
    #undef USE_WIRINGPI
    #undef USE_GPIOD
    #undef USE_SHM_REGISTERS
    #define diag_mode true
#endif

#ifndef MULTI_INSTANCES
    static int program_instances_count(char* /*path*/, char* /*program*/); //scan /proc/ to check if program is already running, return number of instances found
#endif

//debug
bool debug = def_debug; //enable debug output
bool debug_adv = def_debug_adv; //enable advanced debug output, benchmark
bool i2c_disabled = false, uhid_disabled = false;
bool quiet_mode = false;

//IRQ
bool irq_enable = false; //is set during runtime, do not edit
int irq_gpio = def_irq_gpio; //gpio pin used for IRQ, limited to 31 for pigpio, set to -1 to disable

//Program related
char program_path[PATH_MAX] = {'\0'}, program_name[PATH_MAX] = {'\0'}; //full path to this program
char pid_path[PATH_MAX] = {'\0'}; //full path to program pid file
bool kill_requested = false, already_killed = false; //allow clean close
double program_start_time = 0.;
bool diag_mode_init = false; //used mainly to disable print_stderr and print_stdout output in diag mode
bool diag_first_run = false; //running in "first run" mode, used to ease ADCs setup
bool diag_postmessagetest = false; //only output "first run" post message and close
bool diag_noinputs = false; //disable mcu inputs in menus
bool allow_diag_run = true; //avoid diag loop at driver start
bool driver_lock = false; //lock driver if shm_path/status set to 2, stop i2c poll and uhid update
//bool cfg_deleted = false; //program arg prohibe create of a config file but one was created

//UHID related
int uhid_fd = -1;
int mcu_input_map_size = sizeof(mcu_input_map)/sizeof(mcu_input_map[0]);
uint32_t mcu_input_digital_prev = 0xFFFFFFFF; //last digital inputs
int uhid_device_id = def_uhid_device_id; //number added after reported name, mainly used if running multiple drivers
int hid_vendor = def_hid_vendor; //HID vendor ID
int hid_product = def_hid_product; //HID product ID

struct gamepad_report_t {
    int8_t hat0;
    uint8_t buttons7to0;
    uint8_t buttons15to8;
    uint8_t buttonsmisc;
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
} gamepad_report = {0,0,0,0,0x7FFF,0x7FFF,0x7FFF,0x7FFF}/*, gamepad_report_prev*/;
uint16_t* js_values[4] = {&gamepad_report.left_x, &gamepad_report.left_y, &gamepad_report.right_x, &gamepad_report.right_y};
char* js_axis_names[5] = {"none", "X1", "Y1", "X2", "Y2"}; //joystick axis names, virtually start at index -1

//I2C related
int i2c_bus = def_i2c_bus; //I2C bus
bool mcu_search = def_mcu_search; //enable search of proper MCU address if provided one fails.
int mcu_addr = def_mcu_addr; //main MCU I2C address
int mcu_fd = -1;
bool adc_fd_valid[4] = {0};
const int i2c_errors_report = 10; //error count before report to tty
int i2c_errors_count = 0; //errors count that happen in a row
int i2c_last_error = 0; //last detected error
unsigned long i2c_allerrors_count = 0;

uint8_t mcu_signature = 0, mcu_id=0, mcu_version=0; //device device signature, id, version
struct i2c_joystick_register_struct i2c_joystick_registers;
//const uint8_t mcu_i2c_register_adc0 = offsetof(struct i2c_joystick_register_struct, a0_msb) / sizeof(uint8_t); //defined at runtime, based on i2c_joystick_registers, a0_msb
//const uint8_t mcu_i2c_register_adc2 = offsetof(struct i2c_joystick_register_struct, a2_msb) / sizeof(uint8_t); //defined at runtime, based on i2c_joystick_registers, a2_msb
const uint8_t mcu_i2c_register_adc_conf = offsetof(struct i2c_joystick_register_struct, adc_conf_bits) / sizeof(uint8_t); //defined at runtime, based on i2c_joystick_registers, adc_conf_bits
const uint8_t mcu_i2c_register_adc_res = offsetof(struct i2c_joystick_register_struct, adc_res) / sizeof(uint8_t); //defined at runtime, based on i2c_joystick_registers, adc_res
const uint8_t mcu_i2c_register_config0 = offsetof(struct i2c_joystick_register_struct, config0) / sizeof(uint8_t); //defined at runtime, based on i2c_joystick_registers, config0

//I2C secondary related
#ifdef ALLOW_MCU_SEC_I2C
    int mcu_addr_sec = def_mcu_addr_sec; //secondary MCU I2C address
    int mcu_fd_sec = -1;
    struct i2c_secondary_address_register_struct i2c_secondary_registers;
    const uint8_t mcu_sec_register_backlight = offsetof(struct i2c_secondary_address_register_struct, config_backlight) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, config_backlight
    const uint8_t mcu_sec_register_backlight_max = offsetof(struct i2c_secondary_address_register_struct, backlight_max) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, backlight_max
    const uint8_t mcu_sec_register_write_protect = offsetof(struct i2c_secondary_address_register_struct, write_protect) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, write_protect
    const uint8_t mcu_sec_register_joystick_i2c_addr = offsetof(struct i2c_secondary_address_register_struct, joystick_i2c_addr) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, joystick_i2c_addr
    const uint8_t mcu_sec_register_secondary_i2c_addr = offsetof(struct i2c_secondary_address_register_struct, secondary_i2c_addr) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, secondary_i2c_addr
    const uint8_t mcu_sec_register_status_led_control = offsetof(struct i2c_secondary_address_register_struct, status_led_control) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, status_led_control
    const uint8_t mcu_sec_register_power_control = offsetof(struct i2c_secondary_address_register_struct, power_control) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, power_control
    const uint8_t mcu_sec_register_battery_capacity = offsetof(struct i2c_secondary_address_register_struct, battery_capacity) / sizeof(uint8_t); //defined at runtime, based on i2c_secondary_registers, battery_capacity
#endif

//MCU config
int digital_debounce = def_digital_debounce; //debounce filtering to mitigate possible pad false contact, default:5, max:7, 0 to disable
bool mcu_adc_enabled[4] = {0}; //adc enabled on mcu, set during runtime
bool mcu_adc_read[2] = {0}; //read mcu adc0-1/2-3 during poll
mcu_config0_t mcu_config0 = {0};
bool battery_gpio_enable = false;
#ifdef ALLOW_MCU_SEC_I2C
    int mcu_backlight = 255; //current backlight level, set during runtime
    int mcu_backlight_steps = 255; //maximum amount of backlight steps, set during runtime

    double battery_clock_start = -1.; //set during runtime, used for mcu battery interval update
    int battery_interval = def_battery_interval; //MCU battery related stuff update interval in sec
    int battery_report_type = def_battery_report_type; //todo
    int lowbattery_gpio = def_lowbattery_gpio; //low battery gpio pin, set to -1 to disable
    bool lowbattery_gpio_invert = def_lowbattery_gpio_invert; //invert low battery gpio pin signal
    //mcu_power_control_t mcu_power_control = {0}; //mcu power_control register bitfield structure
#endif

//ADC related
bool adc_firstrun = true; //trigger adc data compute part when joystick poll

adc_data_t adc_params[4] = {
    {-1,-1,INT_MAX,INT_MIN, def_adc0_res,0xFF, 0x7FFF,def_adc0_min,def_adc0_max,0, def_adc0_fuzz,def_adc0_flat,def_adc0_flat, 0,0, def_adc0_enabled,def_adc0_reversed,def_adc0_autocenter},
    {-1,-1,INT_MAX,INT_MIN, def_adc1_res,0xFF, 0x7FFF,def_adc1_min,def_adc1_max,0, def_adc1_fuzz,def_adc1_flat,def_adc1_flat, 0,0, def_adc1_enabled,def_adc1_reversed,def_adc1_autocenter},
    {-1,-1,INT_MAX,INT_MIN, def_adc2_res,0xFF, 0x7FFF,def_adc2_min,def_adc2_max,0, def_adc2_fuzz,def_adc2_flat,def_adc2_flat, 0,0, def_adc2_enabled,def_adc2_reversed,def_adc2_autocenter},
    {-1,-1,INT_MAX,INT_MIN, def_adc3_res,0xFF, 0x7FFF,def_adc3_min,def_adc3_max,0, def_adc3_fuzz,def_adc3_flat,def_adc3_flat, 0,0, def_adc3_enabled,def_adc3_reversed,def_adc3_autocenter},
};

int adc_map[4] = {def_adc0_map, def_adc1_map, def_adc2_map, def_adc3_map}; int adc_map_size = 4; //adc to joystick maps for uhid report
int *adc_map_ptr[]={adc_map, &adc_map_size};

//Pollrate related
bool i2c_poll_rate_disable = false; //allow full throttle update if true, set during runtime if i2c_poll_rate set to 0
int i2c_poll_rate = def_i2c_poll_rate; //Driver pollrate in hz
int i2c_adc_poll = def_i2c_adc_poll; //poll adc every given poll loops. <=1 for every loop, 2 to poll every 2 poll loop and so on
double i2c_poll_rate_time = 1.; //poll interval in sec
const double i2c_poll_duration_warn = 0.15; //warning if loop duration over this
double i2c_poll_duration = 0., uhid_write_duration = 0., poll_clock_start = 0., poll_benchmark_clock_start = -1.;
long poll_benchmark_loop = 0;
int poll_stress_loop = 0, i2c_adc_poll_loop = 0;

//SHM related
FILE *logs_fh;
bool shm_enable = false; //set during runtime
char shm_fullpath[PATH_MAX] = {'\0'}; //path to shm incl uhid id
char* shm_status_path_ptr; //pointer to shm status path var

#ifndef DIAG_PROGRAM
    int shm_status = 1; //shm_path/status content
    double shm_clock_start = -1., shm_update_interval = 0.25; //sec
#endif

#ifdef USE_SHM_REGISTERS
    uint8_t power_control_low_batt_mode = 0; //mcu sec power_control low_batt_mode field value
    uint8_t power_control_lcd_dimming_mode = 0; //mcu sec power_control lcd_dimming_mode field value
    uint8_t power_control_lcd_sleep_mode = 0; //mcu sec power_control lcd_sleep_mode field value

    typedef struct shm_vars_protected_reg_struct {
        uint8_t reg; //register
        uint8_t enable, disable; //value to set to protect and unprotect
    } shm_vars_protected_reg_t;

    shm_vars_protected_reg_t shm_mcu_sec_write_protect = {
        .reg=mcu_sec_register_write_protect,
        .enable=mcu_write_protect_enable,
        .disable=mcu_write_protect_disable,
    };

    struct shm_vars_t {
        int* fd; //i2c fd
        uint8_t reg; //register
        bool reg_rw; //register allow read/write, is write protected
        shm_vars_protected_reg_t* reg_wp;
        struct {
            char* filename;
            struct timespec mtime;
            uint8_t var_back; //backup to track modification
            uint8_t* var_ptr; //pointer to current "bitfield" value
            uint8_t bit_pos, bit_count; //"bitfield" bit start/amount, bit_count to 0 for no "bitfield"
        } fields[8];
        uint8_t fields_count;
    } shm_vars[] = {
        { //0:mcu sec config_backlight
            .fd=&mcu_fd_sec, .reg=mcu_sec_register_backlight,
            .reg_rw=true, .reg_wp=&shm_mcu_sec_write_protect,
            .fields={{.filename="backlight", .var_ptr=(uint8_t*)&(i2c_secondary_registers.config_backlight)}},
            .fields_count=1,
        },

        { //1:mcu sec backlight_max
            .fd=&mcu_fd_sec, .reg=mcu_sec_register_backlight_max,
            .fields={{.filename="backlight_max", .var_ptr=(uint8_t*)&(i2c_secondary_registers.backlight_max)}},
            .fields_count=1,
        },

        { //2:mcu sec status_led_control
            .fd=&mcu_fd_sec, .reg=mcu_sec_register_status_led_control,
            .reg_rw=true, .reg_wp=&shm_mcu_sec_write_protect,
            .fields={{.filename="status_led", .var_ptr=(uint8_t*)&(i2c_secondary_registers.status_led_control)}},
            .fields_count=1,
        },

        { //3:mcu sec battery_capacity
            .fd=&mcu_fd_sec, .reg=mcu_sec_register_battery_capacity,
            .reg_rw=true, .reg_wp=&shm_mcu_sec_write_protect,
            .fields={{.filename="battery_capacity", .var_ptr=(uint8_t*)&(i2c_secondary_registers.battery_capacity)}},
            .fields_count=1,
        },

        { //4:mcu sec power_control
            .fd=&mcu_fd_sec, .reg=mcu_sec_register_power_control,
            .reg_rw=true, .reg_wp=&shm_mcu_sec_write_protect,
            .fields={
                {.filename="low_batt_mode", .var_ptr=&power_control_low_batt_mode, .bit_pos=power_control_pos_low_batt, .bit_count=1},
                {.filename="lcd_dimming_mode", .var_ptr=&power_control_lcd_dimming_mode, .bit_pos=power_control_pos_lcd_dimming, .bit_count=1},
                {.filename="lcd_sleep_mode", .var_ptr=&power_control_lcd_sleep_mode, .bit_pos=power_control_pos_lcd_sleep, .bit_count=1},
            },
            .fields_count=3,
        },
    };
    const int shm_vars_arr_size = sizeof(shm_vars) / sizeof(shm_vars[0]); //shm array size
#endif

//Config related vars
char config_path[PATH_MAX+11] = {'\0'}; //full path to config file
#ifndef DIAG_PROGRAM
    bool config_reload = false; //config reload trigger via shm_path/status set to 2
    double config_check_start = -1., config_check_interval = 5.; //check config update every 5sec
    struct timespec config_mtime;
#endif

const char debug_desc[] = "Enable debug outputs (0:disable, 1:enable).";
const char debug_adv_desc[] = "Enable debug outputs (0:disable, 1:enable).";

const char pollrate_desc[] = "Driver pollrate (in hz), avoid going over 1000 (0 to disable limit).";
const char adc_pollrate_desc[] = "ADC pollrate (every given poll loop).";

const char debounce_desc[] = "Debounce filtering to mitigate possible pad false contact, max:7 (0 to disable).";

const char i2c_bus_desc[] = "I2C bus to use.";
const char mcu_search_desc[] = "Try to correct wrong I2C bus number and enable search of proper MCU address if provided one fails.";
const char mcu_addr_desc[] = "MCU I2C address.";
#ifdef ALLOW_MCU_SEC_I2C
    const char mcu_addr_sec_desc[] = "MCU Secondary I2C address for additionnal features.";
    const char battery_interval_desc[] = "MCU battery related stuff update interval in sec, require Battery Gauge IC installed (incl. kernel driver) or GPIO low battery pin.";
    const char battery_report_type_desc[] = "TODO.";
    const char lowbattery_gpio_desc[] = "Low battery GPIO pin, -1 to disable.";
    const char lowbattery_gpio_invert_desc[] = "Invert Low battery GPIO signal (0:disable, 1:enable).";
#endif
const char irq_gpio_desc[] = "GPIO pin to use for interrupt, default:40 (-1 to disable)."; //gpio pin used for irq, limited to 31 for pigpio, set to -1 to disable

const char adc_map_desc[] = "ADCs to Joystick axis mapping (-1:disabled axis, 0:X1, 1:Y1, 2:X2, 3:Y2), format: ADC0,ADC1,ADC2,ADC3.";

const char adc_enabled_desc[] = "Enable MCU/External ADC (0:disable, 1:enable).";
#ifdef ALLOW_EXT_ADC
    const char adc_addr_desc[] = "External ADC I2C address, set invalid address (0xFF) to disable.";
    char adc_type_desc[4096] = "External ADC type identifier";
#endif
const char adc_res_desc[] = "External ADC resolution in bits, ignored if using MCU ADC or external ADC defines it.";
const char adc_min_desc[] = "Lowest ADC output limit (>= 0).";
const char adc_max_desc[] = "Highest ADC output limit (<= 65354).";
const char adc_fuzz_desc[] = "Fuzz value to smooth ADC value.";
const char adc_flat_desc[] = "Center deadzone (in percent).";
const char adc_flat_outside_desc[] = "Outside deadzone (in percent).";
const char adc_reversed_desc[] = "Reverse axis output (0:disable, 1:enable).";
const char adc_autocenter_desc[] = "Autodetect physical center (0:disable, 1:enable). Important: if enable, leave device alone during boot process to avoid messing detection.";

const char uhid_device_id_desc[] = "Number added after driver name, mainly used if running multiple drivers.";
const char hid_vendor_desc[] = "HID reported vendor ID.";
const char hid_product_desc[] = "HID reported product ID.";

cfg_vars_t cfg_vars[] = {
    {"debug", debug_desc, 4, &debug},
    {"debug_adv", debug_adv_desc, 4, &debug_adv},

    {"\npollrate", pollrate_desc, 0, &i2c_poll_rate},
    {"adc_pollrate", adc_pollrate_desc, 0, &i2c_adc_poll},
    {"irq_gpio", irq_gpio_desc, 0, &irq_gpio},

    {"\ni2c_bus", i2c_bus_desc, 0, &i2c_bus},
    {"mcu_search", mcu_search_desc, 4, &mcu_search},
    {"mcu_address", mcu_addr_desc, 6, &mcu_addr},
#ifdef ALLOW_MCU_SEC_I2C
    {"mcu_address_sec", mcu_addr_sec_desc, 6, &mcu_addr_sec},

    {"\nbattery_interval", battery_interval_desc, 0, &battery_interval},
    {"battery_report_type", battery_report_type_desc, 0, &battery_report_type},
    {"lowbattery_gpio", lowbattery_gpio_desc, 0, &lowbattery_gpio},
    {"lowbattery_gpio_invert", lowbattery_gpio_invert_desc, 4, &lowbattery_gpio_invert},
#endif

    {"\ndigital_debounce", debounce_desc, 0, &digital_debounce},

    {"\nadc_map", adc_map_desc, 5, &adc_map_ptr},

    {"\nadc0_enabled", adc_enabled_desc, 4, &adc_params[0].enabled},
#ifdef ALLOW_EXT_ADC
    {"adc0_address", adc_addr_desc, 6, &adc_addr[0]},
    {"adc0_type", adc_type_desc, 0, &adc_type[0]},
#endif
    {"adc0_res", adc_res_desc, 0, &adc_params[0].res},
    {"adc0_min", adc_min_desc, 0, &adc_params[0].min},
    {"adc0_max", adc_max_desc, 0, &adc_params[0].max},
    {"adc0_fuzz", adc_fuzz_desc, 0, &adc_params[0].fuzz},
    {"adc0_flat", adc_flat_desc, 0, &adc_params[0].flat_in},
    {"adc0_flat_outside", adc_flat_outside_desc, 0, &adc_params[0].flat_out},
    {"adc0_reversed", adc_reversed_desc, 4,  &adc_params[0].reversed},
    {"adc0_autocenter", adc_autocenter_desc, 4, &adc_params[0].autocenter},

    {"\nadc1_enabled", adc_enabled_desc, 4, &adc_params[1].enabled},
#ifdef ALLOW_EXT_ADC
    {"adc1_address", adc_addr_desc, 6, &adc_addr[1]},
    {"adc1_type", adc_type_desc, 0, &adc_type[1]},
#endif
    {"adc1_res", adc_res_desc, 0, &adc_params[1].res},
    {"adc1_min", adc_min_desc, 0, &adc_params[1].min},
    {"adc1_max", adc_max_desc, 0, &adc_params[1].max},
    {"adc1_fuzz", adc_fuzz_desc, 0, &adc_params[1].fuzz},
    {"adc1_flat", adc_flat_desc, 0, &adc_params[1].flat_in},
    {"adc1_flat_outside", adc_flat_outside_desc, 0, &adc_params[1].flat_out},
    {"adc1_reversed", adc_reversed_desc, 4,  &adc_params[1].reversed},
    {"adc1_autocenter", adc_autocenter_desc, 4, &adc_params[1].autocenter},

    {"\nadc2_enabled", adc_enabled_desc, 4, &adc_params[2].enabled},
#ifdef ALLOW_EXT_ADC
    {"adc2_address", adc_addr_desc, 6, &adc_addr[2]},
    {"adc2_type", adc_type_desc, 0, &adc_type[2]},
#endif
    {"adc2_res", adc_res_desc, 0, &adc_params[2].res},
    {"adc2_min", adc_min_desc, 0, &adc_params[2].min},
    {"adc2_max", adc_max_desc, 0, &adc_params[2].max},
    {"adc2_fuzz", adc_fuzz_desc, 0, &adc_params[2].fuzz},
    {"adc2_flat", adc_flat_desc, 0, &adc_params[2].flat_in},
    {"adc2_flat_outside", adc_flat_outside_desc, 0, &adc_params[2].flat_out},
    {"adc2_reversed", adc_reversed_desc, 4,  &adc_params[2].reversed},
    {"adc2_autocenter", adc_autocenter_desc, 4, &adc_params[2].autocenter},

    {"\nadc3_enabled", adc_enabled_desc, 4, &adc_params[3].enabled},
#ifdef ALLOW_EXT_ADC
    {"adc3_address", adc_addr_desc, 6, &adc_addr[3]},
    {"adc3_type", adc_type_desc, 0, &adc_type[3]},
#endif
    {"adc3_res", adc_res_desc, 0, &adc_params[3].res},
    {"adc3_min", adc_min_desc, 0, &adc_params[3].min},
    {"adc3_max", adc_max_desc, 0, &adc_params[3].max},
    {"adc3_fuzz", adc_fuzz_desc, 0, &adc_params[3].fuzz},
    {"adc3_flat", adc_flat_desc, 0, &adc_params[3].flat_in},
    {"adc3_flat_outside", adc_flat_outside_desc, 0, &adc_params[3].flat_out},
    {"adc3_reversed", adc_reversed_desc, 4,  &adc_params[3].reversed},
    {"adc3_autocenter", adc_autocenter_desc, 4, &adc_params[3].autocenter},

    {"\nuhid_device_id", uhid_device_id_desc, 0, &uhid_device_id},
    {"hid_vendor", hid_vendor_desc, 7, &hid_vendor},
    {"hid_product", hid_product_desc, 7, &hid_product},
};

const unsigned int cfg_vars_arr_size = sizeof(cfg_vars) / sizeof(cfg_vars[0]); //config array size

//terminal related
struct termios term_backup; //original terminal state backup
