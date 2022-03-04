/*
* Freeplaytech I2C Joystick Kernel driver setup/diagnostic tool
* Important Notes:
* - If something goes wrong, user terminal will be messed as this program disable echo(s), incl. normal or esc keys echos. If so, please run command 'sudo diag -termreset'
* - Provided "as is", please check git for further updates
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

#include "diag.h" //program specific "non-user" defined header

/*
* TODO NOTES FOR TESTING:
* update "dtoverlay_filename", "dtoverlay_install_cmd_path", "struct dtoverlay_driver_struct", "button_names[input_values_count]"
*/

//vendor defined settings
bool debug = true; //enable debug outputs
char* dtoverlay_name = "freeplay-joystick"; //dtoverlay module name
char* dtoverlay_filename = "/dev/shm/config.txt"/*"/boot/config.txt"*/; //file that contain dtoverlays, e.g. "/boot/config.txt" for Raspberry Pi
char* dtoverlay_install_cmd_path = "/dev/shm/fpjs_install.sh"; //file that contain install command for dtoverlay, leave blank to disable
const uint8_t i2c_dev_manuf = 0xED; //I2C MCU manufacturer signature, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
const int button_count_limit = 19; //max amount of digital buttons (excl. Dpad)
const int debounce_default = 5, debounce_limit = 7; //default/limit value of digital debouncing

dtoverlay_driver_t driver_default = { //driver default settings, 'driver_user' and 'driver_back' set during runtime
    .bus=1, .addr=0x30, .interrupt=40, /*.analogsticks=0,*/ .digitalbuttons=11, .dpads=false, .debounce=debounce_default,
    .joy0_x_min=0, .joy0_x_max=0xFFF, .joy0_x_fuzz=32, .joy0_x_flat=300, .joy0_x_inverted=false,/* .joy0_x_enabled=false,*/
    .joy0_y_min=0, .joy0_y_max=0xFFF, .joy0_y_fuzz=32, .joy0_y_flat=300, .joy0_y_inverted=false,/* .joy0_y_enabled=false,*/
    .joy0_swapped_x_y=false, .joy0_enabled=false,
    .joy1_x_min=0, .joy1_x_max=0xFFF, .joy1_x_fuzz=32, .joy1_x_flat=300, .joy1_x_inverted=false,/* .joy1_x_enabled=false,*/
    .joy1_y_min=0, .joy1_y_max=0xFFF, .joy1_y_fuzz=32, .joy1_y_flat=300, .joy1_y_inverted=false,/* .joy1_y_enabled=false,*/
    .joy1_swapped_x_y=false, .joy1_enabled=false,
};

mcu_config0_t mcu_config0_default = {.vals.debounce_level = debounce_default}; //default config register value

dtoverlay_parser_store_t dtoverlay_store[] = { //store dtoverlay argument to program "transaction". format:argument,type,pointer. Type: 0:int, 1:hex, 2:bool
    {"interrupt", 0, &driver_user.interrupt},
    {"addr", 1, &driver_user.addr},
    {"digitalbuttons", 0, &driver_user.digitalbuttons},
    {"dpads", 2, &driver_user.dpads},
    {"debounce", 0, &driver_user.debounce},

    {"joy0-x-min", 0, &driver_user.joy0_x_min},
    {"joy0-x-max", 0, &driver_user.joy0_x_max},
    {"joy0-x-fuzz", 0, &driver_user.joy0_x_fuzz},
    {"joy0-x-flat", 0, &driver_user.joy0_x_flat},
    {"joy0-x-inverted", 2, &driver_user.joy0_x_inverted},
    /*{"joy0-x-enable", 2, &driver_user.joy0_x_enabled},*/
    {"joy0-y-min", 0, &driver_user.joy0_y_min},
    {"joy0-y-max", 0, &driver_user.joy0_y_max},
    {"joy0-y-fuzz", 0, &driver_user.joy0_y_fuzz},
    {"joy0-y-flat", 0, &driver_user.joy0_y_flat},
    {"joy0-y-inverted", 2, &driver_user.joy0_y_inverted},
    /*{"joy0-y-enable", 2, &driver_user.joy0_y_enabled},*/
    {"joy0-swapped-x-y", 2, &driver_user.joy0_swapped_x_y},
    {"joy0-enabled", 2, &driver_user.joy0_enabled},

    {"joy1-x-min", 0, &driver_user.joy1_x_min},
    {"joy1-x-max", 0, &driver_user.joy1_x_max},
    {"joy1-x-fuzz", 0, &driver_user.joy1_x_fuzz},
    {"joy1-x-flat", 0, &driver_user.joy1_x_flat},
    {"joy1-x-inverted", 2, &driver_user.joy1_x_inverted},
    /*{"joy1-x-enable", 2, &driver_user.joy1_x_enabled},*/
    {"joy1-y-min", 0, &driver_user.joy1_y_min},
    {"joy1-y-max", 0, &driver_user.joy1_y_max},
    {"joy1-y-fuzz", 0, &driver_user.joy1_y_fuzz},
    {"joy1-y-flat", 0, &driver_user.joy1_y_flat},
    {"joy1-y-inverted", 2, &driver_user.joy1_y_inverted},
    /*{"joy1-y-enable", 2, &driver_user.joy1_y_enabled},*/
    {"joy1-swapped-x-y", 2, &driver_user.joy1_swapped_x_y},
    {"joy1-enabled", 2, &driver_user.joy1_enabled},
};

struct i2c_joystick_register_struct { //mcu main registers, direct copy from mcu code file, DO NOT EDIT UNTIL YOU KNOW WHAT YOU ARE DOING
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

#define input_registers_count 3 //amount of digital input registers
#define input_values_count 8*input_registers_count //size of all digital input registers

const int dpad_start_index = 0; //where dpad report start (all input registers included), can go from 0 to 19 with 3 input registers
char* button_names[input_values_count] = { //event report key code, follow input0-3 order, based on 'button_codes', 'input0_bit_struct', 'input2_bit_struct' var from freeplay-joystick.c
    "Dpad UP", "Dpad DOWN", "Dpad LEFT", "Dpad RIGHT", "BTN_START", "BTN_SELECT", "BTN_A", "BTN_B",
    "BTN_MODE", "BTN_THUMBR", "BTN_TL2", "BTN_TR2", "BTN_X", "BTN_Y", "BTN_TL", "BTN_TR", 
    "BTN_THUMBL", "", "BTN_C", "BTN_Z", "BTN_0", "BTN_1", "BTN_2", "BTN_3",
};


adc_settings_t adc_settings_default[4] = { //default adc data pointers
    {&driver_default.joy0_x_min, &driver_default.joy0_x_max, &driver_default.joy0_x_fuzz, &driver_default.joy0_x_flat, &driver_default.joy0_x_inverted/*, &driver_default.joy0_x_enabled*/},
    {&driver_default.joy0_y_min, &driver_default.joy0_y_max, &driver_default.joy0_y_fuzz, &driver_default.joy0_y_flat, &driver_default.joy0_y_inverted/*, &driver_default.joy0_y_enabled*/},
    {&driver_default.joy1_x_min, &driver_default.joy1_x_max, &driver_default.joy1_x_fuzz, &driver_default.joy1_x_flat, &driver_default.joy1_x_inverted/*, &driver_default.joy1_x_enabled*/},
    {&driver_default.joy1_y_min, &driver_default.joy1_y_max, &driver_default.joy1_y_fuzz, &driver_default.joy1_y_flat, &driver_default.joy1_y_inverted/*, &driver_default.joy1_y_enabled*/},
};
bool *adc_axis_swap_default[2] = {&driver_default.joy0_swapped_x_y, &driver_default.joy1_swapped_x_y};
bool *js_enabled_default[2] = {&driver_default.joy0_enabled, &driver_default.joy1_enabled};


//debug functs
static void debug_print_binary_int_term(int line, int col, int val, int bits, char* var){ //print given var in binary format at given term position
	printf("\e[%d;%dH\e[1;100m%s : ", line, col, var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\e[0m");
}

//time functs
static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return -1.; //failed
}


//stream print functs
#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr


//generic functs
static void program_get_path(char** args, char* var){ //get current program path based on program argv or getcwd if failed
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


//dtoverlay functs/vars
const int dtoverlay_store_size = sizeof(dtoverlay_store) / sizeof(dtoverlay_parser_store_t); //computed store size

static int dtoverlay_parser_search_name(dtoverlay_parser_store_t* store, unsigned int store_size, char* value){ //search name into dtoverlay parser store, return index on success, -1 on failure
    char *rowPtr;
    for (unsigned int i = 0; i < store_size; i++) {
		char tmpVar[strlen(store[i].name)+1]; strcpy (tmpVar, store[i].name);
        if (tmpVar[0]=='\n'){rowPtr = tmpVar + 1;} else {rowPtr = tmpVar;}
        if (strcmp(rowPtr, value) == 0){return i;}
    }
    return -1;
}

static int dtoverlay_parser(char* filename, char* dtoverlay_name, dtoverlay_parser_store_t* store, unsigned int store_size){ //parse file that content dtoverlay data, e.g /boot/config.txt for Rpi
    int found = 0;
    FILE *filehandle = fopen(filename, "r");
    if (filehandle != NULL){
        char strBuffer[4096]={'\0'}; bool valid = false; int line = 0;
        while (fgets (strBuffer, 4095, filehandle) != NULL && !valid){ //line loop
            bool first = true; line++;
            char *bufferTokPtr, *tmpPtr = strtok_r(strBuffer, ",", &bufferTokPtr); //split element
            while (tmpPtr != NULL){ //var=val loop
                char buffer[strlen(tmpPtr)+1]; strcpy(buffer, tmpPtr); //copy element to new buffer to avoid pointer mess
                char *tmpPtr1 = strchr(buffer, '='); //'=' char position
                if (tmpPtr1 != NULL){ //contain '='
                    *tmpPtr1='\0';
                    char tmpVar[strlen(buffer)+1]; strcpy(tmpVar, buffer); //extract var
                    char *tmpVarPtr = tmpVar; str_trim_whitespace(&tmpVarPtr); //trim whitespaces
                    if (first){
                        char *commentPtr = strchr(tmpVarPtr, '#'); if (commentPtr && commentPtr == tmpVarPtr){if(debug){print_stderr("DEBUG: line %d, skip commented\n", line);} break;} //skip commented line
                        if (strcmp(tmpVarPtr, "dtoverlay")!=0){if (debug){print_stderr("DEBUG: line %d, skip line not starting by \"dtoverlay\"\n", line);} break;}
                    }

                    char tmpVal[strlen(tmpPtr1+1)+1]; strcpy(tmpVal, tmpPtr1+1); //extract val
                    char *tmpValPtr = tmpVal; str_trim_whitespace(&tmpValPtr); //trim whitespaces
                    if (first){
                        if (strcmp(tmpValPtr, dtoverlay_name)!=0){if (debug){print_stderr("DEBUG: line %d, skip invalid dtoverlay:\"%s\"\n", line, tmpValPtr);} break;} //invalid dtoverlay
                        if (debug){print_stderr("DEBUG: line %d, proper dtoverlay found:\"%s\"\n", line, tmpValPtr);}
                        valid = true; first = false;
                    } else {
                        int tmpIndex = dtoverlay_parser_search_name(store, store_size, tmpVarPtr); //var in store
                        if (tmpIndex != -1 && store[tmpIndex].ptr){ //found in config array
                            int type = store[tmpIndex].type;
                            if (type == 0 || type == 1){ //0:int, 2:hex
                                if (strchr(tmpValPtr,'x')){sscanf(tmpValPtr, "0x%x", (int*)store[tmpIndex].ptr);}else{*(int*)store[tmpIndex].ptr = atoi(tmpValPtr);} found++; //hex or int?
                                if (debug){print_stderr("DEBUG: line %d, %s=%d\n", line, tmpVarPtr, *(int*)store[tmpIndex].ptr);} //debug
                            } else if (type == 2){ //3:bool
                                *(bool*)store[tmpIndex].ptr = atoi(tmpValPtr)?true:false; found++;
                            } else if (debug){print_stderr("DEBUG: invalid type:%d\n", type);}
                        } else if (debug){print_stderr("DEBUG: line %d, var '%s' not allowed, typo?\n", line, tmpVarPtr);} //invalid var
                    }
                }
                tmpPtr = strtok_r(NULL, ",", &bufferTokPtr); //next element
            }
        }
        fclose(filehandle);
    } else {print_stderr("file not found\n"); return -1;}
    return found;
}

static void dtoverlay_generate(char* str, unsigned int len, char* dtoverlay_name, char dtoverlay_separator, dtoverlay_parser_store_t* store, unsigned int store_size){ //generate text line from given dtoverlay store
    array_fill(str, len, '\0'); //fully reset array
    strcat(str, dtoverlay_name);
    for (int i=0; i<store_size; i++){
        char buffer[4096]={'\0'}, buffer1[4095]={'\0'};
        if (store[i].type==1){sprintf(buffer1, "0x%X", *(int*)store[i].ptr); //hex
        } else if (store[i].type==2){sprintf(buffer1, "%d", (*(bool*)store[i].ptr)?1:0); //bool
        } else {sprintf(buffer1, "%d", *(int*)store[i].ptr);} //int
        sprintf(buffer, "%c%s=%s", dtoverlay_separator, store[i].name, buffer1);
        strcat(str, buffer);
    }
}

static int dtoverlay_save(char* filename, char* dtoverlay_name, dtoverlay_parser_store_t* store, unsigned int store_size){ //save current dtoverlay data to file. return 0 on success, -1 on failure
    char filename_tmp[strlen(filename)+7]; sprintf(filename_tmp, "%sXXXXXX", filename); mkstemp(filename_tmp); //generate temporary filename
    bool file_exists = false;

    FILE *filehandle_tmp = fopen(filename_tmp, "w");
    if (!filehandle_tmp){
        if (debug){print_stderr("FATAL: failed to create \"%s\"\n", filename_tmp);}
        return -1;
    } else if (debug){print_stderr("temporary file:%s\n", filename_tmp);}

    char strBuffer[4096]={'\0'};
    FILE *filehandle = fopen(filename, "r"); bool found = false;
    if (filehandle != NULL){
        file_exists = true;
        while (fgets(strBuffer, 4095, filehandle) != NULL){ //line loop
            bool valid = false;
            if (!found){ //proper dt line not found yet
                char strBuffer1[4096]={'\0'}; strcpy(strBuffer1, strBuffer);
                char *bufferTokPtr, *tmpPtr = strtok_r(strBuffer1, ",", &bufferTokPtr); //split element
                if (tmpPtr != NULL){ //',' found
                    char buffer[strlen(tmpPtr)+1]; strcpy(buffer, tmpPtr); //copy element to new buffer to avoid pointer mess
                    char *tmpPtr1 = strchr(buffer, '='); //'=' char position
                    if (tmpPtr1 != NULL){ //contain '='
                        *tmpPtr1='\0';
                        char tmpVar[strlen(buffer)+1]; strcpy(tmpVar, buffer); char *tmpVarPtr = tmpVar; str_trim_whitespace(&tmpVarPtr); //extract var, trim whitespaces
                        char *commentPtr = strchr(tmpVarPtr, '#'); if (commentPtr && commentPtr == tmpVarPtr){goto skip;} //commented line
                        if (strcmp(tmpVarPtr, "dtoverlay")!=0){goto skip;} //line not starting by "dtoverlay"
                        char tmpVal[strlen(tmpPtr1+1)+1]; strcpy(tmpVal, tmpPtr1+1); char *tmpValPtr = tmpVal; str_trim_whitespace(&tmpValPtr); //extract val, trim whitespaces
                        if (strcmp(tmpValPtr,dtoverlay_name)!=0){goto skip;} //invalid dtoverlay
                        valid = true; found = true;
                    }
                }
            }

            skip:;
            if (valid){dtoverlay_generate(strBuffer, 4096, dtoverlay_name, ',', store, store_size);} //generate proper dt line
            fputs("dtoverlay=", filehandle_tmp); fputs(strBuffer, filehandle_tmp); fputs("\n", filehandle_tmp); //copy line to temporary file
        }
        fclose(filehandle);
    }

    if (!found){
        dtoverlay_generate(strBuffer, 4096, dtoverlay_name, ',', store, store_size);
        fputs("dtoverlay=", filehandle_tmp); fputs(strBuffer, filehandle_tmp); fputs("\n", filehandle_tmp); //copy line to temporary file
    }

    //set file owner and rights
    unsigned int uid = getuid(), gid = getgid(), mode = 0644; struct stat file_stat = {0};
    if (file_exists && stat(filename, &file_stat) == 0){uid = file_stat.st_uid; gid = file_stat.st_gid; mode = file_stat.st_mode;} //get original file rights and owner
    if (fchown(fileno(filehandle_tmp), uid, gid) != 0 && debug){print_stderr("failed to change \"%s\" owner\n", filename_tmp);}
    if (fchmod(fileno(filehandle_tmp), mode) != 0 && debug){print_stderr("failed to change \"%s\" right\n", filename_tmp);}
    fclose(filehandle_tmp);

    //backup and replace
    char filename_backup[strlen(filename)+10]; sprintf(filename_backup, "%s.bak.fpjs", filename); //backup filename
    if (stat(filename_backup, &file_stat) == 0){
        if (debug){print_stderr("backup file \"%s\" removed\n", filename_backup);}
        remove(filename_backup);
    }

    if (file_exists){
        if (rename(filename, filename_backup) != 0 && debug){print_stderr("failed to rename \"%s\" to \"%s\"\n", filename, filename_backup);
        } else if (debug){print_stderr("\"%s\" renamed to \"%s\"\n", filename, filename_backup);}
    }

    if (rename(filename_tmp, filename) != 0 && debug){print_stderr("failed to rename \"%s\" to \"%s\"\n", filename_tmp, filename);
    } else if (debug){print_stderr("\"%s\" renamed to \"%s\"\n", filename_tmp, filename); return -1;}

    return 0;
}

static int dtoverlay_check(char* filename, char* dtoverlay_name){ //check dtoverlay file. Return detected line number on success, -1 on failure
    int ret = -1;
    FILE *filehandle = fopen(filename, "r");
    if (filehandle != NULL){
        char strBuffer[4096]={'\0'}; int line = 0;
        while (fgets(strBuffer, 4095, filehandle) != NULL){ //line loop
            char *bufferTokPtr, *tmpPtr = strtok_r(strBuffer, ",", &bufferTokPtr); line++; //split element
            if (tmpPtr != NULL){ //',' found
                char buffer[strlen(tmpPtr)+1]; strcpy(buffer, tmpPtr); //copy element to new buffer to avoid pointer mess
                char *tmpPtr1 = strchr(buffer, '='); //'=' char position
                if (tmpPtr1 != NULL){ //contain '='
                    *tmpPtr1='\0';
                    char tmpVar[strlen(buffer)+1]; strcpy(tmpVar, buffer); char *tmpVarPtr = tmpVar; str_trim_whitespace(&tmpVarPtr); //extract var, trim whitespaces
                    char *commentPtr = strchr(tmpVarPtr, '#'); if (commentPtr && commentPtr == tmpVarPtr){goto skip;} //commented line
                    if (strcmp(tmpVarPtr, "dtoverlay")!=0){goto skip;} //line not starting by "dtoverlay"
                    char tmpVal[strlen(tmpPtr1+1)+1]; strcpy(tmpVal, tmpPtr1+1); char *tmpValPtr = tmpVal; str_trim_whitespace(&tmpValPtr); //extract val, trim whitespaces
                    if (strcmp(tmpValPtr,dtoverlay_name)!=0){goto skip;} //invalid dtoverlay
                    ret = line; break;
                }
            }
            skip:;
        }
        fclose(filehandle);
    }
    return ret;
}


//I2C/ADC functs/vars
int i2c_mcu_register_js0 = offsetof(struct i2c_joystick_register_struct, a0_msb) / sizeof(uint8_t); //adc0 start register index
int i2c_mcu_register_js1 = offsetof(struct i2c_joystick_register_struct, a2_msb) / sizeof(uint8_t); //adc2 start register index
int i2c_mcu_register_adc_conf = offsetof(struct i2c_joystick_register_struct, adc_conf_bits) / sizeof(uint8_t); //adc configuration register index
int i2c_mcu_register_adc_res = offsetof(struct i2c_joystick_register_struct, adc_res) / sizeof(uint8_t); //adc resolution register index
int i2c_mcu_register_config0 = offsetof(struct i2c_joystick_register_struct, config0) / sizeof(uint8_t); //adc resolution register index
bool i2c_failed_bus = true, i2c_failed_dev = true, i2c_failed_sig = true, i2c_failed = true; //used to check I2C specific failure

static int i2c_init(int* fd, int bus, int addr){ //open fd for I2C device, check device signature, get device id/version, ADC resolution, current device ADC configuration
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

    //current ADC resolution
    ret = i2c_smbus_read_byte_data(*fd, i2c_mcu_register_adc_res);
    if (ret < 0){close(*fd); return -1;}
    adc_res = ret; adc_res_limit = 0xFFFFFFFF >> (32 - adc_res); //compute adc limit value

    //current analog config
    ret = i2c_smbus_read_byte_data(*fd, i2c_mcu_register_adc_conf);
    if (ret < 0){close(*fd); return -1;}
    mcu_conf_current.bits = (uint8_t)ret;
    for (uint8_t i=0; i<4; i++){
        int_constrain(adc_settings[i].max, 0, adc_res_limit);
        int_constrain(adc_settings_back[i].max, 0, adc_res_limit);
        int_constrain(adc_settings_default[i].max, 0, adc_res_limit);
        adc_data[i].raw_min = adc_res_limit; adc_data[i].raw_max = 0;
        adc_reg_enable[i] = (bool)((mcu_conf_current.bits >> (i+4)) & 0b1);
        if (adc_reg_enable[i]){adc_reg_used_backup[i] = adc_reg_used[i] = adc_reg_used_prev[i] = (bool)((mcu_conf_current.bits >> i) & 0b1);}
    }
    if (!(adc_reg_used[0] && adc_reg_used[1])){*js_enabled[0] = false;}
    if (!(adc_reg_used[2] && adc_reg_used[3])){*js_enabled[1] = false;}
    js_enabled_prev[0] = *js_enabled[0]; js_enabled_prev[1] = *js_enabled[1];

    //current mcu config0
    ret = i2c_smbus_read_byte_data(*fd, i2c_mcu_register_config0);
    if (ret < 0){close(*fd); return -1;}
    mcu_config0_current.bits = mcu_config0_backup.bits = (uint8_t)ret;

    i2c_failed = false;
    return 0;
}

static int adc_defuzz(int value, int old_val, int fuzz){ //ADC defuzz, based on input_defuzz_abs_event(): https://elixir.bootlin.com/linux/latest/source/drivers/input/input.c#L56
	if (fuzz) {
		if (value > old_val - fuzz / 2 && value < old_val + fuzz / 2){return old_val;}
		if (value > old_val - fuzz && value < old_val + fuzz){return (old_val * 3 + value) / 4;}
		if (value > old_val - fuzz * 2 && value < old_val + fuzz * 2){return (old_val + value) / 2;}
	}
	return value;
}

static int adc_correct_offset_center(int adc_resolution, int adc_value, int adc_min, int adc_max, int adc_offset, int flat_in, int flat_out){ //apply offset center, expand adc range, inside/ouside flat, flat_in/out are values relative to adc resolution (not percent), direct copy from uhid driver
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


//"string"/char array functs
static void str_trim_whitespace(char** ptr){ //update pointer to skip leading pointer, set first trailing space to null char
    char *trimPtrE = *ptr + strlen(*ptr);
    while(isspace(*(trimPtrE-1))){trimPtrE--;} *trimPtrE='\0'; 
    while(isspace(*(*ptr))){(*ptr)++;}
}

static void array_fill(char* arr, int size, char chr){for (int i=0;i<size-1;i++){arr[i]=chr;} arr[size-1]='\0';} //fill array with given character, works with '\0' for full reset, last char set to '\0'

static int array_pad(char* arr, int arr_len, int size, char pad, int align){ //pad a array, 'align': 0:center 1:left 2:right, 'size':final array size, return padding length
    if (size < arr_len || pad == '\0' || arr[0] == '\0'){return 0;} //no padding to do
    char arr_backup[arr_len+1]; strcpy(arr_backup, arr); //backup original array
    int pad_len = size - arr_len; //padding length
    if (align==0){if (pad_len % 2 != 0){pad_len++;} pad_len /= 2;} //align to center, do necessary to have equal char count on both side
    char pad_buffer[pad_len+1]; 
    array_fill(pad_buffer, pad_len+1, pad); //generate padding array
    if (align != 1){
        array_fill(arr, arr_len, '\0'); //fully reset original array
        strcpy(arr, pad_buffer); strcat(arr, arr_backup);
        if (align == 0 && size-arr_len-pad_len >= 0){pad_buffer[size - arr_len - pad_len] = '\0'; strcat(arr, pad_buffer);}
    } else {strcat(arr, pad_buffer);}
    return pad_len;
}

static int strcpy_noescape(char* dest, char* src, int limit){ //strcpy "clone" that ignore terminal escape code, set dest=src or dest=NULL to only return "noescape" char array length. Current limitations:defined limit of escape code (w/o "\e["). warnings: no size check, broken if badly formated escape, only check for h,l,j,m ending
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


//integer manipulation functs
static void int_rollover(int* val, int min, int max){ //rollover int value between (incl) min and max, work both way
    if(*val < min){*val = max;} else if(*val > max){*val = min;}
}

static void int_constrain(int* val, int min, int max){ //limit int value to given (incl) min and max value
    if(*val < min){*val = min;} else if(*val > max){*val = max;}
}


//terminal functs
static void term_user_input(term_input_t* input){ //process terminal key inputs
    char term_read_char; char last_key[32] = {'\0'}; //debug, last char used
    memset(input, 0, sizeof(term_input_t)); //reset input struct
    if (read(STDIN_FILENO, &term_read_char, 1) > 0){
        if (term_read_char == '\n'){if(debug){strcpy(last_key, "ENTER");} input->enter = true;}
        else if (term_read_char == '\t'){if(debug){strcpy(last_key, "TAB");} input->tab = true;}
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
                char buffer[buffer_size] = {0}, buffer1[buffer_size] = {0}/*, buffer2[buffer_size] = {0}*/;
                int type = store[*index].type;
                
                if (store[*index].defval.ptrbool && (type == 1 || type == 2)){ //bool
                    if (type == 1){strcpy(buffer1, *(store[*index].defval.ptrbool)?"Enabled":"Disabled"); //bool
                    } else if (type == 2){strcpy(buffer1, *(store[*index].defval.ptrbool)?"X":"_");} //toogle
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

    bool selected;
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
                } else if (store[i].type == 2 && store[i].value.ptrbool){sprintf(buffer, "%s", *(store[i].value.ptrbool)?"X":"_"); tmpptr = buffer;} //toogle bool
                if (tmpptr){fprintf(stdout, "\e[%d;%dH\e[%d;%d;%d;4m%s\e[0m", store[i].position.y, store[i].position.x, tmpcol_style, tmpcol_bg, tmpcol_txt, tmpptr);}
            }
        }
    }
}

static int term_print_path_multiline(char* str, int line, int col, int width_limit, int esc_color){ //print a multiple line if needed, return no of lines
    int lines = 0; char buffer[buffer_size];
    array_fill(buffer, buffer_size, '\0');
    char str_back[strlen(str)+1]; strcpy(str_back, str);
    char *tmpPtr = strtok(str_back, "/");
	while (tmpPtr != NULL){
        if(strlen(buffer) + strlen(tmpPtr) + col + 2 > width_limit){
            fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[1m\xE2\x86\x93\e[21m\e[0m", line + lines++, col, esc_color, buffer);
            array_fill(buffer, buffer_size, '\0');
        }
        strcat(buffer, "/"); strcat(buffer, tmpPtr);
		tmpPtr = strtok (NULL, "/");
	}
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", line + lines++, col, esc_color, buffer);
    return lines;
}


//term screen functs
static void (*term_screen_funct_ptr[])(int, int, int) = {term_screen_main, term_screen_adc, term_screen_save, term_screen_digitaldebug}; //pointer to screen functions

static void term_screen_main(int tty_line, int tty_last_width, int tty_last_height){ //main screen:0
    char buffer[buffer_size];
    bool default_resquested = false, reset_resquested = false;
    int hint_line = tty_last_height - 6, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;

    char* term_hint_main_str[]={
        "Discard current modifications (exclude Analog Configuration)",
        "Reset values to default (exclude Analog Configuration)",
        "", //"Bus to use, change with caution",
        "Address of the device, bus used by default:1",
        "GPIO pin used for interrupt, -1 to disable",
        "Amount of reported digital buttons (excl Dpad)",
        "Enable/disable Dpad report",
        "Filter to limit impact of pads false contacts, 0 to disable",
        "Please refer to \e[4m[Digital Input Debug]\e[24m menu to for visual representation.",
        "Change enable ADCs, limits, fuzz, flat values",
        "Display digital inputs state",
        "Discard current modifications (exclude Analog Configuration)",
        "Reset values to default (exclude Analog Configuration)",
    };

    const int select_max = 11;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Kernel driver setup/diagnostic tool";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(programname))/2, term_esc_col_normal, programname);

    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);

    if (!i2c_failed){ //display device id/version if i2c not failed
        sprintf(buffer, "device id:0x%02X version:%d", i2c_dev_id, i2c_dev_minor);
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(buffer))/2, tmp_esc_col, buffer);
    }
    tty_line+=2;

    //i2c bus, commented because of the way dtoverlay
    /*
    if (i2c_failed_bus){tmp_esc_col = term_esc_col_error;}
    sprintf(buffer, "%s", i2c_failed_bus?term_hint_i2c_failed[0]:term_hint_main[0]);
    fprintf(stdout, "\e[%d;%dH\e[%dmI2C bus:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+8, .y=tty_line, .size=3}, .type=0, .value={.min=0, .max=255, .ptrint=&driver_user.bus}, .defval={.ptrint=&driver_default.bus, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic[1]}};
    if (i2c_failed_bus){goto i2c_failed_jump;} //skip other settings if i2c failed
    tty_line+=2;
    */

    //i2c address
    if (i2c_failed_dev){tmp_esc_col = term_esc_col_error;}
    sprintf(buffer, "%s", i2c_failed_dev?term_hint_i2c_failed_str[1]:term_hint_main_str[3]);
    fprintf(stdout, "\e[%d;%dH\e[%dmI2C address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+12, .y=tty_line, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&driver_user.addr}, .defval={.ptrint=&driver_default.addr, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
    if (i2c_failed_dev){goto i2c_failed_jump;} //skip other settings if i2c failed
    tty_line++;

    //report i2c signature
    if (!i2c_failed_sig){sprintf(buffer, "Detected device signature: 0x%02X", i2c_dev_sig);
    } else {tmp_esc_col = term_esc_col_error; sprintf(buffer, "Wrong device signature, was expecting 0x%02X but got 0x%02X", i2c_dev_manuf, i2c_dev_sig);}
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    if (i2c_failed_sig){goto i2c_failed_jump;}
    tty_line+=2;

    //interrupt pin
    fprintf(stdout, "\e[%d;%dH\e[%dmInterrupt:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main_str[4]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+10, .y=tty_line, .size=3}, .type=0, .value={.min=-1, .max=127, .ptrint=&driver_user.interrupt}, .defval={.ptrint=&driver_default.interrupt, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
    tty_line+=2;

    //buttons no
    fprintf(stdout, "\e[%d;%dH\e[%dmButtons:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main_str[5]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+8, .y=tty_line++, .size=3}, .type=0, .value={.min=0, .max=button_count_limit, .ptrint=&driver_user.digitalbuttons}, .defval={.ptrint=&driver_default.digitalbuttons, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
    //dpad enabled
    fprintf(stdout, "\e[%d;%dH\e[%dmDpad:_ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main_str[6]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+5, .y=tty_line++, .size=3}, .type=2, .value={.ptrbool=&driver_user.dpads}, .defval={.ptrbool=&driver_default.dpads, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
    //debounce
    fprintf(stdout, "\e[%d;%dH\e[%dmDebounce:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, term_hint_main_str[7]);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+9, .y=tty_line++, .size=3}, .type=0, .value={.min=0, .max=debounce_limit, .ptrint=&driver_user.debounce}, .defval={.ptrint=&driver_default.debounce, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};

    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, 0/*tmp_esc_col*/, term_hint_main_str[8]);
    tty_line+=2;

    //adc configuration
    bool term_go_adcconf = false;
    char* term_buttons_adc_config = " Analog Configuration ";
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_buttons_adc_config))/2, .y=tty_line, .size=strlen(term_buttons_adc_config)}, .type=1, .value={.ptrchar=term_buttons_adc_config, .ptrbool=&term_go_adcconf}, .hint={.y=hint_line, .str=term_hint_main_str[9]}};
    tty_line+=2;

    //digital debug
    bool term_go_digitaldebug = false;
    char* term_buttons_digitaldebug_config = " Digital Input Debug ";
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_buttons_digitaldebug_config))/2, .y=tty_line, .size=strlen(term_buttons_digitaldebug_config)}, .type=1, .value={.ptrchar=term_buttons_digitaldebug_config, .ptrbool=&term_go_digitaldebug}, .hint={.y=hint_line, .str=term_hint_main_str[10]}};

    i2c_failed_jump:; //jump point for i2c failure

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-5, (tty_last_width - strcpy_noescape(NULL, term_hint_generic_str[0], 20)) / 2, term_hint_generic_str[0]); //nav hint

    //buttons
    bool term_go_save = false;
    #define term_footer_main_btn0_count 2
    #define term_footer_main_btn1_count 2
    int term_buttons0_pad = (tty_last_width - term_footer_buttons_width * term_footer_main_btn0_count) / (term_footer_main_btn0_count + 1);
    int term_buttons1_pad = (tty_last_width - term_footer_buttons_width * term_footer_main_btn1_count) / (term_footer_main_btn1_count + 1);

    term_pos_button_t term_footer_main_btn0[term_footer_main_btn0_count] = {
        {.str="Discard", .ptrbool=&reset_resquested, .ptrhint=term_hint_main_str[0]},
        {.str="Default", .ptrbool=&default_resquested, .ptrhint=term_hint_main_str[1]},
    };

    term_pos_button_t term_footer_main_btn1[term_footer_main_btn1_count] = {
        {.str="Save", .ptrbool=&term_go_save, .ptrhint=term_hint_generic_str[2], .disabled=i2c_failed},
        {.str="Close", .ptrbool=&kill_resquested, .ptrhint=term_hint_generic_str[6]},
    };

    for (int i=0; i<term_footer_main_btn0_count; i++){
        int x = term_buttons0_pad + (term_footer_buttons_width + term_buttons0_pad) * i;
        array_pad(term_footer_main_btn0[i].str, strlen(term_footer_main_btn0[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-3, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_main_btn0[i].disabled, .value={.ptrchar=term_footer_main_btn0[i].str, .ptrbool=term_footer_main_btn0[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_main_btn0[i].ptrhint}};
    }

    for (int i=0; i<term_footer_main_btn1_count; i++){
        int x = term_buttons1_pad + (term_footer_buttons_width + term_buttons1_pad) * i;
        array_pad(term_footer_main_btn1[i].str, strlen(term_footer_main_btn1[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_main_btn1[i].disabled, .value={.ptrchar=term_footer_main_btn1[i].str, .ptrbool=term_footer_main_btn1[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_main_btn1[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    select_index_last = -1; //force selectible element update on first loop
    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        if (term_input.escape){select_index_current = select_limit-1;} //escape key pressed, move cursor to last selectible element
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_go_adcconf){term_screen_current = 1; goto funct_end;} //go to adc configuration screen requested
        if (term_go_save){term_screen_current = 2; goto funct_end;} //go to save screen requested
        if (term_go_digitaldebug){term_screen_current = 3; goto funct_end;} //go to digital debug requested

        if (reset_resquested){ //restore backup requested
            driver_user.bus = driver_back.bus; driver_user.addr = driver_back.addr;
            driver_user.interrupt = driver_back.interrupt; driver_user.digitalbuttons = driver_back.digitalbuttons; driver_user.dpads = driver_back.dpads;
            driver_user.debounce = driver_back.debounce;
        }

        if (default_resquested){ //restore defaults requested
            driver_user.bus = driver_default.bus; driver_user.addr = driver_default.addr;
            driver_user.interrupt = driver_default.interrupt; driver_user.digitalbuttons = driver_default.digitalbuttons; driver_user.dpads = driver_default.dpads;
            driver_user.debounce = driver_default.debounce;
        }

        if (driver_user.bus != i2c_bus_prev || driver_user.addr != i2c_addr_prev || reset_resquested || default_resquested){ //i2c bus or address changed
            i2c_bus_prev = driver_user.bus; i2c_addr_prev = driver_user.addr;
            i2c_init(&i2c_fd, driver_user.bus, driver_user.addr);
            if (reset_resquested || default_resquested){select_index_current = 0;}
            term_screen_update = true; goto funct_end; //force full redraw
        }

        //mcu config0 update
        if (driver_user.debounce != mcu_config0_current.vals.debounce_level){mcu_config0_current.vals.debounce_level = driver_user.debounce;}

        if (mcu_config0_current.bits != mcu_config0_backup.bits){ //done that way for possible config0 struct update
            i2c_smbus_write_byte_data(i2c_fd, i2c_mcu_register_config0, mcu_config0_current.bits); //update i2c config
            mcu_config0_backup.bits = mcu_config0_current.bits;
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (10000);
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

static void term_screen_adc(int tty_line, int tty_last_width, int tty_last_height){ //display adc screen:1
    term_pos_generic_t term_adc_raw[4] = {0}, term_adc_output[4] = {0};
    term_pos_string_t term_adc_string[4] = {0};
    bool adc_use_raw_min[4] = {0}, adc_use_raw_max[4] = {0};
    bool default_resquested = false, reset_resquested = false, reset_raw_limits_resquested = false, term_go_mainscreen = false;

    char buffer[buffer_size], buffer1[buffer_size];
    int term_adc_pad = (tty_last_width - term_adc_width * 2) / 3; //padding between each ADC sections
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1;

    char* term_hint_adc_str[]={
        "Discard current modifications (exclude Main Settings)",
        "Reset values to default (exclude Main Settings)",
        "Reset min/max detected values",
        "Press \e[1m[ENTER]\e[0m to enable or disable",
        "Press \e[1m[ENTER]\e[0m to switch axis direction",
        "Press \e[1m[ENTER]\e[0m to set as MIN limit value",
        "Press \e[1m[ENTER]\e[0m to set as MAX limit value",
        "Press \e[1m[ENTER]\e[0m to enable axis swap, apply only on driver",
    };

    const int select_max = 40;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Analog Configuration";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    for(int x_loop=0, adc_loop=0; x_loop<2; x_loop++){
        int term_left = term_adc_pad + (term_adc_width + term_adc_pad) * x_loop, term_right = term_left + term_adc_width, tmp_line = tty_line, tmp_line_last = tty_line; //left/right border of current adc
        int x, x1, x2, w;
        bool js_used = adc_reg_used[2*x_loop] && adc_reg_used[2*x_loop+1];

        //enable joystick
        sprintf(buffer, "Joystick %d enabled:-", x_loop);
        x = 1 + term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line++, .size=1}, .type=2, .disabled=!js_used, .value={.ptrbool=js_enabled[x_loop]}, .defval={.y=hint_def_line, .ptrbool=js_enabled_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[3]}};

        for(int y_loop=0; y_loop<2; y_loop++){
            int term_esc_col = term_esc_col_disabled;
            bool adc_used = adc_reg_used[adc_loop], adc_enabled = adc_reg_enable[adc_loop];
            tmp_line++;

            //adc "title"
            if (adc_used){sprintf (buffer1, "%dbits", adc_res); term_esc_col = term_esc_col_normal;} else if (adc_enabled){sprintf (buffer1, "available");} else {sprintf (buffer1, "disabled");}
            sprintf(buffer, "ADC%d(%s)(%s%s)", adc_loop, adc_data[adc_loop].name, buffer1, (adc_enabled && (!js_used || !(*js_enabled[x_loop])))?",unused":""); strcpy(term_adc_string[adc_loop].str, buffer);
            x = term_left + array_pad(buffer, strlen(buffer), term_adc_width, '_', 0); w = strlen(buffer1);
            fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tmp_line, term_left, term_esc_col, buffer);
            if (adc_enabled){term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=w}, .type=1, .value={.ptrchar=term_adc_string[adc_loop].str, .ptrbool=&adc_reg_used[adc_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[3]}};}
            tmp_line++;

            //limits
            x = term_right - 17; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmlimits\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_res_limit, .force_update=true, .ptrint=adc_settings[adc_loop].min}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].min}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_res_limit, .force_update=true, .ptrint=adc_settings[adc_loop].max}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].max}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
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
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=adc_settings[adc_loop].reversed}, .defval={.y=hint_def_line, .ptrbool=adc_settings_default[adc_loop].reversed}, .hint={.y=hint_line, .str=term_hint_adc_str[4]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_min[adc_loop], .ptrint=&adc_data[adc_loop].raw_min}, .hint={.y=hint_line, .str=term_hint_adc_str[5]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_max[adc_loop], .ptrint=&adc_data[adc_loop].raw_max}, .hint={.y=hint_line, .str=term_hint_adc_str[6]}};
            }
            tmp_line+=2;

            //flat, fuzz
            x = term_left + 5; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmflat:------\e[0m", tmp_line, x - 5, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmfuzz:------\e[0m", tmp_line, x1 - 5, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.min=0, .max=adc_res_limit/2, .ptrint=adc_settings[adc_loop].flat}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].flat}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.min=0, .max=adc_res_limit/2, .ptrint=adc_settings[adc_loop].fuzz}, .defval={.y=hint_def_line, .ptrint=adc_settings_default[adc_loop].fuzz}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
            }
            tmp_line += 2; tmp_line_last = tmp_line; adc_loop++;
        }

        //swap xy axis
        sprintf(buffer, "Swap %s/%s axis:-", adc_data[adc_loop-2].name, adc_data[adc_loop-1].name);
        x = term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line_last, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line, .size=1}, .type=2, .disabled=!js_used, .value={.ptrbool=adc_axis_swap[x_loop]}, .defval={.y=hint_def_line, .ptrbool=adc_axis_swap_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[7]}};
    }

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_generic_str[0], 20)) / 2, term_hint_generic_str[0]); //nav hint

    //buttons
    #define term_footer_adc_btn_count 4
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_adc_btn_count) / (term_footer_adc_btn_count + 1);
    term_pos_button_t term_footer_adc_btn[term_footer_adc_btn_count] = {
        {.str="Reset limits", .ptrbool=&reset_raw_limits_resquested, .ptrhint=term_hint_adc_str[2], .disabled=i2c_failed},
        {.str="Discard", .ptrbool=&reset_resquested, .ptrhint=term_hint_adc_str[0], .disabled=i2c_failed},
        {.str="Default", .ptrbool=&default_resquested, .ptrhint=term_hint_adc_str[1], .disabled=i2c_failed},
        {.str="Back", .ptrbool=&term_go_mainscreen, .ptrhint=term_hint_generic_str[5]},
    };
    for (int i=0; i<term_footer_adc_btn_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_adc_btn[i].str, strlen(term_footer_adc_btn[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_adc_btn[i].disabled, .value={.ptrchar=term_footer_adc_btn[i].str, .ptrbool=term_footer_adc_btn[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_adc_btn[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    select_index_last = -1; //force selectible element update on first loop
    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        if (!i2c_failed){
            fprintf(stdout, "\e[%d;0H\e[2K", tty_last_height-6); //reset i2c error line

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

        if (term_input.escape){term_go_mainscreen = true; /*select_index_current = select_limit-1;*/} //escape key pressed, move cursor to last selectible element
        if (term_go_mainscreen){term_screen_current = 0; goto funct_end;} //return to main screen requested

        if (reset_raw_limits_resquested){
            for(int i=0; i<4; i++){adc_data[i].raw_min = INT_MAX; adc_data[i].raw_max = INT_MIN;}
            term_screen_update = true; goto funct_end; //force full redraw
        }

        if (reset_resquested){ //restore backup requested
            for (int i=0; i<4; i++){
                *adc_settings[i].min = *adc_settings_back[i].min;
                *adc_settings[i].max = *adc_settings_back[i].max;
                *adc_settings[i].fuzz = *adc_settings_back[i].fuzz;
                *adc_settings[i].flat = *adc_settings_back[i].flat;
                *adc_settings[i].reversed = *adc_settings_back[i].reversed;
                adc_reg_used[i] = adc_reg_used_backup[i]; //replace by *adc_settings[i].enabled if dtoverlay updated to allow individual ADC
            }
            *js_enabled[0] = *js_enabled_backup[0]; *js_enabled[1] = *js_enabled_backup[1]; //js enabled
            *adc_axis_swap[0] = *adc_axis_swap_backup[0]; *adc_axis_swap[1] = *adc_axis_swap_backup[1]; //axis swap
            select_index_current = 0; term_screen_update = true; goto funct_end; //force full redraw
        }

        if (default_resquested){ //restore defaults requested
            for (int i=0; i<4; i++){
                *adc_settings[i].min = *adc_settings_default[i].min;
                *adc_settings[i].max = *adc_settings_default[i].max;
                *adc_settings[i].fuzz = *adc_settings_default[i].fuzz;
                *adc_settings[i].flat = *adc_settings_default[i].flat;
                *adc_settings[i].reversed = *adc_settings_default[i].reversed;
                adc_reg_used[i] = adc_reg_used_backup[i]; //replace by *adc_settings[i].enabled if dtoverlay updated to allow individual ADC
            }
            *js_enabled[0] = *js_enabled_default[0]; *js_enabled[1] = *js_enabled_default[1]; //js enabled
            *adc_axis_swap[0] = *adc_axis_swap_default[0]; *adc_axis_swap[1] = *adc_axis_swap_default[1]; //axis swap
            select_index_current = 0; term_screen_update = true; goto funct_end; //force full redraw
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
                } else {adc_reg_used_prev[i] = adc_reg_used[i] = false;} //adc not enabled
            }

            //update joystick enabled based on adc enabled
            if (*js_enabled[0]){*js_enabled[0] = adc_reg_used[0] && adc_reg_used[1];}
            if (*js_enabled[1]){*js_enabled[1] = adc_reg_used[2] && adc_reg_used[3];}

            if (*js_enabled[0] != js_enabled_prev[0] || *js_enabled[1] != js_enabled_prev[1] || mcu_conf_changed){ //enabled joystick or adc changed
                js_enabled_prev[0] = *js_enabled[0]; js_enabled_prev[1] = *js_enabled[1];
                i2c_smbus_write_byte_data(i2c_fd, i2c_mcu_register_adc_conf, mcu_conf_current.bits); //update i2c config
                term_screen_update = true; goto funct_end; //force full redraw
            }
        } else { //i2c failed
            bool i2c_valid_bool[4] = {i2c_failed_bus, i2c_failed_dev, i2c_failed_sig, i2c_failed};
            for (int i=0; i<4; i++){
                if (i2c_valid_bool[i]){
                    sprintf(buffer, "Error: %s", term_hint_i2c_failed_str[i]);
                    fprintf(stdout, "\e[%d;%dH\e[2K\e[1;%dm%s\e[0m", tty_last_height-6, (tty_last_width - strlen(buffer)) / 2, term_esc_col_error, buffer);
                    break;
                }
            }
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

static void term_screen_save(int tty_line, int tty_last_width, int tty_last_height){ //save screen:2
    //char buffer[buffer_size];
    int hint_line = tty_last_height - 4, tmp_col = 2; 
    bool term_go_mainscreen = false;

    char* term_hint_save_str[]={
    "Saved successfully",
    "Something went wrong, failed to save",
    "Warning, can be unsafe.",
    "Risk to mess a needed system file if something goes wrong.",
    "If you have direct/SSH access, go for following option.",
    "File will be saved in program folder:",
    "File will be saved to:",
    "Regardless of selected option, previous file will backup as *FILE*.bak.fpjs",
    };

    const int select_max = 5;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Save current settings";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    //save to /boot/config.txt
    bool save_boot_resquested = false; int save_boot_btn_index = select_limit;
    char term_buttons_save_configtxt[12+strlen(dtoverlay_filename)]; sprintf(term_buttons_save_configtxt, " Save to %s ", dtoverlay_filename);
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_buttons_save_configtxt)) / 2, .y=tty_line++, .size=strlen(term_buttons_save_configtxt)}, .type=1, .value={.ptrchar=term_buttons_save_configtxt, .ptrbool=&save_boot_resquested}, .hint={.y=hint_line, .str=term_hint_save_str[7]}};
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[2], 20)) / 2, term_esc_col_error, term_hint_save_str[2]);
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[3], 20)) / 2, term_esc_col_error, term_hint_save_str[3]);
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[4], 20)) / 2, term_esc_col_error, term_hint_save_str[4]);
    tty_line++;

    //save to dtoverlay.txt
    bool save_text_resquested = false; int save_text_btn_index = select_limit;
    char term_buttons_save_dtoverlay_txt[12+strlen(dtoverlay_text_filename)]; sprintf(term_buttons_save_dtoverlay_txt, " Save to %s ", dtoverlay_text_filename);
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_buttons_save_dtoverlay_txt)) / 2, .y=tty_line++, .size=strlen(term_buttons_save_dtoverlay_txt)}, .type=1, .value={.ptrchar=term_buttons_save_dtoverlay_txt, .ptrbool=&save_text_resquested}, .hint={.y=hint_line, .str=term_hint_save_str[7]}};
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[6], 20)) / 2, term_esc_col_normal, term_hint_save_str[6]);
    char tmp_path[strlen(program_path) + strlen(dtoverlay_text_filename) + 5]; sprintf(tmp_path, "%s/%s", program_path, dtoverlay_text_filename); //full path to dtoverlay.txt
    tty_line += term_print_path_multiline(tmp_path, tty_line, tmp_col, tty_last_width, term_esc_col_normal) + 1; //multiline program path

    //save install command
    bool save_command_resquested = false; int save_command_btn_index = select_limit;
    char* term_buttons_save_dtoverlay_cmd = " Save dtoverlay load script ";
    if (strlen(dtoverlay_install_cmd_path)){
        term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_buttons_save_dtoverlay_cmd)) / 2, .y=tty_line++, .size=strlen(term_buttons_save_dtoverlay_cmd)}, .type=1, .value={.ptrchar=term_buttons_save_dtoverlay_cmd, .ptrbool=&save_command_resquested}, .hint={.y=hint_line, .str=term_hint_save_str[7]}};
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[2], 20)) / 2, term_esc_col_error, term_hint_save_str[2]);
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strcpy_noescape(NULL, term_hint_save_str[6], 20)) / 2, term_esc_col_normal, term_hint_save_str[6]);
        tty_line += term_print_path_multiline(dtoverlay_install_cmd_path, tty_line, tmp_col, tty_last_width, term_esc_col_normal) + 1; //multiline command path
    }

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_generic_str[0], 20)) / 2, term_hint_generic_str[0]); //nav hint

    //buttons
    #define term_footer_save_btn_count 2
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_save_btn_count) / (term_footer_save_btn_count + 1);
    term_pos_button_t term_footer_save_btn[term_footer_save_btn_count] = {
        {.str="Close", .ptrbool=&kill_resquested, .ptrhint=term_hint_generic_str[7]},
        {.str="Back", .ptrbool=&term_go_mainscreen, .ptrhint=term_hint_generic_str[5]},
    };
    for (int i=0; i<term_footer_save_btn_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_save_btn[i].str, strlen(term_footer_save_btn[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_save_btn[i].disabled, .value={.ptrchar=term_footer_save_btn[i].str, .ptrbool=term_footer_save_btn[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_save_btn[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_input.escape){term_go_mainscreen = true; /*select_index_current = select_limit-1;*/} //escape key pressed, move cursor to last selectible element
        if (term_go_mainscreen){term_screen_current = 0; goto funct_end;} //return to main screen requested

        if (save_boot_resquested || save_text_resquested || save_command_resquested){
            bool debug_back = debug; debug = false; int ret = -1, button_index = 0;

            if (save_boot_resquested){
                ret = dtoverlay_save(dtoverlay_filename, dtoverlay_name, dtoverlay_store, dtoverlay_store_size);
                button_index = save_boot_btn_index; save_boot_resquested = false;
            } else if (save_text_resquested){
                char tmppath[strlen(program_path)+strlen(dtoverlay_text_filename)+3]; sprintf(tmppath, "%s/%s", program_path, dtoverlay_text_filename);
                ret = dtoverlay_save(tmppath, dtoverlay_name, dtoverlay_store, dtoverlay_store_size);
                button_index = save_text_btn_index; save_text_resquested = false;
            } else { //save_command_resquested, save load command script
                unsigned int uid = getuid(), gid = getgid(), mode = 0744;
                struct stat file_stat = {0}, file_back_stat = {0};
                if (stat(dtoverlay_install_cmd_path, &file_stat) == 0){ //get original file rights and owner, rename existing file
                    uid = file_stat.st_uid; gid = file_stat.st_gid; mode = file_stat.st_mode; 
                    char filename_backup[strlen(dtoverlay_install_cmd_path)+10]; sprintf(filename_backup, "%s.bak.fpjs", dtoverlay_install_cmd_path); //backup filename
                    if (stat(filename_backup, &file_back_stat) == 0){remove(filename_backup);} //backup file already exist, delete it
                    if (rename(dtoverlay_install_cmd_path, filename_backup) != 0){ret = -2;} //rename original file to backup failed
                } else {uid = getuid(); gid = getgid();} //get program file rights and owner if original file not exist

                if (ret != -2){
                    FILE *filehandle_tmp = fopen(dtoverlay_install_cmd_path, "w");
                    if (filehandle_tmp){
                        char strBuffer[4096]={'\0'}; dtoverlay_generate(strBuffer, 4096, dtoverlay_name, ' ', dtoverlay_store, dtoverlay_store_size);
                        fputs("sudo dtoverlay -v ", filehandle_tmp); fputs(strBuffer, filehandle_tmp); fputs("\n", filehandle_tmp); //copy line to temporary file
                        ret = fchown(fileno(filehandle_tmp), uid, gid);
                        ret += fchmod(fileno(filehandle_tmp), mode);
                        ret += fclose(filehandle_tmp);
                    }
                    if (ret > 0){ret=0;} //valid
                }

                button_index = save_command_btn_index; save_command_resquested = false;
            }

            char* save_hint_ptr = term_hint_save_str[0]; int tmp_esc_col = term_esc_col_success;
            if (ret != 0){save_hint_ptr = term_hint_save_str[1]; tmp_esc_col = term_esc_col_error;} //failed
            fprintf(stdout, "\e[%d;%dH\e[2K\e[%dm%s\e[0m", hint_line-2, (tty_last_width - strcpy_noescape(NULL, save_hint_ptr, 20)) / 2, tmp_esc_col, save_hint_ptr);
            term_select[button_index].disabled = true; save_boot_resquested = false; debug = debug_back; select_index_current++;
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (10000);
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


static void term_screen_digitaldebug(int tty_line, int tty_last_width, int tty_last_height){ //digital input debug:3
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, /*hint_def_line = hint_line - 1, */tmp_col = 2/*, tmp_esc_col = term_esc_col_normal*/; 
    bool term_go_mainscreen = false;

    const int select_max = 1;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;
    term_pos_string_t term_input_pos[input_values_count] = {0};

    char* term_hint_digitaldebug_str[]={
    "Please note that reported/enabled order may defer depending on driver itself.",
    "Enabling more/less digital inputs may not result wanted result.",
    };

    char* input_name[input_registers_count] = {"input0", "input1", "input2"};

    char* screen_name = "Digital input registers states";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    if (!i2c_failed){
        for (int line = 0, input_index = 0, input_usable = 0; line < input_registers_count; line++){
            fprintf(stdout, "\e[%d;%dH\e[1;4m%s:\e[0m", tty_line++, tmp_col, input_name[line]); array_fill(buffer, buffer_size, '\0'); //inputX:
            
            for (int i = 0; i < 8; i++, input_index++){
                int input_name_len = strlen(button_names[input_index]);

                //input button name
                sprintf(buffer1, " %d", i);
                if ((input_index < dpad_start_index || input_index > dpad_start_index + 3) && input_name_len){sprintf(buffer2, "(%d)", ++input_usable); strcat(buffer1, buffer2);} //not dpad, and valid button

                if(input_name_len){sprintf(buffer2, ":%s", button_names[input_index]); strcat(buffer1, buffer2);}

                strcat(buffer1, " ");
                strcpy(term_input_pos[input_index].str, buffer1);

                int tmpx = strcpy_noescape(NULL, buffer, 20); //current button x position
                if(tmpx + strcpy_noescape(NULL, buffer1, 20) + tmp_col + 1 > tty_last_width){tty_line+=2; tmpx = 0; array_fill(buffer, buffer_size, '\0');} //new line
                strcat(buffer, buffer1); strcat(buffer, "  ");

                term_input_pos[input_index].x = tmp_col + tmpx; term_input_pos[input_index].y = tty_line; //button position
                fprintf(stdout, "\e[%d;%dHX\e[4m%s\e[0m ", term_input_pos[input_index].y, term_input_pos[input_index].x, term_input_pos[input_index].str);
            }
            tty_line+=2;
        }
    }

    //footer
    for (int i=0; i<2; i++){fprintf(stdout, "\e[%d;%dH\e[2K\e[93m%s\e[0m", tty_last_height-8+i, (tty_last_width - strcpy_noescape(NULL, term_hint_digitaldebug_str[i], 20)) / 2, term_hint_digitaldebug_str[i]);} //warning
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_generic_str[0], 20)) / 2, term_hint_generic_str[0]); //nav hint

    //buttons
    #define term_footer_digitaldebug_btn_count 1
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_digitaldebug_btn_count) / (term_footer_digitaldebug_btn_count + 1);
    term_pos_button_t term_footer_save_btn[term_footer_digitaldebug_btn_count] = {
        {.str="Back", .ptrbool=&term_go_mainscreen, .ptrhint=term_hint_generic_str[5]},
    };
    for (int i=0; i<term_footer_digitaldebug_btn_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_save_btn[i].str, strlen(term_footer_save_btn[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_save_btn[i].disabled, .value={.ptrchar=term_footer_save_btn[i].str, .ptrbool=term_footer_save_btn[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_save_btn[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_input.escape){term_go_mainscreen = true; /*select_index_current = select_limit-1;*/} //escape key pressed, move cursor to last selectible element
        if (term_go_mainscreen){term_screen_current = 0; goto funct_end;} //return to main screen requested

        if (!i2c_failed){
            fprintf(stdout, "\e[%d;0H\e[2K", tty_last_height-6); //reset i2c error line
            int ret = i2c_smbus_read_i2c_block_data(i2c_fd, 0, input_registers_count, (uint8_t *)&i2c_joystick_registers);
            if (ret >= 0){
                uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0;

                for (int i=0, input_usable = 0; i < input_values_count; i++){
                    int tmpcol_bg = 100, tmpcol_txt = 97, tmpcol_style = 0, tmpcol_enable = 49;

                    if ((i < dpad_start_index || i > dpad_start_index + 3)){ //not dpad
                        if (strlen(button_names[i]) && ++input_usable <= driver_user.digitalbuttons){tmpcol_enable = 42;} //valid button is used in driver report
                    } else if (driver_user.dpads){tmpcol_enable = 42;} //dpad is enabled

                    if (~(inputs >> i) & 0b1){tmpcol_bg = 47; tmpcol_txt = 30; tmpcol_style = 1 ;} //current input "high"
                    fprintf(stdout, "\e[%d;%dH\e[%dm \e[0m\e[%d;%d;%dm%s\e[0m", term_input_pos[i].y, term_input_pos[i].x, tmpcol_enable, tmpcol_style, tmpcol_txt, tmpcol_bg, term_input_pos[i].str);
                }
            }
        } else { //i2c failed
            bool i2c_valid_bool[4] = {i2c_failed_bus, i2c_failed_dev, i2c_failed_sig, i2c_failed};
            for (int i=0; i<4; i++){
                if (i2c_valid_bool[i]){
                    sprintf(buffer, "Error: %s", term_hint_i2c_failed_str[i]);
                    fprintf(stdout, "\e[%d;%dH\e[2K\e[1;%dm%s\e[0m", tty_last_height-6, (tty_last_width - strlen(buffer)) / 2, term_esc_col_error, buffer);
                    break;
                }
            }
        }

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

//main functs
static void tty_signal_handler(int sig){ //signal handle func
	if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
    if (sizeof(term_backup)){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
	kill_resquested = true;
}

static void program_usage(char* program){ //program usage, obviously
	fprintf(stdout, "Version: %s\n", programversion);
	//fprintf(stdout, "Example : %s ...\n", program);
	fprintf(stdout, "Need to run as root.\n"
	"Arguments:\n"
	"\t-h or -help: show arguments list.\n"
    "\t-termreset: reset terminal to a pseudo \"default\" state\n"
	);
}

static void term_reset_default(void){ //run when terminal reset requested
    struct termios term_new;
    tcgetattr(STDIN_FILENO, &term_new);
    term_new.c_lflag |= (ECHO | ECHONL | ICANON); //enable input characters, new line character echo, canonical input
    tcsetattr(STDIN_FILENO, TCSANOW, &term_new);
    fprintf(stdout, "\e[?25h"); //show cursor
    print_stdout("terminal reset to \"default\"\n");
}

static void program_close(void){ //run when program closes
    if (sizeof(term_backup)){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct
    close(i2c_fd);
}

int main(int argc, char** argv){
    program_start_time = get_time_double();
	if (getuid() != 0) {print_stderr("FATAL: this program needs to run as root, current user:%d\n", getuid()); program_usage(argv[0]); return EXIT_FAILURE;} //not running as root
    program_get_path(argv, program_path); //get current program path

	for(int i=1; i<argc; ++i){ //program arguments parse
		if (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0){program_usage(argv[0]); return 0;} //-h -help argument
		else if (strcmp(argv[i],"-termreset") == 0){term_reset_default(); return 0;} //reset terminal "default" states
		else if (strcmp(argv[i],"-whatever") == 0){/*something*/}
	}

    //dtoverlay
    memcpy(&driver_user, &driver_default, sizeof(driver_default));
    dtoverlay_parser(dtoverlay_filename, dtoverlay_name, dtoverlay_store, dtoverlay_store_size);
    int_constrain(&driver_user.debounce, 0, debounce_limit);
    memcpy(&driver_back, &driver_user, sizeof(driver_user)); //backup detected dtoverlay settings

    //i2c
    i2c_bus_prev = driver_user.bus; i2c_addr_prev = driver_user.addr;
    i2c_init(&i2c_fd, driver_user.bus, driver_user.addr);

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle
	signal(SIGABRT, tty_signal_handler); //failure

    //disable STDIN print/tty setting backup
    struct termios term_new;
    if (tcgetattr(STDIN_FILENO, &term_backup) != 0){print_stderr("failed to backup current terminal data\n"); program_close(); return EXIT_FAILURE;}
    if (atexit(program_close) != 0 || at_quick_exit(program_close) != 0){print_stderr("failed to set atexit() to restore terminal data\n"); program_close(); return EXIT_FAILURE;}
    if (tcgetattr(STDIN_FILENO, &term_new) != 0){print_stderr("failed to save current terminal data for updates\n"); program_close(); return EXIT_FAILURE;}
    term_new.c_lflag &= ~(ECHO | ECHONL | ICANON); //disable input characters, new line character echo, disable canonical input to avoid needs of enter press for user input submit
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new) != 0){print_stderr("tcsetattr term_new failed\n"); program_close(); return EXIT_FAILURE;}

    //start term
    tty_start:; //landing point if tty is resized or "screen" changed or bool trigger
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); //set stdin to non-blocking
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); int tty_last_width = ws.ws_col, tty_last_height = ws.ws_row, tty_start_line = 2; //tty size
    fprintf(stdout, "\e[?25l\e[2J"); //hide cursor, reset tty
    if (debug){fprintf(stdout, "\e[1;1H\e[100mtty:%dx%d, screen:%d\e[0m", tty_last_width, tty_last_height, term_screen_current);} //print tty size, 640x480 is 80cols by 30rows

    //reset selectible
    select_index_last = -1;
    //memset(term_select, 0, sizeof(term_select));
    if (term_screen_current != term_screen_last){select_index_current = 0; term_screen_last = term_screen_current;} //screen changed, reset select index

    term_screen_funct_ptr[term_screen_current](tty_start_line, tty_last_width, tty_last_height); //current "screen" function
    if (term_screen_current != term_screen_last || term_screen_update){term_screen_update = false; goto tty_start;} //reset screen

    tcflush(STDOUT_FILENO, TCIOFLUSH); //flush STDOUT
    fprintf(stdout, "\e[0;0H\e[2J\e[?25h"); //reset tty, show cursor
    program_close(); //restore tty original state

    return 0;
}
