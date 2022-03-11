/*
* NNS configuration file handler
*/

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

extern bool debug;

#ifndef print_stderr
    #define print_stderr(fmt, ...) do {fprintf(stderr, "%s:%d: %s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

#ifndef print_stdout
    #define print_stdout(fmt, ...) do {fprintf(stdout, "%s:%d: %s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#endif

typedef struct cfg_vars_struct {
	const char* name; //IMPORTANT: no space
	const char* desc;
	const int type; //0:int, 1:uint, 2:float, 3:double, 4:bool, 5:int array (split by comma in cfg file), 6:hex8, 7:hex16, 8:hex32, 9:bin8, 10:bin16, 11:bin32
	const void* ptr;
} cfg_vars_t;

int config_sum (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/); //pseudo checksum for config build
int config_search_name (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/, char* /*value*/, bool /*skipNl*/); //search in cfg_vars struct, return index if found, -1 if not
int config_save (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/, char* /*filename*/, int /*uid*/, int /*gid*/, bool /*reset*/); //save config file
int config_set (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/, char* /*filename*/, int /*uid*/, int /*gid*/, bool /*readcfg*/, char* /*var_value*/); //update var in config file
bool config_type_parse (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/, int /*index*/, int /*type*/, char* /*var*/, char* /*value*/); //parse config var with specific type
void config_parse (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/, char* /*filename*/, int /*uid*/, int /*gid*/); //parse/create program config file
void config_list (cfg_vars_t* /*cfg*/, unsigned int /*cfg_size*/); //print all config vars
