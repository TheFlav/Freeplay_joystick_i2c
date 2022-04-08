/*
* FreeplayTech UHID gamepad driver
* Debug output related
*/

#pragma once

#include <stdbool.h>

extern double program_start_time;
extern double get_time_double(void);
extern bool diag_mode_init, quiet_mode;

#ifndef print_stderr
    #define print_stderr(fmt, ...) do {if(!diag_mode_init && !quiet_mode) fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

#ifndef print_stdout
    #define print_stdout(fmt, ...) do {if(!diag_mode_init && !quiet_mode) fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

