


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

//program related
const char programversion[] = "0.1a"; //program version
bool kill_resquested = false; //program kill requested, stop main loop in a smooth way
#define buffer_size 1024 //char array buffer size


//debug related
bool debug = true; //debug outputs

static void debug_print_binary_int_term(int line, int col, int val, int bits, char* var){ //print given var in binary format at given term position
	printf("\e[%d;%dH\e[1;100m%s : ", line, col, var); for(int i = bits-1; i > -1; i--){printf("%d", (val >> i) & 0b1);} printf("\e[0m");
}


//I2C/ADC related
struct adc_t {
	int raw, raw_prev, raw_min, raw_max; //store raw values for min-max report
	unsigned char res; unsigned int res_limit; //adc resolution/limit set during runtime
	int value, min, max; //current value, min/max limits
	int fuzz, flat; //fuzz, flat
	bool reversed, autocenter; //reverse reading, autocenter: check adc value once to set as offset
} adc_data[4] = {
	{-1,-1,123/*INT_MAX*/,723/*INT_MIN*/, 10,1023, 0x7FFF,0,0xFFFF, 24,51, false,false,},
	{-1,-1,145/*INT_MAX*/,745/*INT_MIN*/, 10,1023, 0x7FFF,0,0xFFFF, 24,51, false,false,},
	{-1,-1,167/*INT_MAX*/,767/*INT_MIN*/, 10,1023, 0x7FFF,0,0xFFFF, 24,51, false,false,},
	{-1,-1,189/*INT_MAX*/,789/*INT_MIN*/, 10,1023, 0x7FFF,0,0xFFFF, 24,51, false,false,},
};

typedef union {struct {uint8_t use0:1, use1:1, use2:1, use3:1, en0:1, en1:1, en2:1, en3:1;} vals; uint8_t bits;} mcu_adc_conf_t;


//time related
double program_start_time = 0.; //program start time

static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return -1.; //failed
}


//print related
#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr


//array manipulation related
void array_fill(char* arr, int size, char chr){for (int i=0;i<size;i++){arr[i]=chr;} arr[size]='\0';} //fill array with given character, works with '\0' for full reset, last char set to '\0'

int array_pad(char* arr, int arr_len, int size, char pad, int align){ //pad a array with 'pad', 'align': 0:center 1:left 2:right, 'size':final array size (can be +1 if align:0 but both L/R pad not equal), return padding length
    if (size < arr_len || pad == '\0'){return 0;} //no padding to do
    char arr_backup[arr_len+1]; strcpy(arr_backup, arr); //backup original array
    
    int pad_len = size - arr_len; //padding length
    if (align==0){if (pad_len % 2 != 0){pad_len++;} pad_len /= 2;} //align to center, do necessary to have equal char count on both side

    char pad_buffer[pad_len + 1]; array_fill(pad_buffer, pad_len, pad); //generate padding array

    if (align != 1){
        array_fill(arr, arr_len, '\0'); //fully reset original array
        strcpy(arr, pad_buffer); strcat(arr, arr_backup);
        if (align == 0){strcat(arr, pad_buffer);}
    } else {strcat(arr, pad_buffer);}
    return pad_len;
}


//terminal related
struct winsize ws; //terminal size
struct termios term_backup; //original terminal state backup
void restore_term(void){tcsetattr(STDIN_FILENO, TCSANOW, &term_backup);} //restore terminal to original state funct

const int term_adc_width = 30; //ADC section width
const int term_adc_vertspacing = 9; //vertical spacing between each horizontal ADC elements
const int term_esc_col_normal = 92; //terminal normal color escape code
const int term_esc_col_disabled = 91; //terminal disabled color escape code

const int term_buttons_width = 15;
#define term_buttons_count 3
const int term_buttons_hintoffset = 6;

struct term_adc_pos_t { //adc terminal data
    struct {int x, y, w; int* value;} //term col, row, length, pointer to value
    min, max,
    raw_min, raw_max,
    raw, output,
    flat, fuzz;
    struct {int x, y, w; char str[buffer_size];} //term col, row, length, string
    title;
    struct {int x, y, w; bool* var;} //term col, row, length, bool
    reversed;
    bool use_raw_min, use_raw_max; //raw min/max used as actual limits
};

struct term_button_pos_t {int x, y, w; char str[buffer_size];}; //buttons

#define term_selectible_count 255 //absolute limit to selectible elements
struct term_selectible_t { //selectible terminal elements data
    int *x, *y; //pointer to term col, row
    int type; //0:int, 1:bool, 2:bool(visual toogle)
    int *size; //pointer to num of char to set, needs to be defined for current index to be valid
    int min, max; //limits when ptrint set
    bool force_update; //update at each loop
    int *ptrint; //pointer to int var
    char *ptrchar; //pointer to char array var
    bool *ptrbool; //pointer to bool var
    char *ptrhint; //pointer to hint
};
int select_index_current = 0, select_index_last = 1; //current element selected, last selected





static void tty_signal_handler(int sig){ //signal handle func
	if (debug){print_stderr("DEBUG: signal received: %d\n", sig);}
	kill_resquested = true;
}

void term_select_add(struct term_selectible_t* store, int type, int* x, int* y, int* size, int min, int max, int update, int* ptrint, char* ptrchar, bool* ptrbool, char* ptrhint){ //add data to selectible elements struc
    if (!store){return;}
    store->type = type;
    store->force_update = update;
    if (x){store->x = x;}
    if (y){store->y = y;}
    if (size){store->size = size;}
    if (min >= 0){store->min = min;}
    if (max >= 0){store->max = max;}
    if (ptrint){store->ptrint = ptrint;}
    if (ptrchar){store->ptrchar = ptrchar;}
    if (ptrbool){store->ptrbool = ptrbool;}
    if (ptrhint){store->ptrhint = ptrhint;}
}


//main
static void program_usage (char* program){ //program usage, obviously
	fprintf(stdout, "Version: %s\n", programversion);
	fprintf(stdout, "Example : %s TODO\n", program);
	fprintf(stdout, "Need to run as root.\n"
	"Arguments:\n"
	"\t-h or -help: show arguments list.\n"
    //TODO something...
	);
}

int main (int argc, char** argv){
    program_start_time = get_time_double();
	if (getuid() != 0) {print_stderr("FATAL: this program needs to run as root, current user:%d\n", getuid()); program_usage(argv[0]); return EXIT_FAILURE;} //not running as root

	for(int i=1; i<argc; ++i){ //program arguments parse
		if (strcmp(argv[1],"-help") == 0 || strcmp(argv[1],"-h") == 0){program_usage(argv[0]); return 0;} //-h -help argument
		else if (strcmp(argv[i],"-whatever") == 0){/*something*/}
	}

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other, SIGKILL not work as program get killed before able to handle

    //disable STDIN print
    struct termios term_new;
    if (tcgetattr(STDIN_FILENO, &term_backup) != 0){print_stderr("failed to backup current terminal data\n"); return EXIT_FAILURE;}
    if (atexit(restore_term) != 0){print_stderr("failed to set atexit() to restore terminal data\n"); return EXIT_FAILURE;}
    if (tcgetattr(STDIN_FILENO, &term_new) != 0){print_stderr("failed to save current terminal data for updates\n"); return EXIT_FAILURE;}
    term_new.c_lflag &= ~(ECHO | ECHONL | ICANON); //disable input characters, new line character echo, disable canonical input to avoid needs of enter press for user unput submit
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_new) != 0){print_stderr("tcsetattr term_new failed\n"); return EXIT_FAILURE;}




    //TODO i2c part
    int ret = 0b10111010;
    mcu_adc_conf_t mcu_conf_current/*, mcu_conf_new*/;
    mcu_conf_current.bits = /*mcu_conf_new.bits = */(uint8_t)ret;
    bool adc_reg_enable[4]={false, false, false, false}, adc_reg_used[4]={false, false, false, false}, adc_reg_used_prev[4]={false, false, false, false};

    print_stdout("current MCU ADC configuration:\n");
    for (uint8_t i=0; i<4; i++){
        adc_reg_enable[i] = (bool)((mcu_conf_current.bits >> i+4) & 0b1);
        if (adc_reg_enable[i]){adc_reg_used[i] = adc_reg_used_prev[i] = (bool)((mcu_conf_current.bits >> i) & 0b1);}
        print_stdout("ADC%d: enabled:%d, used:%d\n", i, adc_reg_enable[i]?1:0, adc_reg_used[i]?1:0);
    }

    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK); //set stdin to non-blocking







    //struct term_selectible_t term_select[term_selectible_count];

    tty_start:; //landing point if tty is resized
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); int tty_last_width = ws.ws_col, tty_last_height = ws.ws_row, tty_line = 2; //tty size
    fprintf(stdout, "\e[?25l\e[2J"); //hide cursor, reset tty
    if (debug){fprintf(stdout, "\e[1;1H\e[100mtty:%dx%d\e[0m", tty_last_width, tty_last_height);} //print tty size, 640x480 is 80cols by 30rows

    struct term_adc_pos_t term_adc[4] = {0};
    struct term_selectible_t term_select[term_selectible_count] = {0};

    int select_index = 0;


    char buffer[buffer_size], buffer_esc[buffer_size], buffer1[buffer_size], buffer2[buffer_size];

    char* term_hint[]={
    "press [TAB] [UP] [DOWN] or (^) (v) to navigate",
    "press [LEFT] [RIGHT] or (<) (>) to change value",
    "press [ENTER] or (A) to enable or disable",
    "press [ENTER] or (A) to switch axis direction",
    "press [ENTER] or (A) to set as MIN limit value",
    "press [ENTER] or (A) to set as MAX limit value",
    "Save new configuration to /boot/config.txt",
    "Save new configuration to *program_path*/config.txt",
    "Discard any modifications",
    };








    int term_adc_pad = (tty_last_width - term_adc_width * 2) / 3; //padding between each ADC sections



    for(int draw_x_loop=0, adc_loop=0; draw_x_loop<2; draw_x_loop++){
        for(int draw_y_loop=0; draw_y_loop<2; draw_y_loop++){
            int tmp_line = tty_line + term_adc_vertspacing * draw_y_loop, /*tmp_col = 3 + draw_x_loop * 4, */term_esc_col = term_esc_col_disabled;
            struct term_adc_pos_t* adc_pos = &term_adc[adc_loop];
            bool adc_used = adc_reg_used[adc_loop];

            int term_left = 1 + term_adc_pad + (term_adc_width + term_adc_pad) * draw_x_loop, term_right = term_left + term_adc_width; //left/right border of current adc

            //adc "title"
            if (adc_used){
                sprintf (buffer1, "%dbits", adc_data[adc_loop].res); term_esc_col = term_esc_col_normal;
            } else if (adc_reg_enable[adc_loop]){
                

                sprintf (buffer1, "available");
            } else {
                sprintf (buffer1, "disabled");
            }
            sprintf(buffer, "ADC%d(%s)", adc_loop, buffer1); strcpy(adc_pos->title.str, buffer);
            adc_pos->title.x = term_left + array_pad(buffer, strlen(buffer), term_adc_width, '_', 0); adc_pos->title.y = tmp_line; adc_pos->title.w = strlen(adc_pos->title.str); //coordinates
            fprintf(stdout, "\e[%d;%dH\e[4;%dm%s\e[0m", tmp_line++, term_left, term_esc_col, buffer);
            





            if (adc_reg_enable[adc_loop]){term_select_add(&term_select[select_index++], 1, &adc_pos->title.x, &adc_pos->title.y, &adc_pos->title.w, 0, 0, false, NULL, adc_pos->title.str, &adc_reg_used[adc_loop], term_hint[2]);}

            //limits
            adc_pos->min.x = term_right - 17; adc_pos->max.x = term_right - 6; adc_pos->min.y = adc_pos->max.y = tmp_line; adc_pos->min.w = adc_pos->max.w = 6; //coordinates
            adc_pos->min.value = &adc_data[adc_loop].min; adc_pos->max.value = &adc_data[adc_loop].max; //val pointers
            fprintf(stdout, "\e[%d;%dH\e[%dmlimits\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, adc_pos->min.x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, adc_pos->max.x - 4, term_esc_col);
            if (adc_used){
                term_select_add(&term_select[select_index++], 0, &adc_pos->min.x, &adc_pos->min.y, &adc_pos->min.w, 0, adc_data[adc_loop].res_limit, true, adc_pos->min.value, NULL, NULL, term_hint[1]);
                term_select_add(&term_select[select_index++], 0, &adc_pos->max.x, &adc_pos->max.y, &adc_pos->max.w, 0, adc_data[adc_loop].res_limit, true, adc_pos->max.value, NULL, NULL, term_hint[1]);
            }
            tmp_line+=2;

            //raw/output
            adc_pos->raw.x = term_left + 4; adc_pos->output.x = term_right - 6; adc_pos->raw.y = adc_pos->output.y = tmp_line; adc_pos->raw.w = adc_pos->output.w = 6; //coordinates
            fprintf(stdout, "\e[%d;%dH\e[%dmraw:------\e[0m", tmp_line, adc_pos->raw.x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmoutput:------\e[0m", tmp_line++, adc_pos->output.x - 7, term_esc_col);

            //raw min max
            adc_pos->reversed.x = term_left + 7; adc_pos->raw_min.x = term_right - 17; adc_pos->raw_max.x = term_right - 6;
            adc_pos->reversed.y = adc_pos->raw_min.y = adc_pos->raw_max.y = tmp_line;
            adc_pos->reversed.w = 1; adc_pos->raw_min.w = adc_pos->raw_max.w = 6; //coordinates
            adc_pos->reversed.var = &adc_data[adc_loop].reversed; adc_pos->raw_min.value = &adc_data[adc_loop].raw_min; adc_pos->raw_max.value = &adc_data[adc_loop].raw_max; //val pointers

            //fprintf(stdout, "\e[%d;%dH\e[%dmraw\e[0m", tmp_line, term_left, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dminvert:-\e[0m", tmp_line, adc_pos->reversed.x - 7, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmin:------\e[0m", tmp_line, adc_pos->raw_min.x - 4, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmmax:------\e[0m", tmp_line, adc_pos->raw_max.x - 4, term_esc_col);

            if (adc_used){
                term_select_add(&term_select[select_index++], 2, &adc_pos->reversed.x, &adc_pos->reversed.y, &adc_pos->reversed.w, 0, 0, true, NULL, NULL, adc_pos->reversed.var, term_hint[3]);
                term_select_add(&term_select[select_index++], 1, &adc_pos->raw_min.x, &adc_pos->raw_min.y, &adc_pos->raw_min.w, 0, 0, true, adc_pos->raw_min.value, NULL, &adc_pos->use_raw_min, term_hint[4]);
                term_select_add(&term_select[select_index++], 1, &adc_pos->raw_max.x, &adc_pos->raw_max.y, &adc_pos->raw_max.w, 0, 0, true, adc_pos->raw_max.value, NULL, &adc_pos->use_raw_max, term_hint[5]);
            }
            tmp_line+=2;

            //flat/fuzz
            adc_pos->flat.x = term_left + 5; adc_pos->fuzz.x = term_right - 6; adc_pos->flat.y = adc_pos->fuzz.y = tmp_line; adc_pos->flat.w = adc_pos->fuzz.w = 6; //coordinates
            adc_pos->flat.value = &adc_data[adc_loop].flat; adc_pos->fuzz.value = &adc_data[adc_loop].fuzz; //val pointers
            fprintf(stdout, "\e[%d;%dH\e[%dmflat:------\e[0m", tmp_line, adc_pos->flat.x - 5, term_esc_col);
            fprintf(stdout, "\e[%d;%dH\e[%dmfuzz:------\e[0m", tmp_line, adc_pos->fuzz.x - 5, term_esc_col);
            if (adc_used){
                term_select_add(&term_select[select_index++], 0, &adc_pos->flat.x, &adc_pos->flat.y, &adc_pos->flat.w, 0, adc_data[adc_loop].res_limit, false, adc_pos->flat.value, NULL, NULL, term_hint[1]);
                term_select_add(&term_select[select_index++], 0, &adc_pos->fuzz.x, &adc_pos->fuzz.y, &adc_pos->fuzz.w, 0, adc_data[adc_loop].res_limit, false, adc_pos->fuzz.value, NULL, NULL, term_hint[1]);
            }

            adc_loop++;
        }
    }
    tty_line += term_adc_vertspacing * 2;

    //footer
    fprintf(stdout, "\e[%d;%dH\e[2K\e[1m%s\e[0m", tty_last_height-4, (tty_last_width - strlen(term_hint[0])) / 2, term_hint[0]); //nav hint

    //buttons
    int term_buttons_pad = (tty_last_width - term_buttons_width * term_buttons_count) / (term_buttons_count + 1);
    char* term_buttons_name[term_buttons_count] = {"Save to boot", "Save to file", "Close"};
    bool* term_buttons_bool[term_buttons_count] = {&kill_resquested, &kill_resquested, &kill_resquested};

    struct term_button_pos_t term_buttons[term_buttons_count] = {0};

    for (int i = 0; i < term_buttons_count; i++){
        term_buttons[i].x = term_buttons_pad + (term_buttons_width + term_buttons_pad) * i; term_buttons[i].y = tty_last_height - 1;
        strcpy(buffer, term_buttons_name[i]); term_buttons[i].w = array_pad(buffer, strlen(buffer), term_buttons_width, ' ', 0); strcpy(term_buttons[i].str, buffer);
        term_select_add(&term_select[select_index++], 1, &term_buttons[i].x, &term_buttons[i].y, &term_buttons[i].w, 0, 0, false, NULL, term_buttons[i].str, &kill_resquested, term_hint[term_buttons_hintoffset + i]);
    }



    select_index_last = -1; //force update all selectible element
    char last_key[100] = {'\0'}; //debug, last char used

    while (!kill_resquested){
        ioctl(STDIN_FILENO, TIOCGWINSZ, &ws); if(tty_last_width != ws.ws_col || tty_last_height != ws.ws_row){goto tty_start;} //"redraw" if tty size changed

        //TODO read i2c




        //DEBUG garbage data to adc_data, https://www.geeksforgeeks.org/generating-random-number-range-c/
        for(int i=0; i<4; i++){
            int rand_low = 0, rand_high = adc_data[i].res_limit;
            adc_data[i].raw = (rand() % (rand_high - rand_low + 1)) + rand_low;
            adc_data[i].raw_min = (rand() % (rand_high - rand_low + 1)) + rand_low;
            adc_data[i].raw_max = (rand() % (rand_high - rand_low + 1)) + rand_low;
            adc_data[i].value = (rand() % (100 - -100 + 1)) + -100;
        }

        //handle terminal user input
        char term_read_char;
        bool select_plus = false, select_minus = false, select_select = false;
        if (read(STDIN_FILENO, &term_read_char, 1) > 0){
            if (term_read_char == '\t'){if(debug){strcpy(last_key, "TAB");} select_index_current++;} //tab
            else if (term_read_char == '\n'){if(debug){strcpy(last_key, "ENTER");} select_select = true;} //enter
            else if (term_read_char == '\e'){ //escape
                if (read(STDIN_FILENO, &term_read_char, 1) > 0){
                    if (term_read_char == '[' && read(STDIN_FILENO, &term_read_char, 1) > 0){ //escape sequence
                        if (term_read_char == 'A'){if(debug){strcpy(last_key, "UP");} select_index_current--;} //up key
                        else if (term_read_char == 'B'){if(debug){strcpy(last_key, "DOWN");} select_index_current++;} //down key
                        else if (term_read_char == 'D'){if(debug){strcpy(last_key, "LEFT");} select_minus = true;} //left key
                        else if (term_read_char == 'C'){if(debug){strcpy(last_key, "RIGHT");} select_plus = true;} //right key
                    }
                } else {if(debug){strcpy(last_key, "ESC");} kill_resquested = true;} //esc key
            } else if (debug){sprintf(last_key, "'%c'(%d), no used", term_read_char, term_read_char);} //debug
            
            tcflush(STDIN_FILENO, TCIOFLUSH); //flush STDIN, useful?
            if (debug){fprintf(stdout, "\e[%d;%dH\e[2K\e[100mDEBUG last key: %s\e[0m\n", tty_last_height-5, 1, last_key);} //print last char to STDIN if debug
        }

        //handle i2c input
        //TODO

        //selectible elements
        bool select_update = false;
        if (select_index_current != select_index_last){ //selected index changed
            if(select_index_last < 0){select_index_last = select_index - 1;} else if(select_index_last > select_index - 1){select_index_last = 0;} //rollover
            if(select_index_current < 0){select_index_current = select_index - 1;} else if(select_index_current > select_index - 1){select_index_current = 0;} //rollover
            select_update = true;

            //bottom hint
            if (term_select[select_index_current].ptrhint){
                int tmpcol = (tty_last_width - strlen(term_select[select_index_current].ptrhint)) / 2;
                fprintf(stdout, "\e[%d;%dH\e[2K\e[1m%s\e[0m", tty_last_height-3, tmpcol, term_select[select_index_current].ptrhint);
            } else {fprintf(stdout, "\e[%d;0H\e[2K", tty_last_height-3);} //erase hint line
        }

        bool selected, valid;
        for (int i=0; i<select_index; i++){
            selected = (i==select_index_current);
            if (term_select[i].size){
                if (selected){
                    if ((term_select[i].type == 1 || term_select[i].type == 2) && select_select && term_select[i].ptrbool){ //set button pressed while selected element is bool
                        *term_select[i].ptrbool = !(*term_select[i].ptrbool); //toggle bool
                    } else if (term_select[i].type == 0 && (select_minus || select_plus) && term_select[i].ptrint){ //minus/plus button pressed while selected element is int
                        if (select_minus && *(term_select[i].ptrint) - 1 >= term_select[i].min){
                            *term_select[i].ptrint = *(term_select[i].ptrint) - 1;
                        } else if (select_plus && *(term_select[i].ptrint) + 1 <= term_select[i].max){
                            *term_select[i].ptrint = *(term_select[i].ptrint) + 1;
                        }
                        select_update = true; 
                    }
                }

                if (select_update || term_select[i].force_update){
                    int tmpcol_bg = 100, tmpcol_txt = 97, tmpcol_style = 0;
                    if (selected){tmpcol_bg = 47; tmpcol_txt = 30; tmpcol_style = 1;}
                    char* tmpptr = NULL; char buffer[*term_select[i].size + 1];
                    if (term_select[i].ptrchar){tmpptr = term_select[i].ptrchar; //char
                    } else if (term_select[i].ptrint){char fmtbuffer[10]; sprintf(fmtbuffer, "%%%dd", *term_select[i].size); sprintf(buffer, fmtbuffer, *term_select[i].ptrint); tmpptr = buffer; //int
                    } else if (term_select[i].type == 2/* && term_select[i].ptrbool*/){sprintf(buffer, "%s", *(term_select[i].ptrbool)?"X":" "); tmpptr = buffer;} //toogle bool
                    if (tmpptr){fprintf(stdout, "\e[%d;%dH\e[%d;%d;%d;4m%s\e[0m", *term_select[i].y, *term_select[i].x, tmpcol_style, tmpcol_bg, tmpcol_txt, tmpptr);}
                }
            }
        }

        if (select_update){select_index_last = select_index_current;}

        //non selectible elements or specific update
        for(int i=0; i<4; i++){ //adc loop
            if (adc_reg_used[i] != adc_reg_used_prev[i]){ //adc configuration update
                if (adc_reg_enable[i]){ //enabled
                    mcu_conf_current.bits ^= 1U << i; //toggle needed bit
                    //TODO i2c adc config update
                    adc_reg_used_prev[i] = adc_reg_used[i];
                    goto tty_start; //force full redraw
                } else {adc_reg_used[i] = adc_reg_used_prev[i] = false;} //adc not enabled
            }

            if (term_adc[i].use_raw_min){adc_data[i].min = adc_data[i].raw_min; term_adc[i].use_raw_min = false;} //set raw min as min limit
            if (term_adc[i].use_raw_max){adc_data[i].max = adc_data[i].raw_max; term_adc[i].use_raw_max = false;} //set raw max as max limit

            if (adc_reg_used[i]){
                //TODO compute output value

                fprintf(stdout, "\e[%d;%dH\e[1;4;92m%6d\e[0m", term_adc[i].raw.y, term_adc[i].raw.x, adc_data[i].raw); //raw
                fprintf(stdout, "\e[%d;%dH\e[1;4;92m%6d\e[0m", term_adc[i].output.y, term_adc[i].output.x, adc_data[i].value); //output
            }
        }


        fprintf(stdout, "\e[%d;0H\n", tty_last_height-1); //force tty update
        usleep (10000);
    }

    fprintf(stdout, "\e[%d;0H\e[?25h", tty_last_height+1); //set position to end, show cursor
    return 0;
}
