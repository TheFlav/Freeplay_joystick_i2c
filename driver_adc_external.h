 /*
* FreeplayTech UHID gamepad driver
* External ADCs header
*
* Notes:
* - adc_type_funct_init[], adc_type_funct_read[] and adc_type_name[] needs to have same amount of elements.
* - if the ADC you implement doesn't need init/read function(s), please use "adc_type_funct_null" in adc_type_funct_init[] and/or adc_type_funct_read[].
* - it is highly recommended to set ADC resolution in the ADC init function.
* - please refer to 'adc_mcp3x21.c' for "template" functions.
*/

#pragma once

#include "adc_mcp3x21.c" //MCP3021/MCP3221

//type
int adc_type_funct_null(int fd, adc_data_t* adc_store){return 0;} //generic function to use as placeholder in adc_type_funct_init/adc_type_funct_read
int (*adc_type_funct_init[])(int /*fd*/, adc_data_t* /*adc_store*/) = {MCP3021_init,}; //pointer to init functions, should return 0 on success, negative on failure
int (*adc_type_funct_read[])(int /*fd*/, adc_data_t* /*adc_store*/) = {MCP3021_read,}; //pointer to read functions, should return 0 on success, negative on failure
char* adc_type_name[] = {MCP3021_name,}; //pointer to adc names

//general
int adc_type_count = sizeof(adc_type_funct_init) / sizeof(adc_type_funct_init[0]); //computed during runtime
int adc_addr[] = {def_adc0_addr, def_adc1_addr, def_adc2_addr, def_adc3_addr}; //I2C address, set to 0xFF to disable
int adc_type[] = {def_adc0_type, def_adc1_type, def_adc2_type, def_adc3_type}; //ADC type
int adc_fd[] = {-1, -1, -1, -1}; //i2c device file descriptor
int adc_init_err[] = {-1, -1, -1, -1}; //0:ok, -1:adc not enabled ,-2:failed to init



