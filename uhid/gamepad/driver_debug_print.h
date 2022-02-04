/*
* UHID driver debug functions
*/

#pragma once

extern bool diag_mode_init;
extern double program_start_time;

#ifndef print_stderr
#define print_stderr(fmt, ...) do {if(!diag_mode_init)fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

#ifndef print_stdout
#define print_stdout(fmt, ...) do {if(!diag_mode_init)fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time , __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

