/*
* FreeplayTech UHID gamepad driver
* MCP3021/MCP3221 External ADC implement
* Mainly here as a template for other external ADC implementation.
* Please refer to driver_adc_external.h for additional informations.
*/

#include <linux/swab.h> //byteswap integer

#define MCP3021_name "MCP3x21" //reported device name

static int MCP3021_init(int fd, adc_data_t* adc_store){ //init function
    //Do whatever needed to initialize the I2C IC
    adc_store->res = 12; //adc resolution (bits)
    return 0;
}

static int MCP3021_read(int fd, adc_data_t* adc_store){ //read function
    //Do whatever needed to read the I2C IC
    int ret = i2c_smbus_read_word_data(fd, 0);
    if (ret < 0){print_stderr("read failed, errno:%d (%m)\n", -ret); return ret;}
    adc_store->raw = (uint16_t)(__swab16(ret)); //swapped (specific to this model)
    return 0;
}

