/*
* FreeplayTech UHID gamepad driver
* Diagnostic part header
*/


#include "driver_config.h"
#include "driver_debug_print.h"
#include "driver_i2c_registers.h"
#include "driver_diag.h"


void vars_main_default(){ //reset all main config vars to default
    debug = def_debug;
    debug_adv = def_debug_adv;
    //i2c_poll_rate_disable = def_i2c_poll_rate_disable;

    i2c_poll_rate = def_i2c_poll_rate;
    i2c_adc_poll = def_i2c_adc_poll;

    i2c_bus = def_i2c_bus;
    i2c_addr = def_i2c_addr;
    i2c_addr_sec = def_i2c_addr_sec;
    i2c_addr_adc[0] = def_i2c_addr_adc0;
    i2c_addr_adc[1] = def_i2c_addr_adc1;
    i2c_addr_adc[2] = def_i2c_addr_adc2;
    i2c_addr_adc[3] = def_i2c_addr_adc3;

    irq_gpio = def_irq_gpio;
    digital_debounce = def_digital_debounce;
}

void vars_adc_default(){ //reset all adc config vars to default
    mcu_js_enable[0] = def_js0_enable;
    mcu_js_enable[1] = def_js1_enable;

    uhid_js_swap = def_uhid_js_swap;
    uhid_js_swap_axis[0] = def_uhid_js0_swap_axis;
    uhid_js_swap_axis[1] = def_uhid_js1_swap_axis;

    adc_params[0].enabled = def_js0_enable;
    adc_params[0].res = def_adc0_res;
    adc_params[0].min = def_adc0_min;
    adc_params[0].max = def_adc0_max;
    adc_params[0].fuzz = def_adc0_fuzz;
    adc_params[0].flat_in = adc_params[0].flat_out = def_adc0_flat;
    adc_params[0].reversed = def_adc0_reversed;
    adc_params[0].autocenter = def_adc0_autocenter;

    adc_params[1].enabled = def_js0_enable;
    adc_params[1].res = def_adc1_res;
    adc_params[1].min = def_adc1_min;
    adc_params[1].max = def_adc1_max;
    adc_params[1].fuzz = def_adc1_fuzz;
    adc_params[1].flat_in = adc_params[1].flat_out = def_adc1_flat;
    adc_params[1].reversed = def_adc1_reversed;
    adc_params[1].autocenter = def_adc1_autocenter;

    adc_params[2].enabled = def_js1_enable;
    adc_params[2].res = def_adc2_res;
    adc_params[2].min = def_adc2_min;
    adc_params[2].max = def_adc2_max;
    adc_params[2].fuzz = def_adc2_fuzz;
    adc_params[2].flat_in = adc_params[2].flat_out = def_adc2_flat;
    adc_params[2].reversed = def_adc2_reversed;
    adc_params[2].autocenter = def_adc2_autocenter;

    adc_params[3].enabled = def_js1_enable;
    adc_params[3].res = def_adc3_res;
    adc_params[3].min = def_adc3_min;
    adc_params[3].max = def_adc3_max;
    adc_params[3].fuzz = def_adc3_fuzz;
    adc_params[3].flat_in = adc_params[3].flat_out = def_adc3_flat;
    adc_params[3].reversed = def_adc3_reversed;
    adc_params[3].autocenter = def_adc3_autocenter;
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
                    *store[i].value.ptrint = tmpval;
                    if(store[i].value.ptrbool){*store[i].value.ptrbool = !(*store[i].value.ptrbool);} //toogle ptrbool if set along to ptrint
                    update = true;
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

int program_diag_mode(){ //main diag function
    //disable STDIN print/tty setting backup
    struct termios term_new;
    if (tcgetattr(STDIN_FILENO, &term_backup) != 0){print_stderr("failed to backup current terminal data\n"); exit(EXIT_FAILURE);}
    if (tcgetattr(STDIN_FILENO, &term_new) != 0){print_stderr("failed to save current terminal data for updates\n"); exit(EXIT_FAILURE);}
    term_new.c_lflag &= ~(ECHO | ECHONL | ICANON); //disable input characters, new line character echo, disable canonical input to avoid needs of enter press for user input submit
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new) != 0){print_stderr("tcsetattr term_new failed\n"); exit(EXIT_FAILURE);}

    diag_mode_init = true;

    //debug_backup = debug; debug_adv_backup = debug_adv; //backup debug bools

    void (*term_screen_funct_ptr[])(int, int, int) = {term_screen_main, term_screen_adc, term_screen_digital, term_screen_save}; //pointer to screen functions

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

    //debug = debug_backup; debug_adv = debug_adv_backup; //restore debug bools
    diag_mode_init = false;
	return 0;
}




void term_screen_main(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size]/*, buffer1[buffer_size], buffer2[buffer_size]*/;
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_adc = false, term_go_screen_digital = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Kernel driver setup/diagnostic tool";
    //fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(uhid_device_name))/2, term_esc_col_normal, uhid_device_name);
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line++;

    bool i2c_update = false, i2c_failed = false/*, manuf_failed = false, config0_failed = false*/;
    
    //bus
    bool i2c_bus_update = false;
    int i2c_bus_default = def_i2c_bus;
    if (i2c_bus != i2c_bus_back){
        i2c_bus_back = i2c_bus; i2c_bus_update = true;
        if (i2c_check_bus(i2c_bus) != 0){i2c_bus_err = errno; i2c_bus_failed = true;} else {i2c_bus_failed = false;}
    }

    if (i2c_bus_failed){
        tmp_esc_col = term_esc_col_error;
        sprintf(buffer, "Failed to open I2C bus, %s", strerror(i2c_bus_err));
    } else {strcpy(buffer, "Bus to use for I2C device(s)");}

    fprintf(stdout, "\e[%d;%dH\e[%dmBus:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+4, .y=tty_line++, .size=3}, .type=0, .value={.min=0, .max=255, .ptrint=&i2c_bus, .ptrbool=&i2c_update}, .defval={.ptrint=&i2c_bus_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    if (i2c_bus_failed){goto i2c_failed_jump;} //bus failed

    //main address
    int i2c_addr_default = def_i2c_addr;
    if (i2c_addr != i2c_addr_back || i2c_bus_update){
        i2c_addr_back = i2c_addr;
        if (i2c_open_dev(&i2c_fd, i2c_bus, i2c_addr) != 0){i2c_main_err = errno;}
    }

    if (i2c_fd < 0){ //failed
        i2c_failed = true;
        tmp_esc_col = term_esc_col_error;
        sprintf(buffer, "Invalid address, %s", strerror(i2c_main_err));
    } else {strcpy(buffer, "Main I2C address, used for inputs");}

    fprintf(stdout, "\e[%d;%dH\e[%dmMain address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+13, .y=tty_line++, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&i2c_addr, .ptrbool=&i2c_update}, .defval={.ptrint=&i2c_addr_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    if (i2c_failed){goto i2c_failed_jump;} //main address failed

    //second address
    int i2c_addr_sec_default = def_i2c_addr_sec;
    if (i2c_addr_sec != i2c_addr_sec_back || i2c_bus_update){
        i2c_addr_sec_back = i2c_addr_sec;
        if (i2c_open_dev(&i2c_fd_sec, i2c_bus, i2c_addr_sec) != 0){i2c_sec_err = errno;}
    }

    if (i2c_fd_sec < 0){ //failed
        //i2c_failed = true;
        tmp_esc_col = term_esc_col_error;
        sprintf(buffer, "Invalid address, %s", strerror(i2c_sec_err));
    } else {strcpy(buffer, "Secondary I2C address, used for additionnal features");}
    
    fprintf(stdout, "\e[%d;%dH\e[%dmSecond address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+15, .y=tty_line++, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&i2c_addr_sec, .ptrbool=&i2c_update}, .defval={.ptrint=&i2c_addr_sec_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    if (i2c_failed){goto i2c_failed_jump;} //second address failed

    //i2c signature
    if (mcu_check_manufacturer() != 0){/*manuf_failed = true;*/ i2c_failed = true;} //invalid manufacturer
    if (i2c_failed){tmp_esc_col = term_esc_col_error; sprintf(buffer, "Wrong device signature, was expecting 0x%02X but got 0x%02X", i2c_dev_manuf, i2c_dev_sig);
    } else {tmp_esc_col = term_esc_col_normal; sprintf(buffer, "Device signature: 0x%02X, id:%d, version:%d", i2c_dev_sig, i2c_dev_id, i2c_dev_minor);}
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, tmp_col, tmp_esc_col, buffer);
    if (i2c_failed){goto i2c_failed_jump;}

    //config0
    if (mcu_update_config0() != 0){ //read/update of config0 register failed
        //config0_failed = true; i2c_failed = true;
        fprintf(stdout, "\e[%d;%dH\e[%dmWarning, failed to read 'config0' register\e[0m", tty_line++, tmp_col, term_esc_col_error);
    }
    tty_line++;

    //digital inputs
    fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tty_line++, tmp_col, term_esc_col_normal, "Input settings:");

    //pollrate
    int i2c_poll_rate_default = def_i2c_poll_rate;
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, term_esc_col_normal, "Pollrate (digital):_____ (in hz, set to 0 for unlimited)");
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+19, .y=tty_line++, .size=5}, .type=0, .value={.min=0, .max=9999, .ptrint=&i2c_poll_rate}, .defval={.ptrint=&i2c_poll_rate_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //adc pollrate
    int i2c_adc_poll_default = def_i2c_adc_poll;
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, term_esc_col_normal, "Pollrate (analog):____ (N for every N digital polls, 1 align to digital)");
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+18, .y=tty_line++, .size=4}, .type=0, .value={.min=1, .max=100, .ptrint=&i2c_adc_poll}, .defval={.ptrint=&i2c_adc_poll_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //irq
    int irq_gpio_default = def_irq_gpio;
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, term_esc_col_normal, "IRQ:___ (GPIO pin used for digital input interrupts, -1 to disable)");
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+4, .y=tty_line++, .size=3}, .type=0, .value={.min=-1, .max=45, .ptrint=&irq_gpio}, .defval={.ptrint=&irq_gpio_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //debounce
    int digital_debounce_default = def_digital_debounce;
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, tmp_col, term_esc_col_normal, "Debounce:__ (filter digital inputs to mitigate false contacts, 0 to disable)");
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+9, .y=tty_line++, .size=2}, .type=0, .value={.min=0, .max=7, .ptrint=&digital_debounce, .ptrbool=&i2c_update}, .defval={.ptrint=&digital_debounce_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    tty_line++;

    //analog
    //external adc
    int i2c_addr_adc_default[]={def_i2c_addr_adc0, def_i2c_addr_adc1, def_i2c_addr_adc2, def_i2c_addr_adc3};
    fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tty_line++, tmp_col, term_esc_col_normal, "External ADCs (set a invalid address to disable):");
    for (int i=0; i<4; i++){
        if (i2c_addr_adc[i] != i2c_addr_adc_back[i] || i2c_bus_update){
            i2c_addr_adc_back[i] = i2c_addr_adc[i];
            if (i2c_open_dev(&i2c_adc_fd[i], i2c_bus, i2c_addr_adc[i]) != 0){i2c_adc_err[i] = errno;}
        }
        if (i2c_adc_fd[i] < 0){tmp_esc_col = term_esc_col_error; sprintf(buffer, "(Invalid address, %s)", strerror(i2c_adc_err[i]));
        } else {tmp_esc_col = term_esc_col_normal; strcpy(buffer, "");}
        fprintf(stdout, "\e[%d;%dH\e[%dmADC%d address:_____ %s\e[0m", tty_line, tmp_col, tmp_esc_col, i, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+13, .y=tty_line++, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&i2c_addr_adc[i], .ptrbool=&i2c_update}, .defval={.ptrint=&i2c_addr_adc_default[i], .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    }
    tty_line++;

    //digital debug / adc configuration
    term_pos_button_t term_inputs_buttons[] = {
        {.str=" Digital Input Debug ", .ptrbool=&term_go_screen_digital, .ptrhint="Display digital inputs state"},
        {.str=" Analog Configuration ", .ptrbool=&term_go_screen_adc, .ptrhint="Enable/change ADCs limits, fuzz, flat values"},
    };

    int term_inputs_buttons_count = sizeof(term_inputs_buttons) / sizeof(term_inputs_buttons[0]);
    int term_inputs_buttons_pad = (tty_last_width - term_footer_buttons_width * term_inputs_buttons_count) / (term_inputs_buttons_count + 1);
    for (int i=0; i<term_inputs_buttons_count; i++){
        int x = term_inputs_buttons_pad + (term_footer_buttons_width + term_inputs_buttons_pad) * i;
        array_pad(term_inputs_buttons[i].str, strlen(term_inputs_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_line, .size=term_footer_buttons_width}, .type=1, .disabled=term_inputs_buttons[i].disabled, .value={.ptrchar=term_inputs_buttons[i].str, .ptrbool=term_inputs_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_inputs_buttons[i].ptrhint}};
    }

    i2c_failed_jump:; //jump point for i2c failure
    
    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //footer buttons
    bool reset_requested = false, default_requested = false, save_requested = false;
    term_pos_button_t term_footer_buttons[] = {
        //{.str="Discard", .ptrbool=&reset_requested, .ptrhint="Discard current modifications"},
        {.str="Default", .ptrbool=&default_requested, .ptrhint="Reset values to default (excl. Analog Configuration)"},
        {.str="Save", .ptrbool=&save_requested, .ptrhint="Save new configuration", .disabled=i2c_failed},
        {.str="Close", .ptrbool=&kill_resquested, .ptrhint="Close without saving"},
    };
    
    int term_footer_buttons_count = sizeof(term_footer_buttons) / sizeof(term_footer_buttons[0]);
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_buttons_count) / (term_footer_buttons_count + 1);
    for (int i=0; i<term_footer_buttons_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_buttons[i].str, strlen(term_footer_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_buttons[i].disabled, .value={.ptrchar=term_footer_buttons[i].str, .ptrbool=term_footer_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_buttons[i].ptrhint}};
    }
    
    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        //if (term_input.escape){term_go_mainscreen = true;} //escape key pressed, move cursor to last selectible element
        if (term_input.escape){select_index_current = select_limit-1;} //escape key pressed, move cursor to last selectible element
        if (term_go_screen_adc){term_screen_current = 1; goto funct_end;} //go to adc screen requested
        if (term_go_screen_digital){term_screen_current = 2; goto funct_end;} //go to digital screen requested
        if (save_requested){term_screen_current = 3; goto funct_end;} //go to save screen requested

        if (reset_requested){term_screen_update = true; goto funct_end;} //TODO
        if (default_requested){vars_main_default(); term_screen_update = true; goto funct_end;}

        
        if (i2c_update){term_screen_update = true; goto funct_end;} //value update

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);

}

void term_screen_adc(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_main = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Analog Configuration";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;




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







    term_pos_generic_t term_adc_raw[4] = {0}, term_adc_output[4] = {0};
    term_pos_string_t term_adc_string[4] = {0};


    int term_adc_pad = (tty_last_width - term_adc_width * 2) / 3; //padding between each ADC sections




bool adc_enabled_back[] = {adc_params[0].enabled, adc_params[1].enabled, adc_params[2].enabled, adc_params[3].enabled};

bool js_mcu_enabled[] = {mcu_conf_current.vals.en0==3, mcu_conf_current.vals.en1==3};
bool js_ext_enabled[] = {i2c_adc_fd[0]>-1 && i2c_adc_fd[1]>-1, i2c_adc_fd[2]>-1 && i2c_adc_fd[3]>-1};

bool js_enabled[] = {js_mcu_enabled[0] || js_ext_enabled[0], js_mcu_enabled[1] || js_ext_enabled[1]};
/*
bool js_used[] = {
    mcu_conf_current.vals.use0==3 || (i2c_adc_fd[0]>-1 && i2c_adc_fd[1]>-1),
    mcu_conf_current.vals.use1==3 || (i2c_adc_fd[2]>-1 && i2c_adc_fd[3]>-1),
};
*/
bool js_enabled_default[] = {def_js0_enable, def_js1_enable};
    bool adc_use_raw_min[4] = {0}, adc_use_raw_max[4] = {0};
bool uhid_js_swap_axis_default[] = {def_uhid_js0_swap_axis, def_uhid_js1_swap_axis}; //swap joystick XY axis

char* js_use_mcu_ext = "use MCU:_ EXT:_";


bool mcu_js_used_back[] = {mcu_js_used[0], mcu_js_used[1]}, external_js_used_back[] = {external_js_used[0], external_js_used[1]};

bool adc_setting_update = false;




    for(int x_loop=0, adc_loop=0; x_loop<2; x_loop++){
        
        int term_left = term_adc_pad + (term_adc_width + term_adc_pad) * x_loop, term_right = term_left + term_adc_width, tmp_line = tty_line, tmp_line_last = tty_line; //left/right border of current adc
        int x, x1, x2, w;
        bool js_used = mcu_js_used[x_loop] || external_js_used[x_loop]; //adc_reg_used[2*x_loop] && adc_reg_used[2*x_loop+1];
int term_esc_col = js_enabled[x_loop] ? term_esc_col_normal : term_esc_col_disabled;
        //enable joystick
        /*
        sprintf(buffer, "Joystick %d enabled:-", x_loop);
        x = 1 + term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line++, .size=1}, .type=2, .disabled=!js_used, .value={.ptrbool=&js_enabled[x_loop]}, .defval={.y=hint_def_line, .ptrbool=&js_enabled_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[3]}};
*/

        sprintf(buffer, "Joystick %d", x_loop);
        x = 1 + term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line++, x, term_esc_col, buffer);

//use mcu/ext
x = 1 + term_left + (term_adc_width - strlen(js_use_mcu_ext)) / 2; x1 = x + 8; x2 = x1 + 6;
fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, x, term_esc_col, js_use_mcu_ext);
term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=1}, .type=2, .disabled=!js_mcu_enabled[x_loop], .value={.ptrbool=&mcu_js_used[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[4]}};
term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=1}, .type=2, .disabled=!js_ext_enabled[x_loop], .value={.ptrbool=&external_js_used[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[4]}};
tmp_line++;

//check:
//mcu_js_used_back[] != mcu_js_used[]
//external_js_used_back[] != external_js_used[]





        for(int y_loop=0; y_loop<2; y_loop++){
            
            term_esc_col = term_esc_col_disabled;
            bool adc_used = adc_params[adc_loop].enabled;
            bool adc_enabled = mcu_js_used[x_loop] || external_js_used[x_loop];
            tmp_line++;

            //adc "title"
            if (adc_used){
                sprintf (buffer1, "%dbits:%s", adc_params[adc_loop].res, mcu_js_used[x_loop]?"MCU":"Ext");
                term_esc_col = term_esc_col_normal;
            } else if (adc_enabled){
                sprintf (buffer1, "available");
            } else {
                sprintf (buffer1, "disabled");
            }
            sprintf(buffer, "ADC%d(%s)(%s%s)", adc_loop, adc_params[adc_loop].name, buffer1, (adc_enabled && (!js_used || !(js_enabled[x_loop])))?",unused":""); strcpy(term_adc_string[adc_loop].str, buffer);
            x = term_left + array_pad(buffer, strlen(buffer), term_adc_width, '_', 0); w = strlen(buffer1);
            fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tmp_line, term_left, term_esc_col, buffer);
            if (adc_enabled){term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=w}, .type=1, .value={.ptrchar=term_adc_string[adc_loop].str, .ptrbool=&adc_params[adc_loop].enabled}, .hint={.y=hint_line, .str=term_hint_adc_str[3]}, .defval={.y=hint_def_line, .ptrbool=&js_enabled_default[x_loop]}};}
            tmp_line++;

            //limits
            x = term_right - 17; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmlimits\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_params[adc_loop].res_limit, .force_update=true, .ptrint=&adc_params[adc_loop].min}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].min}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_params[adc_loop].res_limit, .force_update=true, .ptrint=&adc_params[adc_loop].max}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].max}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
            }
            tmp_line++;//=2;

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
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=&adc_params[adc_loop].reversed}, .defval={.y=hint_def_line, .ptrbool=&adc_params_default[adc_loop].reversed}, .hint={.y=hint_line, .str=term_hint_adc_str[4]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_min[adc_loop], .ptrint=&adc_params[adc_loop].raw_min}, .hint={.y=hint_line, .str=term_hint_adc_str[5]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_max[adc_loop], .ptrint=&adc_params[adc_loop].raw_max}, .hint={.y=hint_line, .str=term_hint_adc_str[6]}};
            }

            tmp_line++;//=2;

            //flat, fuzz
            x = term_left + 8; x1 = x + 9; x2 = term_right - 5;
            fprintf(stdout, "\e[%d;%dH\e[%dmflat-in:--- -out:---\e[0m", tmp_line, x - 8, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmfuzz:-----\e[0m", tmp_line, x2 - 5, term_esc_col);
            if (adc_used){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=3}, .type=0, .value={.min=0, .max=35, .ptrint=&adc_params[adc_loop].flat_in, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].flat_in}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=3}, .type=0, .value={.min=0, .max=35, .ptrint=&adc_params[adc_loop].flat_out, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].flat_out}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=5}, .type=0, .value={.min=0, .max=adc_params[adc_loop].res_limit/2, .ptrint=&adc_params[adc_loop].fuzz}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].fuzz}, .hint={.y=hint_line, .str=term_hint_generic_str[1]}};
            }
        
            tmp_line += 2; tmp_line_last = tmp_line; adc_loop++;
        }

        //swap xy axis
        
        sprintf(buffer, "Swap %s/%s axis:-", adc_params[adc_loop-2].name, adc_params[adc_loop-1].name);
        x = term_left + (term_adc_width - strlen(buffer)) / 2;
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line_last, x, js_used?term_esc_col_normal:term_esc_col_disabled, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(buffer)-1, .y=tmp_line_last, .size=1}, .type=2, .disabled=!js_used, .value={.ptrbool=&uhid_js_swap_axis[x_loop]}, .defval={.y=hint_def_line, .ptrbool=&uhid_js_swap_axis_default[x_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[7]}};

    }


//bool uhid_js_swap = def_uhid_js_swap; //swap left and right joystick














    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Go to main menu"},
    };
    int term_footer_buttons_count = sizeof(term_footer_buttons) / sizeof(term_footer_buttons[0]);
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_buttons_count) / (term_footer_buttons_count + 1);
    for (int i=0; i<term_footer_buttons_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_buttons[i].str, strlen(term_footer_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_buttons[i].disabled, .value={.ptrchar=term_footer_buttons[i].str, .ptrbool=term_footer_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_buttons[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        i2c_poll_joystick(false);

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

//joystick enabled update
bool js_used_update = false;
for (int i=0; i<2; i++){if (mcu_js_used[i] != mcu_js_used_back[i] || external_js_used[i] != external_js_used_back[i]){js_used_update = true; break;}}
if (js_used_update){;}

//individual adc update
bool adc_enabled_update = false;
for (int i=0; i<4; i++){
    if (adc_params[i].enabled != adc_enabled_back[i]){
        adc_enabled_update = true; break;
    } 




}






if (adc_enabled_update){;}


, .ptrbool=&adc_settings_update

adc_use_raw_min[adc_loop],
adc_use_raw_max[adc_loop],






        if (term_go_screen_main || term_input.escape){term_screen_current = 0; goto funct_end;} //escape key pressed, go back to main menu

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

void term_screen_digital(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_main = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Digital input registers states";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Go to main menu"},
    };
    int term_footer_buttons_count = sizeof(term_footer_buttons) / sizeof(term_footer_buttons[0]);
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_buttons_count) / (term_footer_buttons_count + 1);
    for (int i=0; i<term_footer_buttons_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_buttons[i].str, strlen(term_footer_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_buttons[i].disabled, .value={.ptrchar=term_footer_buttons[i].str, .ptrbool=term_footer_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_buttons[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_go_screen_main || term_input.escape){term_screen_current = 0; goto funct_end;} //escape key pressed, go back to main menu

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

void term_screen_save(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_main = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Save current settings";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Go to main menu"},
    };
    int term_footer_buttons_count = sizeof(term_footer_buttons) / sizeof(term_footer_buttons[0]);
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_buttons_count) / (term_footer_buttons_count + 1);
    for (int i=0; i<term_footer_buttons_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_buttons[i].str, strlen(term_footer_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_buttons[i].disabled, .value={.ptrchar=term_footer_buttons[i].str, .ptrbool=term_footer_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_buttons[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_go_screen_main || term_input.escape){term_screen_current = 0; goto funct_end;} //escape key pressed, go back to main menu

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

/*
void term_screen_generic(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_main = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Go to main menu"},
    };
    int term_footer_buttons_count = sizeof(term_footer_buttons) / sizeof(term_footer_buttons[0]);
    int term_buttons_pad = (tty_last_width - term_footer_buttons_width * term_footer_buttons_count) / (term_footer_buttons_count + 1);
    for (int i=0; i<term_footer_buttons_count; i++){
        int x = term_buttons_pad + (term_footer_buttons_width + term_buttons_pad) * i;
        array_pad(term_footer_buttons[i].str, strlen(term_footer_buttons[i].str), term_footer_buttons_width, ' ', 0);
        term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tty_last_height-1, .size=term_footer_buttons_width}, .type=1, .disabled=term_footer_buttons[i].disabled, .value={.ptrchar=term_footer_buttons[i].str, .ptrbool=term_footer_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_footer_buttons[i].ptrhint}};
    }

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_limit:%d\e[0m", tty_last_width-17, select_limit);}
    assert(select_limit <= select_max); //failsafe

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height); //update selectible elements

        if (term_go_screen_main || term_input.escape){term_screen_current = 0; goto funct_end;} //escape key pressed, go back to main menu

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}
*/
