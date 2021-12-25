/*
 
 This program sets up a gamepad device in the sytem, interfaces with the attiny i2c device as a gamepad (digital only), and sends HID reports to the system.

 Setup:     sudo apt install libi2c-dev pigpio
 Compile:   gcc -o uhid-i2c-gamepad uhid-i2c-gamepad.c -li2c -lpigpio
 Run:       sudo ./uhid-i2c-gamepad

 Notes:
 	    On the Pi Zero 2 W, I had to use "git clone https://github.com/PinkFreud/WiringPi.git" to get a WiringPi that knew about the Pi02
 
 */


/*  From the attiny code
 
 * PA1 = IO0_0 = UP
 * PA2 = IO0_1 = DOWN
 * PB4 = IO0_2 = LEFT
 * PB5 = IO0_3 = RIGHT
 * PB6 = IO0_4 = BTN_A
 * PB7 = IO0_5 = BTN_B
 * PA6 = IO0_6 = BTN_L2  ifndef USE_ADC2
 * PA7 = IO0_7 = BTN_R2  ifndef USE_ADC3
 *
 * PC0 = IO1_0 = BTN_X
 * PC1 = IO1_1 = BTN_Y
 * PC2 = IO1_2 = BTN_START
 * PC3 = IO1_3 = BTN_SELECT
 * PC4 = IO1_4 = BTN_L
 * PC5 = IO1_5 = BTN_R
 * PB2 = IO1_6 = POWER_BUTTON (Hotkey AKA poweroff_in)   ifndef CONFIG_SERIAL_DEBUG (or can be used for UART TXD0 for debugging)
 * PA5 = IO1_7 = BTN_C ifndef USE_ADC1
 *
 * PB3 =         POWEROFF_OUT
 * PA3 =         PWM Backlight OUT
 *
 */




#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <linux/hid.h>
#include <linux/uhid.h>
#include <stdint.h>

#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <limits.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>


#define REGISTER_ADC_ENABLE	0x08 //turn ON bits here to activate ADC0 - ADC3 (only works if the USE_ADC# are turned on)
#define REGISTER_ADC_RES	0x0B //current ADC resolution

//IRQ specific
int nINT_GPIO = 40; //gpio pin used for irq, limited to 31 for pigpio, set to -1 to disable
#define USE_WIRINGPI_IRQ //use wiringPi for IRQ
//#define USE_PIGPIO_IRQ //or USE_PIGPIO
//or comment out both of the above to poll

#if defined(USE_PIGPIO_IRQ) && defined(USE_WIRINGPI_IRQ)
	#error Cannot do both IRQ styles
#elif defined(USE_WIRINGPI_IRQ)
	#include <wiringPi.h>
#elif defined(USE_PIGPIO_IRQ)
	#include <pigpio.h>
#endif


//Prototypes
static double get_time_double(void); //get time in double (seconds)

static int cstring_in_cstring_array (char** /*arr*/, char* /*value*/, unsigned int /*arrSize*/, bool /*skipNl*/); //search in char array, return index if found, -1 if not
static bool config_save (bool /*reset*/); //save config file
static void config_parse (void); //parse/create program config file

static int uhid_create(int /*fd*/); //create uhid device
static void uhid_destroy(int /*fd*/); //close uhid device
static int uhid_write(int /*fd*/, const struct uhid_event* /*ev*/); //write data to uhid device
static int uhid_send_event(int /*fd*/); //send event to uhid device

static int i2c_open(int /*bus*/, int /*addr*/); //open I2C bus
static void i2c_close(int /*fd*/, int /*addr*/); //close I2C bus
static int adc_correct_offset_center(int /*adc_resolution*/, int /*adc_value*/, int /*adc_min*/, int /*adc_max*/, int /*adc_offsets*/, int /*inside_flat*/, int /*outside_flat*/, int /*index*/); //apply offset center, expand adc range, inside/ouside flat
static void i2c_poll_joystick(void); //poll data from i2c device

#ifdef USE_PIGPIO_IRQ
static void gpio_callback(int /*gpio*/, int /*level*/, uint32_t /*tick*/);
#elif defined(USE_WIRINGPI_IRQ)
static void attiny_irq_handler(void);
#endif

static void tty_signal_handler(int /*sig*/); //handle signal func

static void debug_print_binary_int (int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format


//debug
bool debug = true; //TODO implement
bool debug_adv = true; //advanced debug output, benchmark
bool i2c_poll_rate_disable = true; //allow full throttle update if true

const char debug_desc[] = "Enable debug outputs (0:disable, 1:enable).";
const char debug_adv_desc[] = "Enable debug outputs (0:disable, 1:enable).";
const char i2c_poll_rate_disable_desc[] = "Disable pollrate limitation (0:disable, 1:enable).";


//tty output functions
double program_start_time = 0.;
#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time /*(double)clock()/CLOCKS_PER_SEC*/, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time /*(double)clock()/CLOCKS_PER_SEC*/, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr

//IRQ related
bool irq_enable = false; //is set during runtime, do not edit

//Program related vars
bool kill_resquested = false; //allow clean close

//UHID related vars
const char* uhid_device_name = "Freeplay Gamepad";
int uhid_fd;
bool uhid_js_left_enable = false;
bool uhid_js_right_enable = false;
bool js_enable[] = {false,false}; //used only to set adc register on device

const char js0_enable_desc[] = "Enable joystick 0 (ADC0-1), please check the documentation, does require specific ATTINY configuration.";
const char js1_enable_desc[] = "Enable joystick 1 (ADC2-3), please check the documentation, does require specific ATTINY configuration.";

struct gamepad_report_t {
    int8_t hat_x;
    int8_t hat_y;
    uint8_t buttons7to0;
    uint8_t buttons12to8;
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
} gamepad_report, gamepad_report_prev;


//I2C related vars
int i2c_bus = 1;
int i2c_addr = 0x20, i2c_addr_sec = 0x40;
int i2c_fd = -1, i2c_fd_sec = -1;
const int i2c_errors_report = 10; //error count before report to tty
int i2c_errors_count = 0; //errors count that happen in a row
int i2c_last_error = 0; //last detected error
const uint8_t i2c_magic_sig = 0xED;
uint8_t i2c_dev_major=0, i2c_dev_minor=0;

const char i2c_bus_desc[] = "I2C bus to use, default:1.";
const char i2c_addr_desc[] = "Main I2C address, default:0x20.";
const char i2c_addr_sec_desc[] = "Secondary I2C address for device identification, default:0x40.";
const char nINT_GPIO_desc[] = "GPIO pin to use for interrupt, default:40 (-1 to disable)."; //gpio pin used for irq, limited to 31 for pigpio, set to -1 to disable

struct i2c_register_struct {
	uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
	uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
	uint8_t a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
	uint8_t a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
	uint8_t a1a0_lsb;        // Reg: 0x04 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
	uint8_t a2_msb;          // Reg: 0x05 - ADC2 most significant 8 bits
	uint8_t a3_msb;          // Reg: 0x06 - ADC2 most significant 8 bits
	uint8_t a3a2_lsb;        // Reg: 0x07 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
	uint8_t adc_on_bits;     // Reg: 0x08 - turn ON bits here to activate ADC0 - ADC3 (only works if the USE_ADC# are turned on)
	uint8_t config0;         // Reg: 0x09 - Configuration port 0
	uint8_t config1;         // Reg: 0x0A - Configuration port 0
	uint8_t adc_res;         // Reg: 0x0B - current ADC resolution (maybe settable?)
} i2c_registers;


//ADC related vars
bool adc_firstrun = true;
int adc_res = 16; unsigned int adc_res_limit = INT_MIN+INT_MAX; //limit set in main(), set to absolute extrem here to fill with 1
int adc_min[] = {0,0,0,0}, adc_max[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}; //store axis min-max values
int adc_offsets[] = {0xFFFF/2, 0xFFFF/2, 0xFFFF/2, 0xFFFF/2}; //store axis offset values
bool adc_reversed[] = {false,false,false,false}; //store axis reversed bools
int adc_values_min[] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}, adc_values_max[] = {0,0,0,0}; //store raw axis detected limits during usage for driver exit report
//int adc_mux_map[] = {-1,-1,-1,-1}; //is main client of mux device, mux map
int/*int16_t*/ /*adc_fuzz[] = {16,16,16,16}, */adc_flat[] = {10,10,10,10}, adc_flat_outside[] = {10,10,10,10}; //store axis /*fuzz-*/flat values (in percent)
bool adc_autocenter[] = {false,false,false,false}; //store axis autocenter bools

const char adc_min_desc[] = "Lowest ADC output limit (>= 0).";
const char adc_max_desc[] = "Highest ADC output limit (<= 65354).";
const char adc_flat_desc[] = "Center deadzone (in percent).";
const char adc_flat_outside_desc[] = "Outside deadzone (in percent).";
const char adc_reversed_desc[] = "Reverse axis output (0:disable, 1:enable).";
const char adc_autocenter_desc[] = "Autodetect physical center (0:disable, 1:enable). Important: if enable, leave device alone during boot process to avoid messing detection.";


//Poll rate vars
const int i2c_poll_rate = 125; //poll per sec
const double i2c_poll_rate_time = 1. / i2c_poll_rate; //poll interval in sec
const double i2c_poll_duration_warn = 0.15; //warning if loop duration over this
double i2c_poll_duration = 0.; //poll loop duration
double poll_clock_start = 0., poll_benchmark_clock_start = -1.;
long poll_benchmark_loop = 0;


//Config related vars
const char cfg_filename[] = "config.cfg";
int cfg_version = 0, cfg_version_org = 0;

const char cfg_version_desc[] = "Change value to force resync of configuration file.";


//vars names in config file, IMPORTANT: no space
const char *cfg_vars_name[] = {	"debug", "debug_adv", "disable_pollrate",
								"\ni2c_bus", "i2c_address", "i2c_address_sec", "i2c_irq",
								"\njs0_enable", "js1_enable",
								"\nadc0_min", "adc0_max", "adc0_flat", "adc0_flat_outside", "adc0_reversed", "adc0_autocenter",
								"\nadc1_min", "adc1_max", "adc1_flat", "adc1_flat_outside", "adc1_reversed", "adc1_autocenter",
								"\nadc2_min", "adc2_max", "adc2_flat", "adc2_flat_outside", "adc2_reversed", "adc2_autocenter",
								"\nadc3_min", "adc3_max", "adc3_flat", "adc3_flat_outside", "adc3_reversed", "adc3_autocenter",
								"\nconfig_version"};

const char *cfg_vars_desc[] = {	debug_desc, debug_adv_desc, i2c_poll_rate_disable_desc,
								i2c_bus_desc, i2c_addr_desc, i2c_addr_sec_desc, nINT_GPIO_desc,
								js0_enable_desc, js1_enable_desc,
								adc_min_desc, adc_max_desc, adc_flat_desc, adc_flat_outside_desc, adc_reversed_desc, adc_autocenter_desc,
								adc_min_desc, adc_max_desc, adc_flat_desc, adc_flat_outside_desc, adc_reversed_desc, adc_autocenter_desc,
								adc_min_desc, adc_max_desc, adc_flat_desc, adc_flat_outside_desc, adc_reversed_desc, adc_autocenter_desc,
								adc_min_desc, adc_max_desc, adc_flat_desc, adc_flat_outside_desc, adc_reversed_desc, adc_autocenter_desc,
								cfg_version_desc};

//types : 0:int, 1:uint, 2:float, 3:double, 4:bool, 5:int array (split by comma in cfg file), 6:hex8, 7:hex16, 8:hex32, 9:bin8, 10:bin16, 11:bin32
const int cfg_vars_type[] = {	4/*debug*/,4/*debug_adv*/,4/*disable_pollrate*/,
								0/*i2c_bus*/, 6/*i2c_address*/, 6/*i2c_address_sec*/, 0/*i2c_irq*/,
								4/*js0_enable*/, 4/*js1_enable*/,
								0/*adc0_min*/, 0/*adc0_max*/, 0/*adc0_flat*/, 0/*adc0_flat_outside*/, 4/*adc0_reversed*/, 4/*adc0_autocenter*/,
								0/*adc1_min*/, 0/*adc1_max*/, 0/*adc1_flat*/, 0/*adc1_flat_outside*/, 4/*adc1_reversed*/, 4/*adc1_autocenter*/,
								0/*adc2_min*/, 0/*adc2_max*/, 0/*adc2_flat*/, 0/*adc2_flat_outside*/, 4/*adc2_reversed*/, 4/*adc2_autocenter*/,
								0/*adc3_min*/, 0/*adc3_max*/, 0/*adc3_flat*/, 0/*adc3_flat_outside*/, 4/*adc3_reversed*/, 4/*adc3_autocenter*/,
								8/*config_version*/};

/*pointers to real vars
* important note:
* - 8(int array) need to use format : {(int*)&array_prt, (int)&array_size}
*/
const void *cfg_vars_ptr[] = {	&debug, &debug_adv, &i2c_poll_rate_disable,
								&i2c_bus, &i2c_addr, &i2c_addr_sec, &nINT_GPIO,
								&js_enable[0], &js_enable[1],
								&adc_min[0], &adc_max[0], &adc_flat[0], &adc_flat_outside[0], &adc_reversed[0], &adc_autocenter[0],
								&adc_min[1], &adc_max[1], &adc_flat[1], &adc_flat_outside[1], &adc_reversed[1], &adc_autocenter[1],
								&adc_min[2], &adc_max[2], &adc_flat[2], &adc_flat_outside[2], &adc_reversed[2], &adc_autocenter[2],
								&adc_min[3], &adc_max[3], &adc_flat[3], &adc_flat_outside[3], &adc_reversed[3], &adc_autocenter[3],
								&cfg_version};

const unsigned int cfg_vars_arr_size = sizeof(cfg_vars_type) / sizeof(*cfg_vars_type); //config pointers array size


//Config related functions
static int cstring_in_cstring_array (char **arr, char *value, unsigned int arrSize, bool skipNl) { //search in char array, return index if found, -1 if not
    char *rowPtr;
    for (unsigned int i = 0; i < arrSize; i++) {
		char tmpVal [strlen(arr[i])+1]; strcpy (tmpVal, arr[i]);
        if (skipNl && tmpVal[0]=='\n') {rowPtr = tmpVal + 1;} else {rowPtr = tmpVal;}
        if (strcmp (rowPtr, value) == 0) {return i;}
    }
    return -1;
}

static int confic_sum (void) { //pseudo checksum for config build
	int ret = 0;
	for (unsigned int i = 0; i < cfg_vars_arr_size; i++){
		ret += strlen(cfg_vars_name[i]) + (cfg_vars_type[i] + 1)*2 + i*4;
	}
	return ret;
}

static bool config_save (bool reset) { //save config file
	cfg_version = confic_sum (); //pseudo checksum for config build

    if (reset) {if(remove(cfg_filename) != 0) {print_stderr("failed to delete '%s'\n", cfg_filename);}}

    FILE *filehandle = fopen(cfg_filename, "wb");
    if (filehandle != NULL) {
        char strBuffer [4096], strBuffer1 [33]; int strBufferSize;
        for (unsigned int i = 0; i < cfg_vars_arr_size; i++) {
            int tmpType = cfg_vars_type[i];
            if (tmpType == 0) {fprintf (filehandle, "%s=%d;", cfg_vars_name[i], *(int*)cfg_vars_ptr[i]); //int
            } else if (tmpType == 1) {fprintf (filehandle, "%s=%u;", cfg_vars_name[i], *(unsigned int*)cfg_vars_ptr[i]); //unsigned int
            } else if (tmpType == 2) {fprintf (filehandle, "%s=%f;", cfg_vars_name[i], *(float*)cfg_vars_ptr[i]); //float
            } else if (tmpType == 3) {fprintf (filehandle, "%s=%lf;", cfg_vars_name[i], *(double*)cfg_vars_ptr[i]); //double
            } else if (tmpType == 4) {fprintf (filehandle, "%s=%d;", cfg_vars_name[i], (*(bool*)cfg_vars_ptr[i])?1:0); //bool
            } else if (tmpType == 5) { //int array, output format: var=%d,%d,%d,...;
                int arrSize = *(int*)((int*)cfg_vars_ptr[i])[1];
                sprintf (strBuffer, "%s=", cfg_vars_name[i]); int strBufferSize = strlen(strBuffer);
                for (unsigned int j = 0; j < arrSize; j++) {strBufferSize += sprintf (strBuffer1, "%d,", ((int*)((int*)cfg_vars_ptr[i])[0])[j]); strcat (strBuffer, strBuffer1);} *(strBuffer+strBufferSize-1) = '\0';
                fprintf (filehandle, "%s;", strBuffer);
            } else if (tmpType >= 6 && tmpType < 9) { //hex8-32
                int ind = 2; for(int j=0; j<tmpType-6; j++){ind*=2;}
                sprintf (strBuffer, "%%s=0x%%0%dX;", ind); //build that way to limit var size
                fprintf (filehandle, strBuffer, cfg_vars_name[i], *(int*)cfg_vars_ptr[i]);
            } else if (tmpType >= 9 && tmpType < 12) { //bin8-32 (itoa bypass)
                int ind = 8; for(int j=0; j<tmpType-9; j++){ind*=2;}
                for(int j = 0; j < ind; j++){strBuffer[ind-j-1] = ((*(int*)cfg_vars_ptr[i] >> j) & 0b1) +'0';} strBuffer[ind]='\0';
                fprintf (filehandle, "%s=%s;", cfg_vars_name[i], strBuffer);
            }

			if(strlen(cfg_vars_desc[i]) > 0){ //add comments
				fprintf (filehandle, " //%s\n", cfg_vars_desc[i]);
			}else{fprintf (filehandle, "\n");}
        }
        fclose(filehandle);
        print_stdout("%s: %s\n", reset ? "config reset successfully" : "config saved successfully", cfg_filename);

		int err = chown(cfg_filename, (uid_t) 1000, (gid_t) 1000);
		if (err < 0) {print_stderr("changing %s owner failed with errno:%d (%m)\n", cfg_filename, -err);
		}else {print_stdout("%s owner changed successfully (uid:1000, gid:1000)\n", cfg_filename);}

        return true;
    } else {print_stderr("failed to write config: %s\n", cfg_filename);}
    return false;
}

static void config_parse (void) { //parse/create program config file
    FILE *filehandle = fopen(cfg_filename, "r");
    if (filehandle != NULL) {
        char strBuffer [4096], strTmpBuffer [4096]; //string buffer
        char *tmpPtr, *tmpPtr1, *pos; //pointers
        int line=0;
        while (fgets (strBuffer, 4095, filehandle) != NULL) { //line loop
            //clean line from utf8 bom and whitespaces
            tmpPtr = strBuffer; tmpPtr1 = strTmpBuffer; pos = tmpPtr; //pointers
            if (strstr (strBuffer,"\xEF\xBB\xBF") != NULL) {tmpPtr += 3;} //remove utf8 bom, overkill security?
            while (*tmpPtr != '\0') { //read all chars, copy if not whitespace
				if (tmpPtr - pos > 0) {if(*tmpPtr=='/' && *(tmpPtr-1)=='/'){tmpPtr1--; break;}} //break if // comment
				if(!isspace(*tmpPtr)) {
					*tmpPtr1++ = *tmpPtr++;
				}else{
					tmpPtr++;
				}
			}
            *tmpPtr1='\0'; strcpy(strBuffer, strTmpBuffer); line++; //copy char array

            //parse line
            tmpPtr = strtok (strBuffer, ";"); //split element
            while (tmpPtr != NULL) { //var=val loop
                char strElementBuffer [strlen(tmpPtr)]; strcpy (strElementBuffer, tmpPtr); //copy element to new buffer to avoid pointer mess
                tmpPtr1 = strchr(strElementBuffer, '='); //'=' char position
                if (tmpPtr1 != NULL) { //contain '='
                    *tmpPtr1='\0'; int tmpVarSize = strlen(strElementBuffer), tmpValSize = strlen(tmpPtr1+1); //var and val sizes
                    char tmpVar [tmpVarSize+1]; strcpy (tmpVar, strElementBuffer); //extract var
                    char tmpVal [tmpValSize+1]; strcpy (tmpVal, tmpPtr1 + 1); //extract val
                    int tmpIndex = cstring_in_cstring_array ((char**)cfg_vars_name, tmpVar, cfg_vars_arr_size, true); //var in config array
                    if (tmpIndex != -1) { //found in config array
                        int tmpType = cfg_vars_type[tmpIndex];
                        if (tmpType == 0) {
                            *(int*)cfg_vars_ptr[tmpIndex] = atoi (tmpVal); //int
                            if(debug){print_stderr("DEBUG: %s=%d (file:%s)(int:%d)\n", tmpVar, *(int*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);}
                        } else if (tmpType == 1) {
                            *(unsigned int*)cfg_vars_ptr[tmpIndex] = atoi (tmpVal); //unsigned int
                            if(debug){print_stderr("DEBUG: %s=%u (file:%s)(uint:%d)\n", tmpVar, *(unsigned int*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);}
                        } else if (tmpType == 2) {
                            *(float*)cfg_vars_ptr[tmpIndex] = atof (tmpVal); //float
                            if(debug){print_stderr("DEBUG: %s=%f (file:%s)(float:%d)\n", tmpVar, *(float*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);}
                        } else if (tmpType == 3) {
                            *(double*)cfg_vars_ptr[tmpIndex] = atof (tmpVal); //double
                            if(debug){print_stderr("DEBUG: %s=%lf (file:%s)(double:%d)\n", tmpVar, *(double*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);}
                        } else if (tmpType == 4) {
                            *(bool*)cfg_vars_ptr[tmpIndex] = (atoi (tmpVal) > 0)?true:false; //bool
                            if(debug){print_stderr("DEBUG: %s=%d (file:%s)(bool:%d)\n", tmpVar, *(bool*)cfg_vars_ptr[tmpIndex]?1:0, tmpVal, tmpType);}
                        } else if (tmpType == 5) { //int array, input format: var=%d,%d,%d,...;
                            int arrSize = *(int*)((int*)cfg_vars_ptr[tmpIndex])[1]; int j = 0;
                            char tmpVal1 [tmpValSize+1]; strcpy(tmpVal1, tmpVal); char *tmpPtr2 = strtok(tmpVal1, ",");
                            while (tmpPtr2 != NULL) {
                                if (j < arrSize) {((int*)((int*)cfg_vars_ptr[tmpIndex])[0])[j] = atoi (tmpPtr2);} //no overflow
                                j++; tmpPtr2 = strtok (NULL, ","); //next element
                            }

                            if(debug){
                                if (j!=arrSize) {print_stderr("DEBUG: Warning var '%s' elements count mismatch, should have %d but has %d\n", tmpVar, arrSize, j);}
                                char strBuffer1 [4096]={'\0'}; strTmpBuffer[0]='\0'; int strBufferSize = 0;
                                for (j = 0; j < arrSize; j++) {strBufferSize += sprintf (strBuffer1, "%d,", ((int*)((int*)cfg_vars_ptr[tmpIndex])[0])[j]); strcat (strTmpBuffer, strBuffer1);} *(strTmpBuffer+strBufferSize-1) = '\0';
                                print_stderr("DEBUG: %s=%s (file:%s)(int array:%d)\n", tmpVar, strTmpBuffer, tmpVal, tmpType);
                            }
                        } else if (tmpType >= 6 && tmpType < 9) { //hex8-32
                            int ind = 2; for(int j=0; j<tmpType-6; j++){ind*=2;}
                            char *tmpPtr2 = strchr(tmpVal,'x');
                            if(tmpPtr2!=NULL){
                                sprintf(strTmpBuffer, "0x%%0%dX", ind);
                                sscanf(tmpVal, strTmpBuffer, cfg_vars_ptr[tmpIndex]);
                            }else{
                                if(debug){print_stderr("DEBUG: Warning var '%s' should have a hex value, assumed a int\n", tmpVar);}
                                *(int*)cfg_vars_ptr[tmpIndex] = atoi (tmpVal);
                            }

                            if(debug){
                                //sprintf(strTmpBuffer, "DEBUG: %%s=0x%%0%dx(%%d) (file:%%s)(hex:%%d)\n", ind);
                                print_stderr("DEBUG: %s=0x%X(%d) (file:%s)(hex:%d)\n"/*strTmpBuffer*/, tmpVar, *(int*)cfg_vars_ptr[tmpIndex], *(int*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);
                            }
                        } else if (tmpType >= 9 && tmpType < 12) { //bin8-32
                            int ind = 8; for(int j=0; j<tmpType-9; j++){ind*=2;}
                            if (debug && tmpValSize!=ind) {print_stderr("DEBUG: Warning var '%s' value lenght mismatch, needs %d but has %d\n", tmpVar, ind, tmpValSize);}
                            
                            int tmp = 0; for(int j = 0; j < ind; j++){if (j<tmpValSize) {if(tmpVal[tmpValSize-j-1] > 0+'0'){tmp ^= 1U << j;}}}
                            *(int*)cfg_vars_ptr[tmpIndex] = tmp;

                            if(debug){
                                for(int j = 0; j < ind; j++){strTmpBuffer[ind-j-1] = ((*(int*)cfg_vars_ptr[tmpIndex] >> j) & 0b1) +'0';} strTmpBuffer[ind]='\0';
                                print_stderr("DEBUG: %s=%s(%d) (file:%s)(int array:%d)\n", tmpVar, strTmpBuffer, *(int*)cfg_vars_ptr[tmpIndex], tmpVal, tmpType);
                            }
                        }
                    } else if (debug) {print_stderr("DEBUG: var '%s'(line:%d) not allowed, typo?\n", tmpVar, line);}
                }
                tmpPtr = strtok (NULL, ";"); //next element
            }
        }
        fclose(filehandle);

		//pseudo checksum for config build
		cfg_version_org = confic_sum (); 
		if(cfg_version != cfg_version_org) {
			print_stderr("config file version mismatch (got:%d, should be:%d), forcing save to implement new vars set\n", cfg_version, cfg_version_org);
			cfg_version = cfg_version_org; config_save (false);
		}
    } else {config_save (false);}
}


//Time related functions
static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return 0.; //failed
}


//UHID related functions
static int uhid_create(int fd) { //create uhid device
	struct uhid_event ev;

	unsigned char rdesc[] = { //TODO: dynamic build
	0x05, 0x01, //; USAGE_PAGE (Generic Desktop)
	0x09, 0x05, //; USAGE (Gamepad)
	0xA1, 0x01, //; COLLECTION (Application)
		0x05, 0x09, //; USAGE_PAGE (Button)
		0x19, 0x01, //; USAGE_MINIMUM (Button 1)
		0x29, 0x0C, //; USAGE_MAXIMUM (Button 12)
		0x15, 0x00, //; LOGICAL_MINIMUM (0)
		0x25, 0x01, //; LOGICAL_MAXIMUM (1)
		0x75, 0x01, //; REPORT_SIZE (1)
		0x95, 0x10, //; REPORT_COUNT (16)
		0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

		0x05, 0x01, // Usage Page (Generic Desktop)
		0x15, 0xFF, // Logical Minimum (-1)
		0x25, 0x01, // LOGICAL_MAXIMUM (1)
		0x09, 0x34, // Usage (RY)
		0x09, 0x35, // Usage (RZ)
		0x75, 0x08, // Report Size (8)
		0x95, 0x02, // Report Count (2)
		0x81, 0x02, // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

		0x05, 0x01, // Usage Page (Generic Desktop)
		0x15, 0x00, // LOGICAL_MINIMUM (0)
		0x26, 0xFF, 0xFF, // LOGICAL_MAXIMUM (0xFFFF)
		0x09, 0x30, // Usage (X)
		0x09, 0x31, // Usage (Y)
		0x09, 0x32, // Usage (Z)
		0x09, 0x33, // Usage (RX)
		0x75, 0x10, // Report Size (16)
		0x95, 0x04, // Report Count (4)
		0x81, 0x02, // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0xC0,// ; END_COLLECTION
	};

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, uhid_device_name);
	ev.u.create.rd_data = rdesc;
	ev.u.create.rd_size = sizeof(rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

	return uhid_write(fd, &ev);
}

static void uhid_destroy(int fd){ //close uhid device
	struct uhid_event ev;
	if (fd < 0) return; //already failed
	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;
	int ret = uhid_write(fd, &ev);
	if(ret < 0){
		print_stdout("failed to destroy uhid device, errno:%d (%s)\n", -ret, strerror(-ret));
	} else {print_stdout("uhid device destroyed\n");}
}

static int uhid_write(int fd, const struct uhid_event *ev){ //write data to uhid device
	ssize_t ret = write(fd, ev, sizeof(*ev));
	if (ret < 0) {
		print_stderr("write to uhid device failed with errno:%d (%m)\n", -ret);
		return ret;//-errno;
	} else if (ret != sizeof(*ev)) {
		print_stderr("wrong size wrote to uhid device: %zd != %zu\n", ret, sizeof(ev));
		return -EFAULT;
	} else {return ret;}
}

static int uhid_send_event(int fd) { //send event to uhid device
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
    ev.u.input.size = 12;
	//if (uhid_js_left_enable) {ev.u.input.size += 4;}
	//if (uhid_js_right_enable) {ev.u.input.size += 4;}

	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons12to8;

	ev.u.input.data[2] = (unsigned char) gamepad_report.hat_x;
	ev.u.input.data[3] = (unsigned char) gamepad_report.hat_y;

	if (uhid_js_left_enable) {
		ev.u.input.data[4] = gamepad_report.left_x & 0xFF;
		ev.u.input.data[5] = gamepad_report.left_x >> 8;
		ev.u.input.data[6] = gamepad_report.left_y & 0xFF;
		ev.u.input.data[7] = gamepad_report.left_y >> 8;
	} else {
		ev.u.input.data[4] = 0xFF; ev.u.input.data[5] = 0x7F;
		ev.u.input.data[6] = 0xFF; ev.u.input.data[7] = 0x7F;
	}

	if (uhid_js_right_enable) {
		ev.u.input.data[8] = gamepad_report.right_x & 0xFF;
		ev.u.input.data[9] = gamepad_report.right_x >> 8;
		ev.u.input.data[10] = gamepad_report.right_y & 0xFF;
		ev.u.input.data[11] = gamepad_report.right_y >> 8;
	} else {
		ev.u.input.data[8] = 0xFF; ev.u.input.data[9] = 0x7F;
		ev.u.input.data[10] = 0xFF; ev.u.input.data[11] = 0x7F;
	}

	return uhid_write(fd, &ev);
}


//I2C related
static int i2c_open(int bus, int addr){ //open I2C bus
	char fd_path[13];
	if (bus < 0){bus = 0;}
	if (addr < 0) {print_stderr("FATAL: invalid I2C address:0x%02X (%d)\n", addr, addr); exit(EXIT_FAILURE);}

	sprintf(fd_path, "/dev/i2c-%d", bus);
	
	int fd = open(fd_path, O_RDWR);
	if (fd < 0) {
		print_stderr("FATAL: failed to open '%s', errno:%d (%m)\n", fd_path, -fd);
		exit(EXIT_FAILURE);
	}

	int ret = ioctl(fd, I2C_SLAVE, addr);
	if (ret < 0) {
		close(fd);
		print_stderr("FATAL: ioctl failed for adress 0x%02X, errno:%d (%m)\n", addr, -ret);
		exit(EXIT_FAILURE);
	} else {
		ret = i2c_smbus_read_byte_data(fd, 0);
		if (ret < 0) {
			close(fd);
			print_stderr("FATAL: read from adress 0x%02X failed, errno:%d (%m)\n", addr, -ret);
			exit(EXIT_FAILURE);
		}
	}

	print_stdout("address:0x%02X opened (bus:%s)\n", addr, fd_path);
	return fd;
}

static void i2c_close(int fd, int addr){ //close I2C bus
	if (fd < 0) {print_stdout("address 0x%02X already closed\n", addr);
	} else {
		int ret = close(fd);
		if (ret < 0){print_stderr("failed to close I2C handle for address 0x%02X, errno:%d (%m)\n", addr, -ret);} else {print_stdout("I2C handle for address:0x%02X closed\n", addr);}
	}
}

static int adc_correct_offset_center(int adc_resolution, int adc_value, int adc_min, int adc_max, int adc_offsets, int inside_flat, int outside_flat, int index){ //apply offset center, expand adc range, inside/ouside flat
	double adc_center = adc_resolution / 2; int range; int ratio; int corrected_value;

	if (adc_value < (adc_center + adc_offsets)){ //value under center offset
		range = (adc_center + adc_offsets) - adc_min;
		if (range != 0){corrected_value = (adc_value - adc_min) * (adc_center / range);
		} else {corrected_value = adc_value;} //range=0, setting problems?
	} else { //value over center offset
		range = adc_max - (adc_center + adc_offsets);
		if(range != 0){corrected_value = adc_center + (adc_value - (adc_center + adc_offsets)) * (adc_center / range);
		} else {corrected_value = adc_value;} //range=0, setting problems?
	}

	corrected_value -= adc_center;
	if (abs(corrected_value) < abs(adc_center * (double)(inside_flat / 100.))){return adc_center;} //inside flat
	corrected_value = (corrected_value * (100. + outside_flat) / 100.) + adc_center; //apply outside flat

	if (corrected_value < 0){return 0;}else if(corrected_value > adc_resolution){return adc_resolution;} //constrain computed value to adc resolution
	return corrected_value;
}

static void i2c_poll_joystick(void){ //poll data from i2c device
	/*
	* Benchmark (post regs update):
	* i2c_smbus_read_i2c_block_data : no analog : 2940 polls per sec
	* i2c_smbus_read_i2c_block_data : left : 2060 polls per sec
	* i2c_smbus_read_i2c_block_data : left + right : 1640 polls per sec
	*
	* TODO: test 12bits on real hardware, only tested by ino code hack by should work fine
	*/
	int read_limit = 2;
	if (uhid_js_right_enable){read_limit = 8;} else if (uhid_js_left_enable){read_limit = 5;}

	int ret = i2c_smbus_read_i2c_block_data(i2c_fd, 0, read_limit, (uint8_t *)&i2c_registers);

	if (ret < 0) {
		i2c_errors_count++;
		if (ret == -6) {print_stderr("FATAL: i2c_smbus_read_word_data() failed, errno %d : %m\n", -ret); kill_resquested = true;}
		if (i2c_errors_count >= i2c_errors_report) {print_stderr("WARNING: I2C requests failed %d times in a row\n", i2c_errors_count); i2c_errors_count = 0;}
		i2c_last_error = ret; return;
	}

	if (i2c_errors_count > 0){
		print_stderr("last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));
		i2c_last_error = i2c_errors_count = 0; //reset error count
	}
	
	//digital
	int8_t dpad[4];
	for (int8_t i=0; i<4; i++){dpad[i] = (~i2c_registers.input0 >> i & 0x01);}
	gamepad_report.hat_y = dpad[0] - dpad[1];
	gamepad_report.hat_x = dpad[3] - dpad[2];

	gamepad_report.buttons7to0 = ~(i2c_registers.input0 >> 4 | i2c_registers.input1 << 4);
	gamepad_report.buttons12to8 = ~(i2c_registers.input1 >> 4);

	//analog
	int js_values[4] = {0,0,0,0};
	if (uhid_js_left_enable) {
		js_values[0] = (i2c_registers.a0_msb << 8 | i2c_registers.a1a0_lsb << 4) >> (16 - adc_res);
		js_values[1] = (i2c_registers.a1_msb << 8 | ((i2c_registers.a1a0_lsb >> 4) << 4)) >> (16 - adc_res);
	}

	if (uhid_js_right_enable) {
		js_values[2] = (i2c_registers.a2_msb << 8 | i2c_registers.a3a2_lsb << 4) >> (16 - adc_res);
		js_values[3] = (i2c_registers.a3_msb << 8 | ((i2c_registers.a3a2_lsb >> 4) << 4)) >> (16 - adc_res);
	}

	for (uint8_t i=0; i<4; i++) {
		if (uhid_js_left_enable && i < 2 || uhid_js_right_enable && i > 1){
			if (adc_firstrun) {
				if(adc_autocenter[i]){adc_offsets[i] = js_values[i] - (adc_res_limit / 2); //auto center enable
				} else {adc_offsets[i] = (((adc_max[i] - adc_min[i]) / 2) + adc_min[i]) - (adc_res_limit / 2);}
			}
			
			if(js_values[i] < adc_values_min[i]){adc_values_min[i] = js_values[i];} //update min value
			if(js_values[i] > adc_values_max[i]){adc_values_max[i] = js_values[i];} //update max value

			js_values[i] = adc_correct_offset_center(adc_res_limit, js_values[i], adc_min[i], adc_max[i], adc_offsets[i], adc_flat[i], adc_flat_outside[i], i); //re-center adc value, apply flats and extend to adc range
			if(adc_reversed[i]){js_values[i] = abs(adc_res_limit - js_values[i]);} //reverse value

			js_values[i] <<= 16 - adc_res; //convert to 16bits for report
		}
	}

	if (uhid_js_left_enable) {gamepad_report.left_x = js_values[0]; gamepad_report.left_y = js_values[1];}
	if (uhid_js_right_enable) {gamepad_report.right_x = js_values[2]; gamepad_report.right_y = js_values[3];}
	
	//report
	int report_val = 0, report_prev_val = 1; adc_firstrun = false;
	if (!(uhid_js_left_enable + uhid_js_right_enable)) {
		report_val = gamepad_report.buttons7to0 + gamepad_report.buttons12to8 + gamepad_report.hat_x + gamepad_report.hat_y;
		report_prev_val = gamepad_report_prev.buttons7to0 + gamepad_report_prev.buttons12to8 + gamepad_report_prev.hat_x + gamepad_report_prev.hat_y;
	}
	
	if (report_val != report_prev_val){
		gamepad_report_prev = gamepad_report;
		uhid_send_event(uhid_fd);
	}
}


//IRQ related functions
#ifdef USE_PIGPIO_IRQ
static void gpio_callback(int gpio, int level, uint32_t tick) { //look to work
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
static void attiny_irq_handler(void) {
	if(debug) print_stdout("DEBUG: GPIO%d triggered\n", nINT_GPIO);
    i2c_poll_joystick();
}
#endif


//TTY related functions
static void tty_signal_handler(int sig) { //handle signal func
	if(debug) print_stderr("DEBUG: signal received: %d\n", sig);
	kill_resquested = true;
}


//Debug related functions
static void debug_print_binary_int (int val, int bits, char* var) { //print given var in binary format
	if(!debug) return;
	printf("DEBUG: BIN: %s : ", var);
	for(int mask_shift = bits-1; mask_shift > -1; mask_shift--){printf("%d", (val >> mask_shift) & 0b1);}
	printf("\n");
}


int main(int argc, char **argv) {
	program_start_time = get_time_double();
	const char *path = "/dev/uhid";
	int ret, main_return = EXIT_SUCCESS;

	config_parse (); //parse config file, create if needed

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
	//signal(SIGKILL, tty_signal_handler); //doesn't work, program get killed before able to handle

	if (argc >= 2) { //TODO
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			fprintf(stderr, "Usage: %s [%s]\n", argv[0], path);
			return EXIT_SUCCESS;
		} else {
			path = argv[1];
		}
	}

	//process secondary attiny address to check magic bit and version
	i2c_fd_sec = i2c_open(i2c_bus, i2c_addr_sec);
	ret = i2c_smbus_read_byte_data(i2c_fd_sec, 0x00); //check signature
	if (ret != i2c_magic_sig){
		if (ret < 0){print_stderr("FATAL: reading I2C device signature failed, errno %d (%m)\n", -ret);
		} else {print_stderr("FATAL: invalid I2C device signature: 0x%02X\n", ret);}
		i2c_close(i2c_fd_sec, i2c_addr_sec);
		return EXIT_FAILURE;
	}

	ret = i2c_smbus_read_word_data(i2c_fd_sec, 0x01); //check version
	if (ret < 0){
		print_stderr("FATAL: reading I2C device version failed, errno %d (%m)\n", -ret);
		i2c_close(i2c_fd_sec, i2c_addr_sec);
		return EXIT_FAILURE;
	} else {
		i2c_dev_major = ret & 0xFF;
		i2c_dev_minor = (ret >> 8) & 0xFF;
		print_stdout("I2C device detected, signature:0x%02X, version:%d.%d\n", i2c_magic_sig, i2c_dev_major, i2c_dev_minor);
	}

	//process primary attiny address
	i2c_fd = i2c_open(i2c_bus, i2c_addr);

	//set analog config
	uint8_t analog_reg = 0; if(js_enable[0]){analog_reg|=0x03;} if(js_enable[1]){analog_reg|=0x0C;}

	ret = i2c_smbus_write_byte_data(i2c_fd, REGISTER_ADC_ENABLE, analog_reg);
	if (ret < 0){print_stderr("failed to set ADC configuration, errno %d (%m)\n", -ret);
	} else {print_stdout("ADC configuration set successfully\n");}

	//detect analog config
	ret = i2c_smbus_read_byte_data(i2c_fd, REGISTER_ADC_ENABLE); //check what adc enabled ADC0-3
	if (ret < 0){
		print_stderr("FATAL: reading ADC configuration failed, errno %d (%m)\n", -ret);
		i2c_close(i2c_fd, i2c_addr);
		return EXIT_FAILURE;
	} else {
		bool tmp_adc[4];
		for (uint8_t i=0; i<4; i++){tmp_adc[i] = (ret >> i) & 0b1;}
		uhid_js_left_enable = tmp_adc[0] && tmp_adc[1];
		uhid_js_right_enable = tmp_adc[2] && tmp_adc[3];

		if (uhid_js_left_enable + uhid_js_right_enable) {
			print_stdout("detected ADC configuration: %s %s\n", uhid_js_left_enable ? "left" : "", uhid_js_right_enable ? "right" : "");
			#if defined(USE_WIRINGPI_IRQ) || defined(USE_PIGPIO_IRQ)
			print_stdout("IRQ disabled\n");
			#endif
			
			ret = i2c_smbus_read_byte_data(i2c_fd, REGISTER_ADC_RES); //current ADC resolution
			if (ret < 0){
				print_stderr("FATAL: reading ADC resolution failed, errno %d (%m)\n", -ret);
				i2c_close(i2c_fd, i2c_addr);
				return EXIT_FAILURE;
			} else {
				adc_res = ret;
				adc_res_limit = adc_res_limit >> (sizeof(adc_res_limit)*8 - adc_res);
				print_stdout("detected ADC resolution: %dbits (%d)\n", adc_res, adc_res_limit);

				//bulletproofing driver against user settings
				for (uint8_t i=0; i<4; i++){
					if (uhid_js_left_enable && i < 2 || uhid_js_right_enable && i > 1){
						if(adc_max[i] > adc_res_limit) {
							print_stdout("WARN: adc%d_max (%d) over ADC resolution (%d), limited said resolution\n", i, adc_max[i], adc_res_limit);
							adc_max[i] = adc_res_limit;
						}
					}
				}
			}
		}
	}

	uhid_fd = open(path, O_RDWR | O_CLOEXEC);
	if (uhid_fd < 0) {
		print_stderr("failed to open uhid-cdev %s, errno:%d (%m)\n", path, -uhid_fd);
		return EXIT_FAILURE;
	} else {print_stdout("uhid-cdev %s opened\n", path);}

	ret = uhid_create(uhid_fd);
	if (ret < 0) {close(uhid_fd); return EXIT_FAILURE;
	} else {print_stdout("uhid device created\n");}

	i2c_poll_joystick(); //initial poll

	if (!(uhid_js_left_enable + uhid_js_right_enable) && nINT_GPIO >= 0) {
	#ifdef USE_WIRINGPI_IRQ
		#define WIRINGPI_CODES 1 //allow error code return
		int err;
		if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
			print_stderr("failed to initialize wiringPi, errno:%d\n", -err);
		} else {
			if ((err = wiringPiISR(nINT_GPIO, INT_EDGE_FALLING, &attiny_irq_handler)) < 0){
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
			gpioSetSignalFunc(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
			irq_enable = true;
		}
	#endif
	}

    fprintf(stderr, "Press '^C' to quit...\n");
	
	if (irq_enable){
		while (!kill_resquested){usleep(100000);} //sleep until app close requested
	} else {
		if (!i2c_poll_rate_disable){print_stdout("polling at %dhz\n", i2c_poll_rate);
		} else {print_stdout("poll speed not limited\n");}

		while (!kill_resquested) {
			poll_clock_start = get_time_double();
			if (debug_adv && poll_benchmark_clock_start < 0.) {poll_benchmark_clock_start = poll_clock_start;} //benchmark

			i2c_poll_joystick();

			//poll rate implement
			
			if (kill_resquested) break;
			i2c_poll_duration = get_time_double() - poll_clock_start;
			//if(debug) print_stdout ("DEBUG: poll_clock_start:%lf, i2c_poll_duration:%lf\n", poll_clock_start, i2c_poll_duration);

			if (!i2c_poll_rate_disable){
				if (i2c_poll_duration > i2c_poll_duration_warn){print_stderr ("WARNING: extremely long loop duration: %dms\n", (int)(i2c_poll_duration*1000));}
				if (i2c_poll_duration < 0){i2c_poll_duration = 0;} //hum, how???
				if (i2c_poll_duration < i2c_poll_rate_time){usleep((useconds_t) ((double)(i2c_poll_rate_time - i2c_poll_duration) * 1000000));} //need to sleep to match poll rate
				//if(debug) print_stdout ("DEBUG: loop duration:%lf, i2c_poll_rate_time:%lf\n", get_time_double() - poll_clock_start, i2c_poll_rate_time);
			}// else {if(debug) print_stdout ("DEBUG: loop duration:%lf\n", get_time_double() - poll_clock_start);}

			if (debug_adv){ //benchmark mode
				poll_benchmark_loop++;
				if ((get_time_double() - poll_benchmark_clock_start) > 2.) { //report every seconds
					print_stdout("poll loops per sec (2secs samples) : %ld\n", poll_benchmark_loop/2);
					poll_benchmark_loop = 0; poll_benchmark_clock_start = -1.;
				}
			}
		}
	}

	if (i2c_last_error != 0) {print_stderr("last detected I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));}

	i2c_close(i2c_fd, i2c_addr);
	i2c_close(i2c_fd_sec, i2c_addr_sec);
	uhid_destroy(uhid_fd);

	if(uhid_js_left_enable + uhid_js_right_enable){
		print_stdout("detected adc limits:\n");
		for (uint8_t i=0; i<4; i++) {print_stdout("adc%d: min:%d max:%d\n", i, adc_values_min[i], adc_values_max[i]);}
	}

	return main_return;
}
