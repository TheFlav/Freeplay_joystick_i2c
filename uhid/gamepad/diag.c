/*
* UHID driver diagnostic file
*/

#include "diag.h"
#include "driver_debug_print.h"


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












int term_init(){ //init terminal related vars
    term_esc_col_normal = 97; //normal color escape code
    term_esc_col_disabled = 90; //disabled color escape code
    term_esc_col_error = 91; //error color escape code
    term_esc_col_success = 92; //success color escape code

    term_footer_buttons_width = 15; //footer button width

    term_adc_width = 30; //ADC section width
    term_adc_vertspacing = 9; //vertical spacing between each horizontal ADC elements

    select_index_current = 0, select_index_last = -1; //current element selected, last selected

    term_screen_current = 0, term_screen_last = -1; //start "screen", last screen
    term_screen_update = false; //"screen" require update


    //disable STDIN print/tty setting backup
    struct termios term_new;
    if (tcgetattr(STDIN_FILENO, &term_backup) != 0){print_stderr("failed to backup current terminal data\n"); exit(EXIT_FAILURE);}
    if (tcgetattr(STDIN_FILENO, &term_new) != 0){print_stderr("failed to save current terminal data for updates\n"); exit(EXIT_FAILURE);}
    term_new.c_lflag &= ~(ECHO | ECHONL | ICANON); //disable input characters, new line character echo, disable canonical input to avoid needs of enter press for user input submit
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term_new) != 0){print_stderr("tcsetattr term_new failed\n"); exit(EXIT_FAILURE);}

    diag_mode_init = true;
    return 0;
}


int program_diag_mode(){
    term_init(); //init needed vars
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
/*
    //char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_adc = false, term_go_screen_digital = false, term_go_screen_save = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;


    //footer
    char* term_footer_str = "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate";
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_footer_str, 20)) / 2, term_footer_str); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="adc", .ptrbool=&term_go_screen_adc, .ptrhint="Hint adc"},
        {.str="digital", .ptrbool=&term_go_screen_digital, .ptrhint="Hint digital"},
        {.str="save", .ptrbool=&term_go_screen_save, .ptrhint="Hint save"},
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
        if (term_go_screen_save){term_screen_current = 3; goto funct_end;} //go to save screen requested

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
*/
}

void term_screen_adc(int tty_line, int tty_last_width, int tty_last_height){
/*
    //char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_adc = false, term_go_screen_digital = false, term_go_screen_save = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;


    //footer
    char* term_footer_str = "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate";
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_footer_str, 20)) / 2, term_footer_str); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="adc", .ptrbool=&term_go_screen_adc, .ptrhint="Hint adc"},
        {.str="digital", .ptrbool=&term_go_screen_digital, .ptrhint="Hint digital"},
        {.str="save", .ptrbool=&term_go_screen_save, .ptrhint="Hint save"},
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
        if (term_go_screen_save){term_screen_current = 3; goto funct_end;} //go to save screen requested

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
*/
}

void term_screen_digital(int tty_line, int tty_last_width, int tty_last_height){
/*
    //char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_adc = false, term_go_screen_digital = false, term_go_screen_save = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;


    //footer
    char* term_footer_str = "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate";
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_footer_str, 20)) / 2, term_footer_str); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="adc", .ptrbool=&term_go_screen_adc, .ptrhint="Hint adc"},
        {.str="digital", .ptrbool=&term_go_screen_digital, .ptrhint="Hint digital"},
        {.str="save", .ptrbool=&term_go_screen_save, .ptrhint="Hint save"},
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
        if (term_go_screen_save){term_screen_current = 3; goto funct_end;} //go to save screen requested

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
*/
}

void term_screen_save(int tty_line, int tty_last_width, int tty_last_height){
/*
    //char buffer[buffer_size], buffer1[buffer_size], buffer2[buffer_size];
    int hint_line = tty_last_height - 4, hint_def_line = hint_line - 1, tmp_col = 2, tmp_esc_col = term_esc_col_normal;
    bool term_go_screen_adc = false, term_go_screen_digital = false, term_go_screen_save = false;

    const int select_max = 255;
    term_select_t* term_select = NULL; term_select = (term_select_t*) malloc(select_max * sizeof(term_select_t)); assert(term_select != NULL);
    int select_limit = 0;

    char* screen_name = "Generic screen";
    fprintf(stdout, "\e[%d;%dH\e[%dm%s\e[0m", tty_line, (tty_last_width - strlen(screen_name))/2, term_esc_col_normal, screen_name);
    tty_line += 2;


    //footer
    char* term_footer_str = "Press \e[1m[TAB]\e[0m,\e[1m[UP]\e[0m,\e[1m[DOWN]\e[0m to navigate";
    fprintf(stdout, "\e[%d;%dH\e[2K%s", tty_last_height-3, (tty_last_width - strcpy_noescape(NULL, term_footer_str, 20)) / 2, term_footer_str); //nav hint

    //buttons
    term_pos_button_t term_footer_buttons[] = {
        {.str="adc", .ptrbool=&term_go_screen_adc, .ptrhint="Hint adc"},
        {.str="digital", .ptrbool=&term_go_screen_digital, .ptrhint="Hint digital"},
        {.str="save", .ptrbool=&term_go_screen_save, .ptrhint="Hint save"},
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
        if (term_go_screen_save){term_screen_current = 3; goto funct_end;} //go to save screen requested

        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (1000000/30); //limit to 60hz max to limit risk of i2c colision if driver is running
    }

    funct_end:; //jump point for fast exit
    free(term_select);
*/
}
















