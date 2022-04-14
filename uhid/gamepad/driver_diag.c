/*
* FreeplayTech UHID gamepad driver
* Diagnostic part header
*/

#include "driver_config.h"
#include "driver_debug_print.h"
#include "driver_i2c_registers.h"
#include "nns_config.h"
#include "driver_diag.h"

void vars_i2c_default(){ //reset all i2c config vars to default
    //debug = def_debug;
    //debug_adv = def_debug_adv;

    mcu_search = def_mcu_search;
    i2c_bus = def_i2c_bus;
    mcu_addr = def_mcu_addr;
    #ifdef ALLOW_MCU_SEC_I2C
        mcu_addr_sec = def_mcu_addr_sec;
    #endif

    #ifdef ALLOW_EXT_ADC
        adc_addr[0] = def_adc0_addr;
        adc_addr[1] = def_adc1_addr;
        adc_addr[2] = def_adc2_addr;
        adc_addr[3] = def_adc3_addr;
    #endif

    uhid_device_id = def_uhid_device_id;
}

void vars_digital_default(){ //reset all digital config vars to default
    irq_gpio = def_irq_gpio;
    digital_debounce = def_digital_debounce;
    i2c_poll_rate = def_i2c_poll_rate;
}

void vars_adc_default(int index, bool all){ //reset all adc config vars to default. "index" to -1 for full reset, "all" to false to only reset enabled,min,max,reverse
    if (index == -1){i2c_adc_poll = def_i2c_adc_poll;}

    if (index == -1 || index == 0){
        if (all){
            #ifdef ALLOW_EXT_ADC
                adc_type[0] = def_adc0_type;
            #endif
            adc_map[0] = def_adc0_map;
            adc_params[0].res = def_adc0_res;
            adc_params[0].fuzz = def_adc0_fuzz;
            adc_params[0].flat_in = adc_params[0].flat_out = def_adc0_flat;
            adc_params[0].autocenter = def_adc0_autocenter;
        }
        adc_params[0].enabled = def_adc0_enabled;
        adc_params[0].min = def_adc0_min;
        adc_params[0].max = def_adc0_max;
        adc_params[0].reversed = def_adc0_reversed;
    }

    if (index == -1 || index == 1){
        if (all){
            #ifdef ALLOW_EXT_ADC
                adc_type[1] = def_adc1_type;
            #endif
            adc_map[1] = def_adc1_map;
            adc_params[1].res = def_adc1_res;
            adc_params[1].fuzz = def_adc1_fuzz;
            adc_params[1].flat_in = adc_params[1].flat_out = def_adc1_flat;
            adc_params[1].autocenter = def_adc1_autocenter;
        }
        adc_params[1].enabled = def_adc1_enabled;
        adc_params[1].min = def_adc1_min;
        adc_params[1].max = def_adc1_max;
        adc_params[1].reversed = def_adc1_reversed;
    }

    if (index == -1 || index == 2){
        if (all){
            #ifdef ALLOW_EXT_ADC
                adc_type[2] = def_adc2_type;
            #endif
            adc_map[2] = def_adc2_map;
            adc_params[2].res = def_adc2_res;
            adc_params[2].fuzz = def_adc2_fuzz;
            adc_params[2].flat_in = adc_params[2].flat_out = def_adc2_flat;
            adc_params[2].autocenter = def_adc2_autocenter;
        }
        adc_params[2].enabled = def_adc2_enabled;
        adc_params[2].min = def_adc2_min;
        adc_params[2].max = def_adc2_max;
        adc_params[2].reversed = def_adc2_reversed;
    }

    if (index == -1 || index == 3){
        if (all){
            #ifdef ALLOW_EXT_ADC
                adc_type[3] = def_adc3_type;
            #endif
            adc_map[3] = def_adc3_map;
            adc_params[3].res = def_adc3_res;
            adc_params[3].fuzz = def_adc3_fuzz;
            adc_params[3].flat_in = adc_params[3].flat_out = def_adc3_flat;
            adc_params[3].autocenter = def_adc3_autocenter;
        }
        adc_params[3].enabled = def_adc3_enabled;
        adc_params[3].min = def_adc3_min;
        adc_params[3].max = def_adc3_max;
        adc_params[3].reversed = def_adc3_reversed;
    }
}

void vars_cfg_reload(void){ //reload config file
    config_parse(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid); //parse config file, create if needed

    //reset backup vars
    i2c_bus_back = -1, mcu_addr_back = -1;
    #ifdef ALLOW_MCU_SEC_I2C
        mcu_addr_sec_back = -1;
    #endif
    #ifdef ALLOW_EXT_ADC
        adc_addr[0] = def_adc0_addr; adc_addr[1] = def_adc1_addr; adc_addr[2] = def_adc2_addr; adc_addr[3] = def_adc3_addr;
        adc_addr_back[0] = adc_addr_back[1] = adc_addr_back[2] = adc_addr_back[3] = -1;
        close(adc_fd[0]); close(adc_fd[1]); close(adc_fd[2]); close(adc_fd[3]);
        adc_fd[0] = adc_fd[1] = adc_fd[2] = adc_fd[3] = -1;
    #endif
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
static void term_user_input(term_input_t* input, bool blocking, bool* wanted_input, bool* wanted_input_sec){ //process terminal key inputs and digital inputs, blocking to true to set blocking mode waiting any inputs if wanted_bool set to NULL and a specific one
    char term_read_char; char last_key[32] = {'\0'}; //debug, last char used
    term_input_t term_input_empty = {0}; //empty input for no terminal input check

    while (true){
        memset(input, 0, sizeof(term_input_t)); //reset input struct

        if (read(STDIN_FILENO, &term_read_char, 1) > 0){ //terminal input
            if (term_read_char == '\n'){input->enter = true;}
            else if (term_read_char == '\t'){input->tab = true;}
            else if (term_read_char == '-'){input->minus = true;}
            else if (term_read_char == '+'){input->plus = true;}
            else if (term_read_char == '\e'){ //escape
                if (read(STDIN_FILENO, &term_read_char, 1) > 0){
                    if (term_read_char == '[' && read(STDIN_FILENO, &term_read_char, 1) > 0){ //escape sequence
                        if (term_read_char == 'A'){input->up = true;} //up key
                        else if (term_read_char == 'B'){input->down = true;} //down key
                        else if (term_read_char == 'D'){input->left = true;} //left key
                        else if (term_read_char == 'C'){input->right = true;} //right key
                    }
                } else {input->escape = true;} //esc key
            } else if (debug){sprintf(last_key, "'%c'(%d), no used", term_read_char, term_read_char);} //debug
            ioctl(STDIN_FILENO, TCFLSH, 2); //flush stdin
        }

        if(io_fd_valid(mcu_fd) && memcmp(input, &term_input_empty, sizeof(term_input_t)) == 0){ //no terminal input, process mcu digital inputs
            double read_start = get_time_double(); //limit pollrate
            if ((read_start - term_read_mcu_start > diag_input_mcu_read_interval) && i2c_smbus_read_i2c_block_data(mcu_fd, 0, input_registers_count, (uint8_t *)&i2c_joystick_registers) >= 0){
                uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0; //merge to ease work
                bool* input_ptr[] = {&input->up, &input->down, &input->left, &input->right, &input->enter, &input->escape}; //ptr to term input struct
                for (int i=0; i<6; i++){if(term_read_mcu_inputs[i] >= 0){*input_ptr[i] = ~(inputs >> term_read_mcu_inputs[i]) & 0b1;}} //check mcu buttons

                //check for dpad right or left hold over 2secs
                if (input->left){ //dpad left
                    if (term_read_mcu_left_hold_start < 0){term_read_mcu_left_hold_start = read_start; //hold start
                    } else if (read_start - term_read_mcu_left_hold_start > 2.){input->left = false; input->minus = true;} //hold over 2sec, big decrement
                } else {term_read_mcu_left_hold_start = -1.;} //released

                if (input->right){ //dpad right
                    if (term_read_mcu_right_hold_start < 0){term_read_mcu_right_hold_start = read_start; //hold start
                    } else if (read_start - term_read_mcu_right_hold_start > 2.){input->right = false; input->plus = true;} //hold over 2sec, big increment
                } else {term_read_mcu_right_hold_start = -1.;} //released

                if (memcmp(input, &term_input_empty, sizeof(term_input_t)) != 0){term_read_mcu_start = read_start;} //some inputs
            }
        }
        
        if (!blocking){break; //non blocking
        } else if (memcmp(input, &term_input_empty, sizeof(term_input_t)) != 0){ //blocking
            if (wanted_input == NULL && wanted_input_sec == NULL){break; //any input pressed
            } else if (wanted_input != NULL && *wanted_input){break; //wanted input pressed
            } else if (wanted_input_sec != NULL && *wanted_input_sec){break;} //wanted input sec pressed
        }

        usleep (1000000/30); //limit speed
    }

    if (debug){
        if (input->enter){strcpy(last_key, "ENTER");} 
        if (input->tab){strcpy(last_key, "TAB");} 
        if (input->minus){strcpy(last_key, "MINUS");} 
        if (input->plus){strcpy(last_key, "PLUS");} 
        if (input->up){strcpy(last_key, "UP");} 
        if (input->down){strcpy(last_key, "DOWN");} 
        if (input->left){strcpy(last_key, "LEFT");} 
        if (input->right){strcpy(last_key, "RIGHT");} 
        if (input->escape){strcpy(last_key, "ESC");} 
        if (strlen(last_key)){fprintf(stdout, "\e[1;25H\e[0K\e[100mDEBUG last key: %s\e[0m\n", last_key);} //print last char to STDIN if debug
    }
}

static void term_select_update(term_select_t* store, int* index, int* index_last, int index_limit, term_input_t* input, int tty_width, int tty_height, bool update){ //update selectible elements
    //bool update = false;
    if (input->up){(*index)--;} else if (input->down || input->tab){(*index)++;}
    int limit = index_limit-1; int_rollover(index, 0, limit); 
    if (*index != *index_last){ //selected index changed
        int_rollover(index_last, 0, limit);
        for (int i=0; i < index_limit; i++){ //deal with shifting or disabled elements
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

    if (debug){fprintf(stdout, "\e[1;%dH\e[100mselect_index:%d\e[0m", tty_width-17, *index);}

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
    if (diag_first_run){term_screen_current = SCREEN_FIRSTRUN;}

    //get mcu input index
    for (int i=0; i<4; i++){term_read_mcu_inputs[i] = mcu_input_dpad_start_index + i;} //dpad
    term_read_mcu_inputs[4] = in_array_int16(mcu_input_map, BTN_A, input_registers_size); //a
    term_read_mcu_inputs[5] = in_array_int16(mcu_input_map, BTN_B, input_registers_size); //b

    //start term
    tty_start:; //landing point if tty is resized or "screen" changed or bool trigger
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); //set stdin to non-blocking
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); int tty_last_width = ws.ws_col, tty_last_height = ws.ws_row, tty_start_line = 2; //tty size
    fprintf(stdout, "\e[?25l\e[2J"); //hide cursor, reset tty

    int_constrain(&term_screen_current, 0, ((sizeof term_screen_funct_ptr)/sizeof (void*)) - 1); //avoid screen index overflow

    if (debug){fprintf(stdout, "\e[1;1H\e[100mtty:%dx%d, screen:%d\e[0m", tty_last_width, tty_last_height, term_screen_current);} //print tty size, 640x480 is 80cols by 30rows

    select_index_last = -1; //reset selectible
    if (term_screen_current != term_screen_last){select_index_current = 0; term_screen_last = term_screen_current;} //screen changed, reset select index

    term_screen_funct_ptr[term_screen_current](tty_start_line, tty_last_width, tty_last_height); //call "screen" function
    if (term_screen_current != term_screen_last || term_screen_update){term_screen_update = false; goto tty_start;} //reset screen request

    ioctl(STDOUT_FILENO, TCFLSH, 2); //flush stdout
    fprintf(stdout, "\e[0;0H\e[2J\e[?25h"); //reset tty, show cursor

    diag_mode_init = false;
    return 0;
}


void term_screen_main(int tty_line, int tty_last_width, int tty_last_height){
    int hint_line = tty_last_height - 4, tmp_col = 2;

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "UHID driver setup/diagnostic tool";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line++;

    //bool i2c_update = false, i2c_failed = false;
    bool term_go_screen_firstrun = false, term_go_screen_i2c = false, term_go_screen_adc = false, term_go_screen_digital = false, term_go_screen_debug = false;
    #ifdef ALLOW_MCU_SEC_I2C
        bool term_go_screen_advanced = false;
    #endif
    bool mcu_failed = !io_fd_valid(mcu_fd) || mcu_signature != mcu_manuf;

    //first run mode
    first_run_goto_adc_screen = true; //open adc screen after save/skip if not running in first run mode
    char *firstrun_btn_str = " Start ADC to Joystick detection ";
    char *firstrun_hint_str = "\e[91mConfiguration will be reloaded if SKIP chosen, unsaved data will be lost\e[0m";
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(firstrun_btn_str))/2, .y=tty_line++, .size=strlen(firstrun_btn_str)}, .type=1, .disabled=mcu_failed, .value={.ptrchar=firstrun_btn_str, .ptrbool=&term_go_screen_firstrun}, .hint={.y=hint_line, .str=firstrun_hint_str}};
    char *firstrun_hint1_str = "Allow easier detection of ADC to Joystick mapping";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(firstrun_hint1_str))/2, term_esc_col_normal, firstrun_hint1_str);

    tty_line+=2;

    //digital/adc configuration
    term_pos_button_t term_inputs_buttons[] = {
        {.str=" I2C/UHID Configuration ", .ptrbool=&term_go_screen_i2c, .ptrhint="Define I2C settings"},
        {.str=" Digital Configuration ", .ptrbool=&term_go_screen_digital, .ptrhint="Digital inputs specific settings, display registers state", .disabled=mcu_failed},
        {.str=" Analog Configuration ", .ptrbool=&term_go_screen_adc, .ptrhint="ADC inputs specific settings", .disabled=mcu_failed},
        #ifdef ALLOW_MCU_SEC_I2C
            {.str=" Advanced Configuration ", .ptrbool=&term_go_screen_advanced, .ptrhint="Edit MCU internal settings", .disabled=!io_fd_valid(mcu_fd_sec)},
        #endif
    };

    int term_inputs_buttons_count = sizeof(term_inputs_buttons) / sizeof(term_inputs_buttons[0]);
    for (int i=0; i<term_inputs_buttons_count; i++){
        int tmp_len = strlen(term_inputs_buttons[i].str);
        term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - tmp_len)/2, .y=tty_line, .size=tmp_len}, .type=1, .disabled=term_inputs_buttons[i].disabled, .value={.ptrchar=term_inputs_buttons[i].str, .ptrbool=term_inputs_buttons[i].ptrbool}, .hint={.y=hint_line, .str=term_inputs_buttons[i].ptrhint}};
        tty_line += 2;
    }

    if (mcu_failed){fprintf(stdout, "\e[%d;%dH\e[%dmFailed to open MCU main address.\e[0m", tty_line++, tmp_col, term_esc_col_error);}
    #ifdef ALLOW_MCU_SEC_I2C
        if (!io_fd_valid(mcu_fd_sec)){mcu_failed=true; fprintf(stdout, "\e[%d;%dH\e[%dmFailed to open MCU secondary address.\e[0m", tty_line++, tmp_col, term_esc_col_error);}
    #endif
    if (mcu_failed){fprintf(stdout, "\e[%d;%dH\e[%dmPlease check I2C Configuration for more information.\e[0m", tty_line++, tmp_col, term_esc_col_error);}
    tty_line++;

    //debug
    char *debug_btn_str = " Debugging ", *debug_hint_str = "Edit settings related to debugging";
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(debug_btn_str))/2, .y=tty_line, .size=strlen(debug_btn_str)}, .type=1, .value={.ptrchar=debug_btn_str, .ptrbool=&term_go_screen_debug}, .hint={.y=hint_line, .str=debug_hint_str}};
    
    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //footer buttons
    bool reset_requested = false, /*default_requested = false, */save_requested = false;
    term_pos_button_t term_footer_buttons[] = {
        {.str="Discard", .ptrbool=&reset_requested, .ptrhint="Reload configuration, unsaved data will be lost"},
        {.str="Save", .ptrbool=&save_requested, .ptrhint="Save new configuration", .disabled=!io_fd_valid(mcu_fd)},
        {.str="Close", .ptrbool=&kill_requested, .ptrhint="Close Setup/Diagnostic program without saving"},
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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

        if (term_input.escape){select_index_current = select_limit-1;} //escape key pressed, move cursor to last selectible element
        if (term_go_screen_firstrun){term_screen_current = SCREEN_FIRSTRUN; goto funct_end;} //go to first run screen requested
        if (term_go_screen_i2c){term_screen_current = SCREEN_I2C; goto funct_end;} //go to adc screen requested
        if (term_go_screen_digital){term_screen_current = SCREEN_DIGITAL; goto funct_end;} //go to digital screen requested
        if (term_go_screen_adc){term_screen_current = SCREEN_ADC; goto funct_end;} //go to adc screen requested
        #ifdef ALLOW_MCU_SEC_I2C
            if (term_go_screen_advanced){term_screen_current = SCREEN_ADVANCED; goto funct_end;} //go to advanced screen requested
        #endif
        if (term_go_screen_debug){term_screen_current = SCREEN_DEBUG; goto funct_end;} //go to adc screen requested
        if (reset_requested){ //reload config file
            vars_cfg_reload();
            i2c_open_dev(&mcu_fd, i2c_bus, mcu_addr); //reopen main address
            #ifdef ALLOW_MCU_SEC_I2C
                i2c_open_dev(&mcu_fd_sec, i2c_bus, mcu_addr_sec); //reopen secondary address
            #endif
            #ifdef ALLOW_EXT_ADC
                for (int i=0; i<4; i++){i2c_open_dev(&adc_fd[i], i2c_bus, adc_addr[i]);} //reopen external adc address
            #endif
            term_screen_update = true;
            goto funct_end;
        }

        if (save_requested){term_screen_current = SCREEN_SAVE; goto funct_end;} //go to save screen requested
        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


void term_screen_i2c(int tty_line, int tty_last_width, int tty_last_height){
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    char buffer[buffer_size];

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "I2C/UHID configuration";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line+=2;

    bool i2c_bus_update = false, i2c_update = false, display_i2c_map = false; i2c_failed = false;

    //uhid driver id
    int uhid_device_id_default = def_uhid_device_id;
    fprintf(stdout, "\e[%d;%dH\e[%dmID:___ (Added after driver name, mainly used if running multiple drivers)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+3, .y=tty_line++, .size=3}, .type=0, .value={.min=0, .max=99, .ptrint=&uhid_device_id}, .defval={.ptrint=&uhid_device_id_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    fprintf(stdout, "\e[%d;%dH\e[%dmWarning: You may have to redo emulators input binding.\e[0m", tty_line, tmp_col, term_esc_col_error);
    tty_line+=2;

    //warning
    fprintf(stdout, "\e[%d;%dH\e[%dmWarning:\e[0m", tty_line++, tmp_col, term_esc_col_error);
    fprintf(stdout, "\e[%d;%dH\e[%dm- Changing bus or main address can/will break digital input menu navigation.\e[0m", tty_line++, tmp_col, term_esc_col_error);
    fprintf(stdout, "\e[%d;%dH\e[%dm- Update value(s) ONLY if you know what you are doing.\e[0m", tty_line, tmp_col, term_esc_col_error);
    tty_line+=2;

    //mcu search
    bool mcu_search_def = def_mcu_search;
    fprintf(stdout, "\e[%d;%dH\e[%dmMCU search:_ (search MCU address if provided one fails, driver only)\e[0m", tty_line, tmp_col, tmp_esc_col);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+11, .y=tty_line, .size=1}, .type=2, .value={.ptrbool=&mcu_search}, .defval={.ptrbool=&mcu_search_def, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};
    tty_line+=2;

    //safelock
    bool i2c_safelock_back = i2c_safelock; //disable ability to change i2c address
    fprintf(stdout, "\e[%d;%dH\e[%dmSafelock:_ (Disable ability to change i2c settings)\e[0m", tty_line, tmp_col, tmp_esc_col);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+9, .y=tty_line++, .size=1}, .type=2, .value={.ptrbool=&i2c_safelock}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};
    //tty_line+=2;

    //bus
    int i2c_bus_default = def_i2c_bus;
    if (i2c_bus != i2c_bus_back){
        i2c_bus_back = i2c_bus; i2c_bus_update = true;
        i2c_bus_failed = (i2c_check_bus(i2c_bus) != 0);
        if (i2c_bus_failed){
            i2c_bus_err = errno;
            i2c_bus_failed = true;
            close(mcu_fd); mcu_fd=-1; //main
            #ifdef ALLOW_MCU_SEC_I2C
                close(mcu_fd_sec); mcu_fd_sec = -1; //secondary
            #endif
            #ifdef ALLOW_EXT_ADC
            for (int i=0; i<4; i++){close(adc_fd[i]); adc_fd[i] = -1; adc_fd_valid[i] = false;} //ext adc
            #endif
        }
    }

    if (i2c_bus_failed){
        tmp_esc_col = term_esc_col_error;
        sprintf(buffer, "Failed to open I2C bus, %s", strerror(i2c_bus_err));
    } else {strcpy(buffer, "Bus to use for I2C device(s)");}

    fprintf(stdout, "\e[%d;%dH\e[%dmBus:___ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+4, .y=tty_line++, .size=3}, .type=0, .disabled=i2c_safelock,.value={.min=0, .max=255, .ptrint=&i2c_bus, .ptrbool=&i2c_update}, .defval={.ptrint=&i2c_bus_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    if (i2c_bus_failed){goto i2c_failed_jump;} //bus failed

    //main address
    int mcu_addr_default = def_mcu_addr; tmp_esc_col = term_esc_col_normal;
    if (mcu_addr != mcu_addr_back || i2c_bus_update || !io_fd_valid(mcu_fd)){
        mcu_addr_back = mcu_addr;
        i2c_failed = (i2c_open_dev(&mcu_fd, i2c_bus, mcu_addr) != 0);
        if (i2c_failed){i2c_main_err = errno;}
    }

    if (!io_fd_valid(mcu_fd)){ //failed
        i2c_failed = true; tmp_esc_col = term_esc_col_error;
        sprintf(buffer, "Invalid address, %s", strerror(i2c_main_err));
    } else {strcpy(buffer, "Main I2C address, used for inputs");}

    fprintf(stdout, "\e[%d;%dH\e[%dmMain address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+13, .y=tty_line++, .size=5}, .type=3, .disabled=i2c_safelock, .value={.min=0, .max=127, .ptrint=&mcu_addr, .ptrbool=&i2c_update}, .defval={.ptrint=&mcu_addr_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //secondary address
    bool mcu_sec_failed = false; tmp_esc_col = term_esc_col_normal;
    #ifdef ALLOW_MCU_SEC_I2C
        int mcu_addr_sec_default = def_mcu_addr_sec;
        if (mcu_addr_sec != mcu_addr_sec_back || i2c_bus_update || !io_fd_valid(mcu_fd_sec)){
            mcu_addr_sec_back = mcu_addr_sec;
            if (i2c_open_dev(&mcu_fd_sec, i2c_bus, mcu_addr_sec) != 0){i2c_sec_err = errno;}
        }

        if (!io_fd_valid(mcu_fd_sec)){ //failed
            mcu_sec_failed = true;
            tmp_esc_col = term_esc_col_error;
            sprintf(buffer, "Invalid address, %s", strerror(i2c_sec_err));
        } else {strcpy(buffer, "Secondary I2C address, used for additionnal features");}
        
        fprintf(stdout, "\e[%d;%dH\e[%dmSecond address:_____ (%s)\e[0m", tty_line, tmp_col, tmp_esc_col, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+15, .y=tty_line++, .size=5}, .type=3, .disabled=i2c_safelock, .value={.min=0, .max=127, .ptrint=&mcu_addr_sec, .ptrbool=&i2c_update}, .defval={.ptrint=&mcu_addr_sec_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    #else
        fprintf(stdout, "\e[%d;%dH\e[%dmSecond address was disabled during compilation.\e[0m", tty_line++, tmp_col, term_esc_col_disabled);
    #endif

    if (!io_fd_valid(mcu_fd) || mcu_sec_failed){
        int tmp_addr_main = 0,  tmp_addr_sec = 0;
        if (mcu_search_i2c_addr(i2c_bus, &tmp_addr_main, &tmp_addr_sec) == 0){
            fprintf(stdout, "\e[%d;%dH\e[%dmPossible address: main:\e[4m0x%02X\e[24m\e[0m", tty_line++, tmp_col, term_esc_col_error, tmp_addr_main);
            #ifdef ALLOW_MCU_SEC_I2C
                fprintf(stdout, "\e[%dm, secondary:\e[4m0x%02X\e[24m\e[0m", term_esc_col_error, tmp_addr_sec);
            #endif
        }
    }
    //if (i2c_failed){goto i2c_failed_jump;} //address failed

    //i2c signature
    if (io_fd_valid(mcu_fd)){
        int check_manuf_ret = mcu_check_manufacturer();
        if (check_manuf_ret < 0){i2c_failed = true;} //invalid manufacturer
        if (i2c_failed){tmp_esc_col = term_esc_col_error; sprintf(buffer, "Wrong device signature, was expecting 0x%02X but got 0x%02X", mcu_manuf, mcu_signature);
        } else {tmp_esc_col = term_esc_col_normal; sprintf(buffer, "Device signature: 0x%02X, id:%d, version:%d", mcu_signature, mcu_id, mcu_version);}
        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line++, tmp_col, tmp_esc_col, buffer);
        if (i2c_failed){goto i2c_failed_jump;}
        if (check_manuf_ret > 0){fprintf(stdout, "\e[%d;%dH\e[%dmWARNING: program register version (%d) mismatch MCU version (%d)\e[0m", tty_line++, tmp_col, term_esc_col_error, mcu_version_even, mcu_version);}

        //config0
        if (mcu_update_config0() != 0){fprintf(stdout, "\e[%d;%dH\e[%dmWarning, failed to read 'config0' register\e[0m", tty_line++, tmp_col, term_esc_col_error);} //read/update of config0 register failed
    }
    tty_line++;

    //external adc
    #ifdef ALLOW_EXT_ADC
        int adc_addr_default[]={def_adc0_addr, def_adc1_addr, def_adc2_addr, def_adc3_addr};
        fprintf(stdout, "\e[%d;%dH\e[4;%dmExternal ADCs (set a invalid address to disable):\e[0m", tty_line++, tmp_col, term_esc_col_normal);
        for (int i=0; i<4; i++){
            if (adc_addr[i] != adc_addr_back[i] || i2c_bus_update || !io_fd_valid(adc_fd[i])){
                adc_addr_back[i] = adc_addr[i];
                adc_fd_valid[i] = (i2c_open_dev(&adc_fd[i], i2c_bus, adc_addr[i]) == 0);
                adc_err[i] = errno;
            }
            if (!adc_fd_valid[i]){tmp_esc_col = term_esc_col_error; sprintf(buffer, "(Invalid address, %s)", strerror(adc_err[i]));
            } else {tmp_esc_col = term_esc_col_normal; strcpy(buffer, "");}
            fprintf(stdout, "\e[%d;%dH\e[%dmADC%d address:_____ %s\e[0m", tty_line, tmp_col, tmp_esc_col, i, buffer);
            term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+13, .y=tty_line++, .size=5}, .type=3, .disabled=i2c_safelock, .value={.min=0, .max=127, .ptrint=&adc_addr[i], .ptrbool=&i2c_update}, .defval={.ptrint=&adc_addr_default[i], .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
        }
    #else
        fprintf(stdout, "\e[%d;%dH\e[%dmExternal ADCs feature was disabled during compilation.\e[0m", tty_line++, tmp_col, term_esc_col_disabled);
    #endif
    tty_line++;

    //i2c map
    //char* display_i2c_map_hint = "Display all devices on I2C bus";
    //term_select[select_limit++] = (term_select_t){.position={.x=tmp_col, .y=tty_line, .size=term_footer_buttons_width}, .type=1, .value={.ptrchar=" I2C map ", .ptrbool=&display_i2c_map}, .hint={.y=hint_line, .str=display_i2c_map_hint}};
    //tty_line+=2;

    i2c_failed_jump:; //jump point for i2c failure
    
    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //footer buttons
    bool term_go_screen_main = false, default_requested = false;
    term_pos_button_t term_footer_buttons[] = {
        {.str="I2C map", .ptrbool=&display_i2c_map, .ptrhint="Display all devices on I2C bus", .disabled=i2c_bus_failed},
        {.str="Default", .ptrbool=&default_requested, .ptrhint="Reset values to default"},
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Return to main menu"},
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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

        if (default_requested){vars_i2c_default(); term_screen_update = true; goto funct_end;} //reset vars to default
        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        if (i2c_safelock_back != i2c_safelock){i2c_update = true;}
        if (i2c_update){term_screen_update = true; goto funct_end;} //value update

        if (display_i2c_map){ //i2c detect "clone"
            fprintf(stdout, "\e[0;0H\e[2J\n"); tty_line = 2; //reset tty
            fprintf(stdout, "\e[%d;%dH\e[%d;4mI2C map:\e[0m\n", tty_line, tmp_col, term_esc_col_normal);
            tty_line += 2;

            int fd; //bus fd
            if (i2c_bus < 0){ //invalid bus
                fprintf(stdout, "\e[%d;%dH\e[%dmInvalid I2C bus:\e[1m%d\e[0m\n", tty_line, tmp_col, term_esc_col_error, i2c_bus);
                goto funct_i2cdetect_end;
            } else { //avoid jump into scope compiler error
                char fd_path[strlen(def_i2c_bus_path_format)+4]; sprintf(fd_path, def_i2c_bus_path_format, i2c_bus);
                fd = open(fd_path, O_RDWR);
                if (!io_fd_valid(fd)){ //invalid bus path
                    fprintf(stdout, "\e[%d;%dH\e[%dmFailed to open '\e[1m%s\e[21m', errno:%d (%m)\e[0m\n", tty_line, tmp_col, term_esc_col_error, fd_path, -fd);
                    goto funct_i2cdetect_end;
                } else {fprintf(stdout, "\e[%d;%dH\e[%dmBus: \e[1m%s\e[0m\n", tty_line, tmp_col, term_esc_col_normal, fd_path);}
            }
            tty_line += 2;

            char tmp_buffer[4096], tmp_buffer1[128];

            //header
            sprintf(tmp_buffer, "\e[%d;%dH\e[1m    ", tty_line++, tmp_col);
            for (int j=0; j<16; j++){sprintf(tmp_buffer1, " %x ", j); strcat(tmp_buffer, tmp_buffer1);}
            fputs(tmp_buffer, stdout); fputs("\e[0m\n", stdout);

            //grid
            double tmp_duration = get_time_double();
            for (int i=0; i<8; i++){
                sprintf(tmp_buffer, "\e[%d;%dH\e[1m%d0:\e[0m ", tty_line++, tmp_col, i);
                for (int j=0; j<16; j++){
                    int tmp = i*16 + j, tmp_esc_col = term_esc_col_disabled;
                    if (ioctl(fd, I2C_SLAVE_FORCE, tmp) >= 0 && i2c_smbus_read_byte_data(fd, 0) >= 0){tmp_esc_col = term_esc_col_success;}
                    sprintf(tmp_buffer1, "\e[%dm%02X\e[0m ", tmp_esc_col, tmp); strcat(tmp_buffer, tmp_buffer1);
                }
                fputs(tmp_buffer, stdout); fputc('\n', stdout);
            }
            tmp_duration = get_time_double() - tmp_duration;
            close(fd);

            tty_line++;
            fprintf(stdout, "\e[%d;%dH\e[%dmDuration: \e[1m%dms\e[0m\n", tty_line++, tmp_col, term_esc_col_normal, (int)(tmp_duration*1000.));
            funct_i2cdetect_end:; display_i2c_map = false;
            
            fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tty_last_height-2, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[3], 20)) / 2, term_hint_nav_str[3]); //press key to continu
            usleep(1000000/2); //wait half a sec
            term_user_input(&term_input, true, &term_input.enter, NULL); //wait for user input
            term_screen_update = true; goto funct_end; //force update screen
        }

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


void term_screen_adc(int tty_line, int tty_last_width, int tty_last_height){
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1;
    int term_adc_pad = (tty_last_width - term_adc_width * 2) / 3; //padding between each ADC column
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    bool adc_settings_update = true, adc_settings_update_reload = false;
    for (int i=0; i<4; i++){if (adc_params[i].enabled != adc_enabled_back[i]){adc_enabled_back[i] = adc_params[i].enabled;}}
    init_adc(); //"refresh" mcu adc data/config

    char* term_hint_adc_str[]={
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to toogle axis",
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to toogle axis direction",
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to set as MIN limit value",
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to set as MAX limit value",
        "\e[1m[LEFT]\e[0m,\e[1m[RIGHT]\e[0m to change ADC to Joystick map: none,X1,Y1,X2,Y2",
        "\e[1m[LEFT]\e[0m,\e[1m[RIGHT]\e[0m to change inside/outside deadzone value (percent)",
        "Detect ADC value at driver startup to help with drifting or non-linear sticks",
    };

    #ifdef ALLOW_EXT_ADC //adc type hint
        char term_hint_adc_type_str[buffer_size] = "Type: ";
        for (int i=0; i<adc_type_count; i++){
            sprintf(buffer, "%d:%s", i, adc_type_name[i]);
            if(i < adc_type_count-1){strcat(buffer, ", ");}
            strcat(term_hint_adc_type_str, buffer);
        }
    #endif

    char* screen_name = "Analog Configuration";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    term_pos_generic_t term_adc_raw[4] = {0}, term_adc_output[4] = {0};
    term_pos_string_t term_adc_string[4] = {0};
    bool adc_enabled_default[] = {def_adc0_enabled, def_adc1_enabled, def_adc2_enabled, def_adc3_enabled};
    int adc_map_default[] = {def_adc0_map, def_adc1_map, def_adc2_map, def_adc3_map};
    bool adc_use_raw_min[4] = {0}, adc_use_raw_max[4] = {0};

    //adc pollrate
    int i2c_adc_poll_default = def_i2c_adc_poll, i2c_adc_poll_last = i2c_adc_poll+1;
    int digits_count = 0;
    if (i2c_poll_rate == 0){sprintf(buffer1, "unlimited");} else {digits_count = int_digit_count(i2c_poll_rate); sprintf(buffer1, "%dhz", i2c_poll_rate);}
    sprintf(buffer, "Pollrate:____ (%s)", buffer1);
    int x = (tty_last_width - strlen(buffer))/2;
    term_pos_string_t term_pollrate_string = {.x=x+14, .y=tty_line ,.w=digits_count};
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, x, term_esc_col_normal, buffer);
    term_select[select_limit++] = (term_select_t){.position={.x=x+9, .y=tty_line, .size=4}, .type=0, .value={.min=1, .max=100, .ptrint=&i2c_adc_poll}, .defval={.ptrint=&i2c_adc_poll_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    tty_line+=2;

    //adcs
    for(int x_loop=0, adc_loop=0; x_loop<2; x_loop++){
        int term_left = term_adc_pad + (term_adc_width + term_adc_pad) * x_loop, term_right = term_left + term_adc_width, tmp_line = tty_line; //left/right border of current adc
        int x, x1, x2, w;

        for(int y_loop=0; y_loop<2; y_loop++){
            int term_esc_col = term_esc_col_disabled;
            bool adc_enabled = adc_params[adc_loop].enabled, adc_available = adc_fd_valid[adc_loop] || mcu_adc_enabled[adc_loop];

            //adc "title"
            sprintf(buffer2, "%s", adc_fd_valid[adc_loop] ? "Ext" : "MCU");
            if (adc_enabled){sprintf(buffer1, "%dbits:%s", adc_params[adc_loop].res, buffer2); term_esc_col = term_esc_col_normal;
            } else if (adc_available){sprintf(buffer1, "available:%s", buffer2);
            } else {sprintf(buffer1, "disabled");}
            
            sprintf(buffer, "ADC%d(%s)(%s)", adc_loop, js_axis_names[adc_map[adc_loop]+1], buffer1); strcpy(term_adc_string[adc_loop].str, buffer);
            x = term_left + array_pad(buffer, strlen(buffer), term_adc_width, '_', 0); w = strlen(buffer1);
            fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tmp_line, term_left, term_esc_col, buffer);
            if (adc_available){term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=w}, .type=1, .value={.ptrchar=term_adc_string[adc_loop].str, .ptrbool=&adc_params[adc_loop].enabled}, .hint={.y=hint_line, .str=term_hint_adc_str[0]}, .defval={.y=hint_def_line, .ptrbool=&adc_enabled_default[x_loop]}};}
            tmp_line++;

            //adc type identifier
            #ifdef ALLOW_EXT_ADC
                if (adc_fd_valid[adc_loop]){
                    int term_type_esc_col = adc_init_err[adc_loop] < -1 ? term_esc_col_error : term_esc_col_normal;
                    x = term_left + 5;
                    fprintf(stdout, "\e[%d;%dH\e[%dmtype:--- %s\e[0m", tmp_line, x - 5, term_type_esc_col, adc_init_err_str[-adc_init_err[adc_loop]]);
                    term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=3}, .type=0, .value={.min=0, .max=adc_type_count-1, .ptrint=&adc_type[adc_loop], .ptrbool=&adc_settings_update_reload}, .defval={.y=hint_def_line, .ptrint=&adc_type_default[adc_loop]}, .hint={.y=hint_line, .str=term_hint_adc_type_str}};
                    tmp_line++;
                }
            #endif

            //map, invert, output
            x = term_left + 8;
            term_adc_output[adc_loop].x = term_right - 4; term_adc_output[adc_loop].y = tmp_line; term_adc_output[adc_loop].w = 4;
            fprintf(stdout, "\e[%d;%dH\e[%dmmapping:---\e[0m", tmp_line, x - 8, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmoutput:----\e[0m", tmp_line, term_adc_output[adc_loop].x - 7, term_esc_col);
            if (adc_enabled){term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=3}, .type=0, .value={.min=-1, .max=3, .ptrint=&adc_map[adc_loop], .ptrbool=&adc_settings_update_reload}, .defval={.y=hint_def_line, .ptrint=&adc_map_default[adc_loop]}, .hint={.y=hint_line, .str=term_hint_adc_str[4]}};}
            tmp_line+=2;

            //limits
            x = term_right - 17; x1 = term_right - 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmlimits\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            if (adc_enabled){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_params[adc_loop].res_limit, .force_update=true, .ptrint=&adc_params[adc_loop].min, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].min}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=0, .value={.max=adc_params[adc_loop].res_limit, .force_update=true, .ptrint=&adc_params[adc_loop].max, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].max}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
            }
            tmp_line++;

            //raw min/max
            x = term_left + 7; x1 = term_right - 17; x2 = term_right - 6;
            term_adc_raw[adc_loop].x = term_left + 4; term_adc_raw[adc_loop].y = tmp_line; term_adc_raw[adc_loop].w = 6;
            fprintf(stdout, "\e[%d;%dH\e[%dmraw:------\e[0m", tmp_line, term_adc_raw[adc_loop].x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, x1 - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, x2 - 4, term_esc_col);
            if (adc_enabled){
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_min[adc_loop], .ptrint=&adc_params[adc_loop].raw_min}, .hint={.y=hint_line, .str=term_hint_adc_str[2]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=6}, .type=1, .value={.force_update=true, .ptrbool=&adc_use_raw_max[adc_loop], .ptrint=&adc_params[adc_loop].raw_max}, .hint={.y=hint_line, .str=term_hint_adc_str[3]}};
            }
            tmp_line++;//=2;

            //flat, fuzz
            x = term_left + 8; x1 = x + 9; x2 = term_right - 5;
            fprintf(stdout, "\e[%d;%dH\e[%dmflat-in:--- -out:---\e[0m", tmp_line, x - 8, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmfuzz:-----\e[0m", tmp_line, x2 - 5, term_esc_col);
            if (adc_enabled){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=3}, .type=0, .value={.min=0, .max=35, .ptrint=&adc_params[adc_loop].flat_in, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].flat_in}, .hint={.y=hint_line, .str=term_hint_adc_str[5]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=3}, .type=0, .value={.min=0, .max=35, .ptrint=&adc_params[adc_loop].flat_out, .ptrbool=&adc_settings_update}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].flat_out}, .hint={.y=hint_line, .str=term_hint_adc_str[5]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x2, .y=tmp_line, .size=5}, .type=0, .value={.min=0, .max=adc_params[adc_loop].res_limit/2, .ptrint=&adc_params[adc_loop].fuzz}, .defval={.y=hint_def_line, .ptrint=&adc_params_default[adc_loop].fuzz}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
            }
            tmp_line++;

            //autocenter, invert
            x = term_left + 11; x1 = term_right - 1;
            fprintf(stdout, "\e[%d;%dH\e[%dmautocenter:-\e[0m", tmp_line, x - 11, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dminvert:-\e[0m", tmp_line, x1 - 7, term_esc_col);
            if (adc_enabled){
                term_select[select_limit++] = (term_select_t){.position={.x=x, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=&adc_params[adc_loop].autocenter}, .defval={.y=hint_def_line, .ptrbool=&adc_params_default[adc_loop].autocenter}, .hint={.y=hint_line, .str=term_hint_adc_str[6]}};
                term_select[select_limit++] = (term_select_t){.position={.x=x1, .y=tmp_line, .size=1}, .type=2, .value={.ptrbool=&adc_params[adc_loop].reversed}, .defval={.y=hint_def_line, .ptrbool=&adc_params_default[adc_loop].reversed}, .hint={.y=hint_line, .str=term_hint_adc_str[1]}};
            }

            tmp_line+=3; adc_loop++;
        }
    }

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    bool reset_raw_limits_requested = true, default_requested = false, term_go_screen_main = false;
    term_pos_button_t term_footer_buttons[] = {
        {.str="Reset limits", .ptrbool=&reset_raw_limits_requested, .ptrhint="Reset min/max detected values"},
        {.str="Default", .ptrbool=&default_requested, .ptrhint="Reset Analog settings to default"},
        {.str="Back", .ptrbool=&term_go_screen_main, .ptrhint="Return to main menu"},
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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed
        
        i2c_poll_joystick(true);

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements
        
        if (term_pollrate_string.w != 0 && i2c_adc_poll != i2c_adc_poll_last){ //update displayed adc pollrate
            int tmp_pollrate = i2c_poll_rate / i2c_adc_poll;
            if (tmp_pollrate < 1){tmp_pollrate = 1;}
            sprintf(buffer, "\e[%d;%dH\e[1;%dm(%%%ddhz)\e[0m", term_pollrate_string.y, term_pollrate_string.x, term_esc_col_normal, term_pollrate_string.w);
            fprintf(stdout, buffer, tmp_pollrate); //pollrate in hz
            i2c_adc_poll_last = i2c_adc_poll;
        }

        for (int i=0; i<4; i++){
            if (adc_params[i].enabled != adc_enabled_back[i]){adc_settings_update_reload = true;} //adc enable changed
            if (adc_use_raw_min[i]){adc_params[i].min = adc_params[i].raw_min; adc_use_raw_min[i] = false; //set raw min as min limit
            } else if (adc_use_raw_max[i]){adc_params[i].max = adc_params[i].raw_max; adc_use_raw_max[i] = false; //set raw max as max limit
            } else if (reset_raw_limits_requested){adc_params[i].raw_min = adc_params[i].raw_max = adc_params[i].raw;} //reset raw detected limits

            if (adc_params[i].enabled){
                if (adc_settings_update){adc_data_compute(i);}

                //update raw display
                sprintf(buffer, "\e[%d;%dH\e[1;4;%dm%%%dd\e[0m", term_adc_raw[i].y, term_adc_raw[i].x, term_esc_col_normal, term_adc_raw[i].w);
                fprintf(stdout, buffer, adc_params[i].raw); //raw

                //update output display
                int adc_value_percent = (((double)adc_params[i].value / 0xFFFF) * 202) - 101; //adc position to -101+101
                int_constrain(&adc_value_percent, -100, 100); //bypass rounding problems
                sprintf(buffer, "\e[%d;%dH\e[%d;4;%dm%%%dd\e[0m", term_adc_output[i].y, term_adc_output[i].x, (adc_value_percent==-100||adc_value_percent==0||adc_value_percent==100)?7:1, term_esc_col_normal, term_adc_output[i].w);
                fprintf(stdout, buffer, adc_value_percent); //output
            }
        }
        if (reset_raw_limits_requested){reset_raw_limits_requested = false;} //reset raw detected limits

        if (adc_settings_update_reload){term_screen_update = true; goto funct_end;} //adc change require reload
        if (default_requested){vars_adc_default(-1, true); term_screen_update = true; goto funct_end;} //reset vars to default
        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        adc_settings_update = false;
        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


void term_screen_digital(int tty_line, int tty_last_width, int tty_last_height){
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2;

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    mcu_update_config0(); //read/update config0 register

    char* screen_name = "Digital Configuration";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    //pollrate
    int i2c_poll_rate_default = def_i2c_poll_rate;
    fprintf(stdout, "\e[%d;%dH\e[%dmPollrate (digital):_____ (in hz, set to 0 for unlimited)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+19, .y=tty_line++, .size=5}, .type=0, .value={.min=0, .max=9999, .ptrint=&i2c_poll_rate}, .defval={.ptrint=&i2c_poll_rate_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //irq
    int irq_gpio_default = def_irq_gpio;
    fprintf(stdout, "\e[%d;%dH\e[%dmIRQ:___ (GPIO pin used for digital input interrupts, -1 to disable)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+4, .y=tty_line++, .size=3}, .type=0, .value={.min=-1, .max=45, .ptrint=&irq_gpio}, .defval={.ptrint=&irq_gpio_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

    //debounce
    int digital_debounce_default = def_digital_debounce, digital_debounce_back = digital_debounce;
    fprintf(stdout, "\e[%d;%dH\e[%dmDebounce:__ (filter digital inputs to mitigate false contacts, 0 to disable)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+9, .y=tty_line, .size=2}, .type=0, .value={.min=0, .max=7, .ptrint=&digital_debounce}, .defval={.ptrint=&digital_debounce_default, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
    tty_line += 2;

    //input registers content
    char* input_registers_title = "Input registers state";
    fprintf(stdout, "\e[%d;%dH\e[%d;4m%s\e[0m", tty_line, (tty_last_width - strlen(input_registers_title))/2, term_esc_col_normal, input_registers_title);
    tty_line++;

    term_pos_string_t term_input_pos[input_registers_size] = {0};
    for (int line=0, input_index=0; line<input_registers_count; line++){
        fprintf(stdout, "\e[%d;%dH\e[1;4minput%d:\e[0m", tty_line++, tmp_col, line); array_fill(buffer, buffer_size, '\0'); //inputX:
        
        for (int i = 0; i < 8; i++, input_index++){
            sprintf(buffer1, " %d", i); //input bit

            int curr_input = mcu_input_map[input_index]; //current input report value
            char* curr_input_name = NULL; //input name pointer

            if ((input_index < mcu_input_dpad_start_index || input_index > mcu_input_dpad_start_index + 3)){
                if (curr_input >= BTN_GAMEPAD && curr_input < BTN_THUMBR + 1){curr_input_name = buttons_gamepad_names[abs(curr_input - BTN_GAMEPAD)];} //gamepad
                #ifdef uhid_buttons_misc_enabled //defined in driver_config.h
                else if (curr_input >= BTN_MISC && curr_input < BTN_9 + 1){curr_input_name = buttons_misc_names[abs(curr_input - BTN_MISC)];} //misc
                #endif
            } else {curr_input_name = buttons_dpad_names[abs(input_index - mcu_input_dpad_start_index)];} //dpad

            if (curr_input_name != NULL){ //valid name
                sprintf(buffer2, ":%s", curr_input_name);
                strcat(buffer1, buffer2);
                term_input_pos[input_index].w = 1;
            }

            strcat(buffer1, " "); strcpy(term_input_pos[input_index].str, buffer1);

            int tmpx = strcpy_noescape(NULL, buffer, 20); //current button x position
            if(tmpx + strcpy_noescape(NULL, buffer1, 20) + tmp_col + 1 > tty_last_width){tty_line+=2; tmpx = 0; array_fill(buffer, buffer_size, '\0');} //new line
            strcat(buffer, buffer1); strcat(buffer, "  ");

            term_input_pos[input_index].x = tmp_col + tmpx; term_input_pos[input_index].y = tty_line; //button position
            fprintf(stdout, "\e[%d;%dHX\e[4m%s\e[0m ", term_input_pos[input_index].y, term_input_pos[input_index].x, term_input_pos[input_index].str);
        }
        tty_line+=2;
    }

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    bool default_requested = false, term_go_screen_main = false;
    term_pos_button_t term_footer_buttons[] = {
        {.str="Default", .ptrbool=&default_requested, .ptrhint="Reset Digital settings to default"},
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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

        if (default_requested){vars_digital_default(); term_screen_update = true; goto funct_end;} //reset vars to default
        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        //update input registers state
        int ret = i2c_smbus_read_i2c_block_data(mcu_fd, 0, input_registers_count, (uint8_t *)&i2c_joystick_registers);
        if (ret >= 0){
            uint32_t inputs = (i2c_joystick_registers.input2 << 16) + (i2c_joystick_registers.input1 << 8) + i2c_joystick_registers.input0;
            for (int i=0; i<input_registers_size; i++){
                int tmpcol_bg = 100, tmpcol_txt = 97, tmpcol_style = 0, tmpcol_enable = 49;
                if (term_input_pos[i].w){tmpcol_enable = 42;} //valid button
                if (~(inputs >> i) & 0b1){tmpcol_bg = 47; tmpcol_txt = 30; tmpcol_style = 1 ;} //current input "high"
                fprintf(stdout, "\e[%d;%dH\e[%dm \e[0m\e[%d;%d;%dm%s\e[0m", term_input_pos[i].y, term_input_pos[i].x, tmpcol_enable, tmpcol_style, tmpcol_txt, tmpcol_bg, term_input_pos[i].str);
            }
        }

        //update debounce value
        if (digital_debounce != digital_debounce_back){
            mcu_update_config0();
            digital_debounce_back = digital_debounce;
        }

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}

void term_screen_save(int tty_line, int tty_last_width, int tty_last_height){
    int hint_line = tty_last_height - 4, tmp_col = 2;
    bool term_go_screen_main = false;

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    //char config_path_backup[strlen(config_path)+11]; sprintf(config_path_backup, "%s.bak.fpjs", config_path); //build config file backup fullpath
    bool save_requested = false;

    char* screen_name = "Save current settings";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;

    fprintf(stdout, "\e[%d;%dH\e[%d;4mNotes:\e[0m", tty_line++, tmp_col, term_esc_col_normal);
    fprintf(stdout, "\e[%d;%dH\e[%dm- Any user added comment in config file will be discarded.\e[0m", tty_line++, tmp_col, term_esc_col_normal);
    fprintf(stdout, "\e[%d;%dH\e[%dm- Configuration file backup will be created with .bak.fpjs extension\e[0m", tty_line, tmp_col, term_esc_col_normal);
    tty_line += 2;

    fprintf(stdout, "\e[%d;%dH\e[%d;4mCurrent configuration file:\e[0m", tty_line++, tmp_col, term_esc_col_normal);
    tty_line += term_print_path_multiline(config_path, tty_line, tmp_col, tty_last_width, term_esc_col_normal) + 2; //multiline path

    //save button
    term_pos_button_t term_save_button = {.str=" Save new configuration ", .ptrbool=&save_requested};
    term_select[select_limit++] = (term_select_t){.position={.x=(tty_last_width - strlen(term_save_button.str))/2, .y=tty_line, .size=strlen(term_save_button.str)}, .type=1, .value={.ptrchar=term_save_button.str, .ptrbool=term_save_button.ptrbool}};

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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        if (save_requested){
            term_splash_save(tty_last_width, tty_last_height); //display save splash
            term_screen_current = SCREEN_MAIN; goto funct_end; //force update screen
        }

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep(1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


void term_splash_save(int tty_last_width, int tty_last_height){ //save new configuration file
    int tty_line = 2, tmp_col = 2;
    char config_path_backup[strlen(config_path)+11]; sprintf(config_path_backup, "%s.bak.fpjs", config_path); //build config file backup fullpath

    fprintf(stdout, "\e[0;0H\e[2J\n"); //reset tty
    fprintf(stdout, "\e[%d;%dH\e[%d;4mSaving new configuration file:\e[0m\n", tty_line, tmp_col, term_esc_col_normal);
    tty_line += 2;

    bool backup_removed = true;
    struct stat file_stat = {0};
    if (stat(config_path_backup, &file_stat) == 0){ //delete existing backup
        if (remove(config_path_backup) != 0){
            fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to remove backup file.\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, tty_line+2, tmp_col, errno);
            tty_line+=2;
            backup_removed = false;
        } else {fprintf(stdout, "\e[%d;%dH\e[%dm> Backup file removed.\e[0m\n", tty_line++, tmp_col, term_esc_col_success);}
    }

    if (backup_removed && stat(config_path, &file_stat) == 0){ //backup
        if (rename(config_path, config_path_backup) != 0){
            fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to backup existing configuration file.\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, tty_line+2, tmp_col, errno);
            tty_line+=2;
        } else {fprintf(stdout, "\e[%d;%dH\e[%dm> Existing configuration file has been backed up.\e[0m\n", tty_line++, tmp_col, term_esc_col_success);}
    }

    if (config_save(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, false) != 0){
        fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to save new configuration file.\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, tty_line+1, tmp_col, errno);
        tty_line+=2;
    } else {fprintf(stdout, "\e[%d;%dH\e[%dm> Configuration file saved successfully.\e[0m\n", tty_line++, tmp_col, term_esc_col_success);}

    fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tty_last_height-2, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[3], 20)) / 2, term_hint_nav_str[3]); //press key to continu
    usleep(1000000/2); //wait half a sec
    term_user_input(&term_input, true, &term_input.enter, NULL); //wait for user input
}


#ifdef ALLOW_MCU_SEC_I2C
    void term_screen_advanced(int tty_line, int tty_last_width, int tty_last_height){
        char buffer[buffer_size];
        int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2;
        bool term_go_screen_main = false;

        const int select_max = 255;
        term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
        int select_limit = 0;

        char* screen_name = "Advanced settings";
        fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
        tty_line += 2;

        //failsafe
        if (!io_fd_valid(mcu_fd_sec)){
            fprintf(stdout, "\e[%d;%dH\e[%dmFailed to open MCU secondary address.\e[0m", tty_line++, tmp_col, term_esc_col_error);
            goto mcu_sec_failed_jump;
        }

        //write protection
        bool mcu_write_protect = true, mcu_write_protect_back = true; i2c_failed = true;
        int ret = i2c_smbus_read_byte_data(mcu_fd_sec, mcu_sec_register_write_protect); 
        if (ret < 0){sprintf(buffer, "failed to read 'write_protect' register");
        } else {
            mcu_write_protect = mcu_write_protect_back = (ret == mcu_write_protect_enable)?true:false; i2c_failed = false;
            sprintf(buffer, "need to be disabled to allow update");
        }
        fprintf(stdout, "\e[%d;%dH\e[%dmWrite protection:_ (%s)\e[0m", tty_line, tmp_col, i2c_failed ? term_esc_col_error : term_esc_col_normal, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+17, .y=tty_line, .size=1}, .type=2, .value={.ptrbool=&mcu_write_protect}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};
        tty_line+=2;

        //mcu addresses
        bool mcu_update = false, i2c_failed = true;
        fprintf(stdout, "\e[%d;%dH\e[%d;4mUpdate MCU address (internally):\e[0m", tty_line++, tmp_col, term_esc_col_normal);

        int mcu_addr_new = mcu_addr;
        fprintf(stdout, "\e[%d;%dH\e[%dmMain:_____ (supply digital/analog inputs)\e[0m", tty_line, tmp_col, term_esc_col_normal);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+5, .y=tty_line++, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&mcu_addr_new}, .defval={.ptrint=&mcu_addr, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

        int mcu_addr_sec_new = mcu_addr_sec;
        fprintf(stdout, "\e[%d;%dH\e[%dmSecondary:_____ (provide additional features incl. this page)\e[0m", tty_line, tmp_col, term_esc_col_normal);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+10, .y=tty_line++, .size=5}, .type=3, .value={.min=0, .max=127, .ptrint=&mcu_addr_sec_new}, .defval={.ptrint=&mcu_addr_sec, .y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};

        fprintf(stdout, "\e[%d;%dH\e[%dmWarning, if address update fails, MCU may need to be reflashed.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        fprintf(stdout, "\e[%d;%dH\e[%dmIn this case, navigation thru menus using digital input will be broken.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        fprintf(stdout, "\e[%d;%dH\e[%dmNo confirmation will be asked during update process.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        fprintf(stdout, "\e[%d;%dH\e[%dmAddresses need to be updated one at a time.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        fprintf(stdout, "\e[%d;%dH\e[%dmIf successful, current configuration will be saved.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        fprintf(stdout, "\e[%d;%dH\e[%dmNote: Default values will remain unchanged.\e[0m", tty_line++, tmp_col, term_esc_col_error);
        //tty_line++;
        int term_mcu_update_button_index = select_limit;
        term_pos_button_t term_mcu_update_button = {.str=" Update MCU address ", .ptrhint="\e[91mWell, you've been warned"};
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col, .y=tty_line, .size=strlen(term_mcu_update_button.str)}, .type=1, .disabled=true, .value={.ptrchar=term_mcu_update_button.str, .ptrbool=&mcu_update, .force_update=true}, .hint={.y=hint_line, .str=term_mcu_update_button.ptrhint}};
        tty_line+=2;

        //backlight
        i2c_failed = true;
        int mcu_backlight_back = mcu_backlight, term_mcu_backlight_index = select_limit;
        ret = i2c_smbus_read_byte_data(mcu_fd_sec, mcu_sec_register_backlight_max); //max backlight
        if (ret < 0){sprintf(buffer, "failed to read 'backlight_max' register");
        } else {
            mcu_backlight_steps = ret;
            ret = i2c_smbus_read_byte_data(mcu_fd_sec, mcu_sec_register_backlight); //current backlight
            if (ret < 0){sprintf(buffer, "failed to read 'config_backlight' register");
            } else {
                mcu_backlight_back = mcu_backlight = ret; i2c_failed = false;
                sprintf(buffer, "directly update 'config_backlight' register");
            }
        }
        fprintf(stdout, "\e[%d;%dH\e[%dmBacklight level:___ (%s)\e[0m", tty_line, tmp_col, i2c_failed ? term_esc_col_error : term_esc_col_normal, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+16, .y=tty_line, .size=3}, .type=0, .disabled=mcu_write_protect, .value={.min=0, .max=mcu_backlight_steps, .ptrint=&mcu_backlight, .force_update=true}, .defval={.y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
        tty_line += 2;

        //led output
        i2c_failed = true;
        int mcu_led_output = 0, mcu_led_output_back = 0, term_mcu_led_output_index = select_limit; 
        ret = i2c_smbus_read_byte_data(mcu_fd_sec, mcu_sec_register_status_led_control); 
        if (ret < 0){sprintf(buffer, "failed to read 'status_led_control' register");
        } else {
            mcu_led_output = mcu_led_output_back = ret; i2c_failed = false;
            sprintf(buffer, "directly update 'status_led_control' register");
        }
        fprintf(stdout, "\e[%d;%dH\e[%dmLed output:___ (%s)\e[0m", tty_line, tmp_col, i2c_failed ? term_esc_col_error : term_esc_col_normal, buffer);
        term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+11, .y=tty_line, .size=3}, .type=0, .disabled=mcu_write_protect, .value={.min=0, .max=255, .ptrint=&mcu_led_output, .force_update=true}, .defval={.y=hint_def_line}, .hint={.y=hint_line, .str=term_hint_nav_str[1]}};
        tty_line += 2;

        mcu_sec_failed_jump:;

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

        while (!kill_requested){
            ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

            term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
            term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

            if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

            if (io_fd_valid(mcu_fd_sec)){
                //mcu address update
                term_select[term_mcu_update_button_index].disabled = !(!mcu_write_protect && mcu_addr_new != mcu_addr_sec_new && (mcu_addr_new != mcu_addr || mcu_addr_sec_new != mcu_addr_sec));

                if (mcu_update){ //update mcu address requested
                    int update_register = -1, update_value; bool update_mcu_main = false;
                    if (mcu_addr_new != mcu_addr){
                        update_register = mcu_sec_register_joystick_i2c_addr;
                        update_value = mcu_addr_new; update_mcu_main = true;
                    } else if (mcu_addr_sec_new != mcu_addr_sec){
                        update_register = mcu_sec_register_secondary_i2c_addr;
                        update_value = mcu_addr_sec_new;
                    }

                    if (update_register != -1){ //something to update
                        fprintf(stdout, "\e[0;0H\e[2J\n"); tty_line = 2; //reset tty
                        fprintf(stdout, "\e[%d;%dH\e[%d;4mUpdating MCU address:\e[0m\n", tty_line, tmp_col, term_esc_col_normal);
                        tty_line += 2;

                        //disable write protection
                        if (mcu_write_protect){fprintf(stdout, "\e[%d;%dH\e[%dm> Write protection needs to be disabled\e[0m\n", tty_line++, tmp_col, term_esc_col_error);
                        } else {
                            fprintf(stdout, "\e[%d;%dH\e[%dm> Write protection disabled\e[0m\n", tty_line++, tmp_col, term_esc_col_success);
                            if (mcu_update_register(&mcu_fd_sec, update_register, update_value, false) < 0){
                                fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to update %s address to 0x%02X\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, update_mcu_main?"Main":"Secondary", update_value, tty_line+2, tmp_col, errno);
                                tty_line+=2;
                            } else {
                                fprintf(stdout, "\e[%d;%dH\e[%dm> Address updated. Reconnecting MCU, please wait...\e[0m\n", tty_line++, tmp_col, term_esc_col_success); usleep (1000000*2); //wait 2s

                                int tmp_addr = update_mcu_main ? update_value : mcu_addr; //reopen main address
                                if (i2c_open_dev(&mcu_fd, i2c_bus, tmp_addr) < 0){
                                    fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to open main address (0x%02X)\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, tmp_addr, tty_line+2, tmp_col, errno);
                                    tty_line+=2;
                                } else {
                                    fprintf(stdout, "\e[%d;%dH\e[%dm> Main address (0x%02X) opened\e[0m\n", tty_line++, tmp_col, term_esc_col_success, tmp_addr);
                                    mcu_addr = tmp_addr; sprintf(buffer, "%s=0x%02X", cfg_mcu_addr_name, mcu_addr);
                                    config_set(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, false, buffer); //update config file
                                }

                                tmp_addr = update_mcu_main ? mcu_addr_sec : update_value; //reopen second address
                                if (i2c_open_dev(&mcu_fd_sec, i2c_bus, tmp_addr) < 0){
                                    fprintf(stdout, "\e[%d;%dH\e[%dm> Failed to open secondary address (0x%02X)\e[%d;%dHerrno:%d(%m)\e[0m\n", tty_line+1, tmp_col, term_esc_col_error, tmp_addr, tty_line+2, tmp_col, errno);
                                    tty_line+=2;
                                } else {
                                    fprintf(stdout, "\e[%d;%dH\e[%dm> Secondary address (0x%02X) opened\e[0m\n", tty_line++, tmp_col, term_esc_col_success, tmp_addr);
                                    mcu_addr_sec = tmp_addr; sprintf(buffer, "%s=0x%02X", cfg_mcu_addr_sec_name, mcu_addr_sec);
                                    config_set(cfg_vars, cfg_vars_arr_size, config_path, user_uid, user_gid, false, buffer); //update config file
                                }
                            }
                        }
                        
                        fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tty_last_height-2, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[3], 20)) / 2, term_hint_nav_str[3]); //press key to continu
                        usleep (1000000/2); //wait half a sec
                        term_user_input(&term_input, true, &term_input.enter, NULL); //wait for user input
                        select_index_current = 0;
                        term_screen_update = true; goto funct_end; //force update screen
                    }

                    mcu_update = false;
                }

                //write protection
                if (mcu_write_protect != mcu_write_protect_back){
                    i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect ? mcu_write_protect_enable : mcu_write_protect_disable);
                    mcu_write_protect_back = mcu_write_protect;
                    term_select[term_mcu_backlight_index].disabled = mcu_write_protect;
                    term_select[term_mcu_led_output_index].disabled = mcu_write_protect;
                }

                //backlight
                if (mcu_backlight != mcu_backlight_back){
                    i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_backlight, mcu_backlight);
                    mcu_backlight_back = mcu_backlight;
                }

                //led output
                if (mcu_led_output != mcu_led_output_back){
                    i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_status_led_control, mcu_led_output);
                    mcu_led_output_back = mcu_led_output;
                }
            }

            fprintf(stdout, "\e[0;0H\n"); //force tty update
            usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
        }

        funct_end:; //jump point for fast exit
        free(term_select);
        if (!mcu_write_protect){i2c_smbus_write_byte_data(mcu_fd_sec, mcu_sec_register_write_protect, mcu_write_protect_enable);} //re-enable write protection
    }
#else
    void term_screen_advanced(int tty_line, int tty_last_width, int tty_last_height){term_screen_current = SCREEN_MAIN;}
#endif


void term_screen_firstrun(int tty_line, int tty_last_width, int tty_last_height){ //resizing this screen will fully reset it
    char buffer[buffer_size]; int hint_line = tty_last_height - 4;
    int term_col_width = 34, term_col_pad = (tty_last_width - term_col_width * 2) / 3; //width, padding between each column

    term_pos_generic_t term_axis_min[4] = {0}, term_axis_max[4] = {0}, term_axis_adc[4] = {0};
    bool axis_detection_started[4] = {0}, axis_detection_update[4] = {0}, axis_start_requested[4] = {0}, axis_reset_requested[4] = {0};
    //int axis_adc[4]={-1,-1,-1,-1};
    //int axis_min[4] = {0}, axis_max[4] = {0}, axis_offset[4] = {0}, axis_flat_in[4] = {0}, axis_flat_out[4] = {0}, axis_output[4] = {0}; bool axis_reversed[4] = {0}; //backup axis data

    int axis_buttons_index[4][3] = {0}; //selectible output,start,reset index for each axis

    char* term_hint_axis_str[]={
        "Has to follow physical axis direction, \e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to reverse axis direction",
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to start ADC detection, move only wanted axis to its limits",
        "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to reset axis, min/max/reverse settings will be reset",
    };

    char *term_axis_start_str = "  Start  ", *term_axis_stop_str = "  Stop   ", *term_axis_reset_str = "  Reset  ";
    const int term_axis_start_len = strlen(term_axis_start_str), term_axis_reset_len = strlen(term_axis_reset_str);

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    diag_first_run = true; init_adc(); //force first run mode, re-init adc data

    char* screen_name = "UHID driver setup/diagnostic tool";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line++, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);

    char* sub_name = "First run mode";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(sub_name))/2, term_esc_col_normal, sub_name);
    tty_line+=2;

    int tmp_line;
    for(int x_loop=0, axis_loop=0; x_loop<2; x_loop++){
        int term_left = term_col_pad + (term_col_width + term_col_pad) * x_loop, term_right = term_left + term_col_width - 1; //left/right border of column
        tmp_line = tty_line;

        sprintf(buffer, "%s Joystick", (x_loop==0)?"Left":"Right");
        fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tmp_line, term_left + (term_col_width - strlen(buffer)) / 2, term_esc_col_normal, buffer);
        tmp_line+=2;

        for(int y_loop=0; y_loop<2; y_loop++){
            sprintf(buffer, "%s Axis", (y_loop==0)?"Horizontal":"Vertical");
            array_pad(buffer, strlen(buffer), term_col_width-1, '_', 0);
            fprintf(stdout, "\e[%d;%dH\e[1;4;%dm%s\e[0m", tmp_line++, term_left, term_esc_col_normal, buffer);

            strcpy(buffer, "ADC not detected");
            array_pad(buffer, strlen(buffer), term_col_width-1, ' ', 0);
            fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, term_left, term_esc_col_normal, buffer);
            term_axis_adc[axis_loop].x = term_left; term_axis_adc[axis_loop].y = tmp_line; term_axis_adc[axis_loop].w = 4;
            tmp_line++;

            fprintf(stdout, "\e[%d;%dH\e[%dmMin:______ Max:______\e[0m", tmp_line, term_left, term_esc_col_normal);
            term_axis_min[axis_loop].x = term_left+4; term_axis_max[axis_loop].x = term_left+15;
            term_axis_min[axis_loop].y = term_axis_max[axis_loop].y = tmp_line;
            term_axis_min[axis_loop].w = term_axis_max[axis_loop].w = 6;

            axis_buttons_index[axis_loop][0] = select_limit;
            fprintf(stdout, "\e[%d;%dH\e[%dmOutput:____\e[0m", tmp_line, term_right-11, term_esc_col_normal);
            term_select[select_limit++] = (term_select_t){.position={.x=term_right-4, .y=tmp_line, .size=4}, .type=1, .value={.force_update=true, .ptrbool=&axis_reversed[axis_loop], .ptrint=&axis_output[axis_loop]}, .hint={.y=hint_line, .str=term_hint_axis_str[0]}};
            tmp_line+=2;

            axis_buttons_index[axis_loop][1] = select_limit;
            term_select[select_limit++] = (term_select_t){.position={.x=term_left, .y=tmp_line, .size=term_axis_start_len}, .type=1, .value={.ptrchar=term_axis_start_str, .ptrbool=&axis_start_requested[axis_loop]}, .hint={.y=hint_line, .str=term_hint_axis_str[1]}};
            axis_buttons_index[axis_loop][2] = select_limit;
            term_select[select_limit++] = (term_select_t){.position={.x=term_right-term_axis_reset_len, .y=tmp_line, .size=term_axis_reset_len}, .type=1, .value={.ptrchar=term_axis_reset_str, .ptrbool=&axis_reset_requested[axis_loop]}, .hint={.y=hint_line, .str=term_hint_axis_str[2]}};

            array_fill(buffer, term_col_width-term_axis_start_len-term_axis_reset_len-2, '_');
            fprintf(stdout, "\e[%d;%dH\e[2m%s\e[0m", tmp_line, term_left + term_axis_start_len + 1, buffer);

            //axis_adc[axis_loop] = axis_loop; adc_params[axis_loop].raw_min = adc_params[axis_loop].min = 163; adc_params[axis_loop].raw_max = adc_params[axis_loop].max = 760; adc_data_compute(axis_loop); //debug
            tmp_line+=3; axis_loop++;
        }
    }

    tty_line = tmp_line;

    char* term_axis_skip_close_str = "Open advanced ADC screen after saving/skipping (close program overwise):_";
    int x = (tty_last_width - strlen(term_axis_skip_close_str)) / 2;
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, x, term_esc_col_normal, term_axis_skip_close_str);
    term_select[select_limit++] = (term_select_t){.position={.x=x+strlen(term_axis_skip_close_str)-1, .y=tty_line, .size=1}, .type=2, .value={.ptrbool=&first_run_goto_adc_screen}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[0], 20)) / 2, term_hint_nav_str[0]); //nav hint

    //buttons
    bool save_requested = false, reset_requested = false, skip_requested = false;
    term_pos_button_t term_footer_buttons[] = {
        {.str="Save", .ptrbool=&save_requested, .ptrhint="Save new configuration"},
        {.str="Reset", .ptrbool=&reset_requested, .ptrhint="Reset all axis settings"},
        {.str="Skip", .ptrbool=&skip_requested, .ptrhint="Skip ADC detection"},
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

    bool term_force_update = true;

    //tty resize or screen update fix
    for (int i=0; i<4; i++){
        int adc_index = axis_adc[i];
        if (adc_index != -1){
            axis_detection_update[i] = true;
            adc_params[adc_index].min = axis_min[i]; adc_params[adc_index].max = axis_max[i];
            adc_params[adc_index].offset = axis_offset[i]; adc_params[adc_index].flat_in_comp = axis_flat_in[i]; adc_params[adc_index].flat_out_comp = axis_flat_out[i];
            adc_data_compute(adc_index);
        }
    }

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        if (axis_detection_started[0] || axis_detection_started[1] || axis_detection_started[2] || axis_detection_started[3]){
            for (int j=0; j<4; j++){ //adc loop
                adc_params[j].min = adc_params[j].raw_min; adc_params[j].max = adc_params[j].raw_max; //reset raw min/max
                adc_data_compute(j); //recompute adc data with raw limits as min/max values
            }
        }

        i2c_poll_joystick(true);

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, term_force_update); //update selectible elements
        term_force_update = false;

        for (int i=0; i<4; i++){ //axis loop
            if (axis_start_requested[i] != axis_detection_started[i]){ //start/stop axis detection request
                for (int j=0; j<select_limit; j++){term_select[j].disabled = axis_start_requested[i];} //disable all selectibles
                //for (int j=0; j<4; j++){term_select[axis_buttons_index[j][0]].disabled = (j==i) ? !axis_start_requested[i] : true;} //disable all output selectibles but current detection axis
                for (int j=0; j<3; j++){term_select[axis_buttons_index[i][j]].disabled = false;} //enable "output"/"start/stop"/"reset" related to current axis
                term_select[axis_buttons_index[i][1]].value.ptrchar = axis_start_requested[i]?term_axis_stop_str:term_axis_start_str; //change "start/stop" button text
                axis_detection_started[i] = axis_reset_requested[i] = axis_start_requested[i]; //start detection and reset axis
                //if (axis_detection_started[i]){select_index_current--;} //move select to output
                term_force_update = true; //update all selectibles on next loop
            }

            if (axis_reset_requested[i]){ //current axis reset requested
                for (int j=0; j<4; j++){adc_params[j].raw_min = adc_params[j].raw_max = adc_params[j].raw;} //reset all adcs
                axis_adc[i] = -1; axis_reversed[i] = false; axis_reset_requested[i] = false; //reset axis vars
                axis_detection_update[i] = true; //update term axis
            } else {
                if (axis_detection_started[i]){ //current axis in detection mode
                    int axis_adc_diff = 0, axis_adc_diff_index = -1; //adc biggest detected min-max difference, adc index
                    for (int j=0; j<4; j++){ //adc loop
                        int tmp_diff = abs(adc_params[j].raw_max - adc_params[j].raw_min); //compute adc min/max difference
                        if (tmp_diff > axis_adc_diff){axis_adc_diff = tmp_diff; axis_adc_diff_index = j;} //new biggest diff found
                    }
                    axis_adc[i] = axis_adc_diff_index; //detected axis
                    axis_detection_update[i] = true; //update term axis
                }
            }

            int adc_index = axis_adc[i];
            if (axis_detection_update[i]){ //update term axis requested
                if (adc_index != -1){sprintf(buffer, "ADC%d detected", adc_index);} else {strcpy(buffer, "ADC not detected");} //adc
                array_pad(buffer, strlen(buffer), term_col_width-1, ' ', 0);
                fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", term_axis_adc[i].y, term_axis_adc[i].x, term_esc_col_normal, buffer);

                if (adc_index != -1){
                    axis_min[i] = adc_params[adc_index].min;
                    sprintf(buffer, "%6d", axis_min[i]);
                } else {strcpy(buffer, "______");} //min
                fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", term_axis_min[i].y, term_axis_min[i].x, term_esc_col_normal, buffer);

                if (adc_index != -1){
                    axis_max[i] = adc_params[adc_index].max;
                    sprintf(buffer, "%6d", axis_max[i]);
                } else {strcpy(buffer, "______");} //max
                fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", term_axis_max[i].y, term_axis_max[i].x, term_esc_col_normal, buffer);

                if (adc_index != -1){
                    axis_offset[i] = adc_params[adc_index].offset;
                    axis_flat_in[i] = adc_params[adc_index].flat_in_comp;
                    axis_flat_out[i] = adc_params[adc_index].flat_out_comp;
                }

                axis_detection_update[i] = false;
            }

            if (adc_index != -1){ //update output
                axis_output[i] = adc_correct_offset_center(adc_params[adc_index].res_limit, adc_params[adc_index].raw, axis_min[i], axis_max[i], axis_offset[i], axis_flat_in[i], axis_flat_out[i]);
                axis_output[i] = (((double)axis_output[i] / adc_params[adc_index].res_limit) * 202) - 101; //adc position to -101+101
                if (axis_reversed[i]){axis_output[i] *= -1;}; //reverse axis
                int_constrain(&axis_output[i], -100, 100); //bypass rounding problems
            } else {axis_output[i] = 0;} //reset ouput
        }

        if (reset_requested){for (int i=0; i<4; i++){axis_reset_requested[i] = true;} reset_requested=false;} //reset all axis

        if (skip_requested || save_requested){ //"close/skip" pressed
            if (save_requested){
                bool adc_axis_used[4] = {0}; //adc used by axis

                for (int i=0; i<4; i++){ //axis loop
                    if (axis_adc[i] != -1){
                        int adc = axis_adc[i];
                        adc_map[adc] = i; //adc mapping
                        adc_params[adc].enabled = adc_axis_used[adc] = true;
                        adc_params[adc].min = axis_min[i]; adc_params[adc].max = axis_max[i]; //adc limits
                        adc_params[adc].reversed = axis_reversed[i]; //adc reversed
                    }
                }

                for (int i=0; i<4; i++){ //adc loop
                    if (!adc_axis_used[i]){ //reset unused adc to "default"
                        adc_map[i] = -1;
                        adc_params[i].enabled = false;
                        adc_params[i].min = 0; adc_params[i].max = adc_params[i].res_limit;
                        adc_params[i].reversed = false;
                    }
                }

                char buffer1[buffer_size];
                int tty_line = 2; fprintf(stdout, "\e[0;0H\e[2J\n"); //reset tty

                char* save_confirm_str = "Save confirmation";
                fprintf(stdout, "\e[%d;%dH\e[%d;4m%s\e[0m\n", tty_line, (tty_last_width - strlen(save_confirm_str)) / 2, term_esc_col_normal, save_confirm_str);
                tty_line += 2;

                for(int x_loop=0, axis_loop=0; x_loop<2; x_loop++){
                    int term_left = term_col_pad + (term_col_width + term_col_pad) * x_loop, term_right = term_left + term_col_width - 1; //left/right border of column
                    tmp_line = tty_line;

                    sprintf(buffer, "%s Joystick", (x_loop==0)?"Left":"Right");
                    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tmp_line, term_left + (term_col_width - strlen(buffer)) / 2, term_esc_col_normal, buffer);
                    tmp_line+=2;

                    for(int y_loop=0; y_loop<2; y_loop++){
                        int adc_index = axis_adc[axis_loop];

                        sprintf(buffer, "%s Axis", (y_loop==0)?"Horizontal":"Vertical");
                        array_pad(buffer, strlen(buffer), term_col_width-1, '_', 0);
                        fprintf(stdout, "\e[%d;%dH\e[1;4;%dm%s\e[0m", tmp_line++, term_left, term_esc_col_normal, buffer);

                        if (adc_index != -1){sprintf(buffer, "ADC%d detected", adc_index);} else {strcpy(buffer, "ADC not detected");} //adc
                        array_pad(buffer, strlen(buffer), term_col_width-1, ' ', 0);
                        fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tmp_line, term_left, term_esc_col_normal, buffer);
                        tmp_line++;

                        if (adc_index != -1){
                            sprintf(buffer, "%6d", axis_min[adc_index]); //min
                            sprintf(buffer1, "%6d", axis_max[adc_index]); //max
                        } else {
                            strcpy(buffer, "______"); //min
                            strcpy(buffer1, "______"); //max
                        }
                        fprintf(stdout, "\e[%d;%dH\e[%dmMin:\e[4m%s\e[24m Max:\e[4m%s\e[0m", tmp_line, term_left, term_esc_col_normal, buffer, buffer1);
                        fprintf(stdout, "\e[%d;%dH\e[%dmReversed:\e[4m%s\e[0m", tmp_line++, term_right-10, term_esc_col_normal, axis_reversed[adc_index]?"X":" ");
                        tmp_line+=2; axis_loop++;
                    }
                }

                if (!first_run_goto_adc_screen){strcpy(buffer, "Program will be closed once configuration saved");
                } else {strcpy(buffer, "Advanced ADC settings menu will be shown once configuration saved");}
                fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tmp_line, (tty_last_width - strlen(buffer)) / 2, buffer);

                fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[3], 20)) / 2, term_hint_nav_str[3]); //press key to continue
                fprintf(stdout, "\e[%d;%dH%s\e[0;0H\n", tty_last_height-2, (tty_last_width - strcpy_noescape(NULL, term_hint_nav_str[4], 20)) / 2, term_hint_nav_str[4]); //press key to go back
                usleep(1000000/2); //wait half a sec
                term_user_input(&term_input, true, &term_input.enter, &term_input.escape); //wait for user input

                if (term_input.escape){term_screen_update = true; goto funct_end; //go back
                } else {term_splash_save(tty_last_width, tty_last_height);} //display save splash
            } else if (skip_requested){vars_cfg_reload();} //reload config if skip
            if (first_run_goto_adc_screen){term_screen_current = SCREEN_ADC; goto funct_end;} else {kill_requested = true;} //go to adc screen or close
        }

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    diag_first_run = false; //disable first run mode
    free(term_select);
}


void term_screen_debug(int tty_line, int tty_last_width, int tty_last_height){ //generic screen function for future implement
    char buffer[buffer_size]; int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2;
    bool term_go_screen_main = false;

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Debug Configuration";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line+=2;

    //warning
    fprintf(stdout, "\e[%d;%dH\e[%dmWarning, configuration needs to be saved to retain defined values.\e[0m", tty_line, tmp_col, term_esc_col_error);
    tty_line+=2;

    //debug
    bool debug_back = debug, debug_def = def_debug;
    fprintf(stdout, "\e[%d;%dH\e[%dmDebug:_ (enable output in driver and setup/diagnostic program)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+6, .y=tty_line++, .size=1}, .type=2, .value={.ptrbool=&debug}, .defval={.y=hint_def_line, .ptrbool=&debug_def}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};

    //debug_adv
    bool debug_adv_def = def_debug_adv;
    fprintf(stdout, "\e[%d;%dH\e[%dmAdvanced Debug:_ (enable benchmark output in driver, does disable pollrate)\e[0m", tty_line, tmp_col, term_esc_col_normal);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+15, .y=tty_line, .size=1}, .type=2, .value={.ptrbool=&debug_adv}, .defval={.y=hint_def_line, .ptrbool=&debug_adv_def}, .hint={.y=hint_line, .str=term_hint_nav_str[2]}};
    tty_line+=2;

    //benchmark
    char* benchmark_hint_str = "\e[1m[ENTER]\e[0m/\e[1m(A)\e[0m to start benchmark";
    fprintf(stdout, "\e[%d;%dH\e[4;%dmBenchmark (sample over 2 seconds, all inputs will be blocked):\e[0m", tty_line++, tmp_col, term_esc_col_normal);

    bool benchmark_i2c = false;
    int benchmark_i2c_index = select_limit, benchmark_i2c_loops = 0, benchmark_i2c_read = input_registers_count + 6;
    fprintf(stdout, "\e[%d;%dH\e[%dmI2C:________ loops per sec (MCU input, %d write/read per poll)\e[0m", tty_line, tmp_col, term_esc_col_normal, benchmark_i2c_read);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+4, .y=tty_line++, .size=8}, .type=1, .disabled=!io_fd_valid(mcu_fd), .value={.ptrbool=&benchmark_i2c, .ptrint=&benchmark_i2c_loops}, .hint={.y=hint_line, .str=benchmark_hint_str}};

    bool benchmark_uhid = false;
    int benchmark_uhid_index = select_limit, benchmark_uhid_loops = 0;
    fprintf(stdout, "\e[%d;%dH\e[%dmUHID:________ loops per sec (push to %s, can crash system)\e[0m", tty_line, tmp_col, term_esc_col_normal, uhid_device_path);
    term_select[select_limit++] = (term_select_t){.position={.x=tmp_col+5, .y=tty_line++, .size=8}, .type=1, .value={.ptrbool=&benchmark_uhid, .ptrint=&benchmark_uhid_loops}, .hint={.y=hint_line, .str=benchmark_hint_str}};

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

    bool term_force_update = true;
    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, term_force_update); //update selectible elements
        term_force_update = false;

        if (debug_back != debug){term_screen_update = true; goto funct_end;} //"redraw" if debug var changed
        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        if (benchmark_i2c){ //start i2c benchmark only if mcu detected
            array_fill(buffer, term_select[benchmark_i2c_index].position.size+1, 'X');
            fprintf(stdout, "\e[%d;%dH\e[1;30;47m%s\e[0m\n", term_select[benchmark_i2c_index].position.y, term_select[benchmark_i2c_index].position.x, buffer); //mimic selected element

            bool mcu_adc_read_back[2] = {0}; memcpy(&mcu_adc_read_back, &mcu_adc_read, sizeof(bool)*2); //backup mcu adc read
            mcu_adc_read[0] = mcu_adc_read[1] = true; //allow to read all mcu adc

            bool adc_fd_valid_back[4] = {0}; memcpy(&adc_fd_valid_back, &adc_fd_valid, sizeof(bool)*4); //backup ext adc valid fd
            adc_fd_valid[0] = adc_fd_valid[1] = adc_fd_valid[2] = adc_fd_valid[3] = false; //disable external adcs

            benchmark_i2c_loops = 0; //reset loops count

            double benchmark_start = get_time_double();
            while (true){
                i2c_poll_joystick(true);
                if (poll_clock_start - benchmark_start > 2.){break;}
                benchmark_i2c_loops++;
            }
            benchmark_i2c_loops /= 2;

            memcpy(&mcu_adc_read, &mcu_adc_read_back, sizeof(bool)*2); //restore mcu adc read
            memcpy(&adc_fd_valid, &adc_fd_valid_back, sizeof(bool)*4); //restore external adcs
            benchmark_i2c = false; term_force_update = true;
        }

        if (benchmark_uhid){
            array_fill(buffer, term_select[benchmark_uhid_index].position.size+1, 'X');
            fprintf(stdout, "\e[%d;%dH\e[1;30;47m%s\e[0m\n", term_select[benchmark_uhid_index].position.y, term_select[benchmark_uhid_index].position.x, buffer); //mimic selected element
            int tmp_fd = open(uhid_device_path, O_WRONLY), tmp_loop=0;
            double benchmark_curr = get_time_double(); double benchmark_start = benchmark_curr;
            benchmark_uhid_loops = 0; //reset loops count
            while (true){
                uhid_send_event(tmp_fd);
                if (tmp_loop++ > 5000){tmp_loop = 0; benchmark_curr = get_time_double();} //update interval every 100 loops
                if (benchmark_curr - benchmark_start > 2.){break;}
                benchmark_uhid_loops++;
            }
            benchmark_uhid_loops = ((double)benchmark_uhid_loops * (2. / (benchmark_curr - benchmark_start))) / 2; //partially compensate for get_time_double() overshot
            close(tmp_fd);
            benchmark_uhid = false; term_force_update = true;
        }

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}


/*
void term_screen_generic(int tty_line, int tty_last_width, int tty_last_height){ //generic screen function for future implement
    char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_main = false;

    const int select_max = 255; //TODO proper count
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[1;%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line+=2;

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

    while (!kill_requested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){term_screen_update = true; goto funct_end;} //"redraw" if tty size changed

        term_user_input(&term_input, false, NULL, NULL); //handle terminal user input
        term_select_update(term_select, &select_index_current, &select_index_last, select_limit, &term_input, tty_last_width, tty_last_height, false); //update selectible elements

        if (term_go_screen_main || term_input.escape){term_screen_current = SCREEN_MAIN; goto funct_end;} //escape key pressed, go back to main menu

        fprintf(stdout, "\e[0;0H\n"); //force tty update
        usleep (1000000/30); //limit to 30hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
}
*/
