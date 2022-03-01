//Freeplay_joystick_i2c_attiny


/*
 * 
 * TODO:  Maybe Add poweroff control via SECONDARY i2c address
 * TODO:  Maybe add an i2c register for the Pi to tell the attiny that the power is low (soon to shutdown).
 *        And then flash the backlight periodically.
 * MAYBE: Add watchdog shutdown if no "petting" after so long
 * 
 */

/* 
 *  You can comment in/out the USE_* #defines to compile in/out features
 */
#define USE_PWM_BACKLIGHT           //turning this off actually saves a fair bit of code space!

#define USE_SECONDARY_I2C_ADDR      //turn this off if you only want joystick support and don't want/need the secondary i2c address functionality

#define USE_INTERRUPTS              //can interrupt the host when input0 or input1 or input2 changes
#define USE_EEPROM

#define USE_ADC0     //or can be used as digital input on input2 if desired
#define USE_ADC1     //or can be used as digital input on input2 if desired
#define USE_ADC2     //or can be used as digital input on input2 if desired
#define USE_ADC3     //or can be used as digital input on input2 if desired
//Keep in mind that these just turn on/off the ABILITY to use the ADCs.  You still need to enable them in adc_conf_bits, otherwise they're just digital inputs

#define USE_MUX_ON_PC0_TO_PC3       //The mux that connects (DPAD u/d/l/r) or (ThumbL, ThumbR, TL2, TR2) to PC0,PC1,PC2,PC3
#define USE_MUX_ON_PC4_TO_PB7       //The mux that connects (Start, Select, A, B) or (X, Y, TL, TR) to PC4,PC5,PB6,PB7

#define USE_SERIAL_DEBUG      //UART is on PB2/PB3 which shares pins with IO2_2/IO2_3

#define USE_DIGITAL_BUTTON_DEBOUNCING


#include <Wire.h>

#ifdef USE_EEPROM
#include <EEPROM.h>
#endif

#define MANUF_ID         0xED
#define DEVICE_ID        0x00
#define VERSION_NUMBER   12


// Firmware for the ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 817


#define CONFIG_I2C_DEFAULT_ADDR     0x30
#ifdef USE_SECONDARY_I2C_ADDR
 #define CONFIG_I2C_2NDADDR          0x40  //0x30 wouldn't work
#endif



//#define ADC_RESOLUTION 10
//#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips
#define ADC_RESOLUTION ADC_NATIVE_RESOLUTION

#if ADC_RESOLUTION > 12
#error ADC Resolution problem
#endif

#define MAX_ADC ((1<<ADC_RESOLUTION)-1)


#ifdef USE_INTERRUPTS
 #define PIN_nINT 7                  //AKA PA3
 bool g_nINT_state = false;
#endif

#if defined(USE_MUX_ON_PC0_TO_PC3) || defined(USE_MUX_ON_PC4_TO_PB7)
#define PIN_MUX_SELECT 18
#endif

#define CONFIG_INVERT_POWER_BUTTON      //power button is on PIN_PB5

#define PIN_POWEROFF_OUT        19    //AKA PA2

#ifdef USE_PWM_BACKLIGHT
 #define PIN_BACKLIGHT_PWM       20     //AKA PA3

 #define NUM_BACKLIGHT_PWM_STEPS 11
 uint8_t backlight_pwm_steps[NUM_BACKLIGHT_PWM_STEPS] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xD0, 0xFF};
 #define CONFIG_BACKLIGHT_STEP_DEFAULT  4

 byte g_pwm_step = 0x00;  //100% on
#endif

#ifdef USE_ADC0
  #define FEATURES_ADC0         (1 << 0)
#else
  #define FEATURES_ADC0         (0 << 0)
#endif

#ifdef USE_ADC1
  #define FEATURES_ADC1         (1 << 1)
#else
  #define FEATURES_ADC1         (0 << 1)
#endif

#ifdef USE_ADC2
  #define FEATURES_ADC2         (1 << 2)
#else
  #define FEATURES_ADC2         (0 << 2)
#endif

#ifdef USE_ADC3
  #define FEATURES_ADC3         (1 << 3)
#else
  #define FEATURES_ADC3         (0 << 3)
#endif

#ifdef USE_INTERRUPTS
  #define FEATURES_IRQ         (1 << 4)
#else
  #define FEATURES_IRQ         (0 << 4)
#endif

#define FEATURES_AVAILABLE (FEATURES_ADC0 | FEATURES_ADC1 | FEATURES_ADC2 | FEATURES_ADC3 | FEATURES_IRQ)
#define ADCS_AVAILABLE ((FEATURES_ADC0 | FEATURES_ADC1 | FEATURES_ADC2 | FEATURES_ADC3) << 4)

struct joy_config0_bit_struct
{
    uint8_t debounce_on : 1;
    uint8_t unused1 : 1;
    uint8_t unused2 : 1;
    uint8_t unused3 : 1;
    uint8_t unused4 : 1;
    uint8_t unused5 : 1;
    uint8_t unused6 : 1;
    uint8_t unused7 : 1;
};

struct i2c_joystick_register_struct 
{
  uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
  uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
  uint8_t input2;          // Reg: 0x03 - INPUT port 2 (extended digital buttons)     BTN_THUMBL and BTN_THUMBR among other things
  uint8_t a0_msb;          // Reg: 0x04 - ADC0 most significant 8 bits
  uint8_t a1_msb;          // Reg: 0x05 - ADC1 most significant 8 bits
  uint8_t a1a0_lsb;        // Reg: 0x06 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
  uint8_t a2_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
  uint8_t a3_msb;          // Reg: 0x08 - ADC2 most significant 8 bits
  uint8_t a3a2_lsb;        // Reg: 0x09 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
#define REGISTER_ADC_CONF_BITS 0x09   //this one is writeable
  uint8_t adc_conf_bits;   // Reg: 0x09 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                           //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                           //             (but can only turn ON ADCs that are turned on in the high nibble.)
#define REGISTER_CONFIG_BITS 0x0A   //this one is writeable
  uint8_t config0;         // Reg: 0x0A - config register //maybe allow PA4-7 to be digital inputs connected to input2  config0[7]=use_extended_inputs
  uint8_t adc_res;         // Reg: 0x0B - current ADC resolution (maybe settable?)
  uint8_t rfu0;            // Reg: 0x0C - reserved for future use (or device-specific use)
  uint8_t manuf_ID;        // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
  uint8_t device_ID;       // Reg: 0x0E -
  uint8_t version_ID;      // Reg: 0x0F - 
  
} volatile i2c_joystick_registers;

struct i2c_secondary_address_register_struct 
{
#define REGISTER_SEC_CONFIG_BACKLIGHT   0x00    //this one is writeable
  uint8_t config_backlight;  // Reg: 0x00
  uint8_t backlight_max;     // Reg: 0x01 
#define REGISTER_SEC_POWER_CONTROL      0x02    //this one is writeable
  uint8_t power_control;     // Reg: 0x02 - host can tell us stuff about the state of the power (like low-batt or shutdown imminent) or even tell us to force a shutdown
  uint8_t features_available;// Reg: 0x03 - bit define if ADCs are available or interrups are in use, etc.
  uint8_t rfu0;              // Reg: 0x04 - reserved for future use (or device-specific use)
  uint8_t rfu1;              // Reg: 0x05 - reserved for future use (or device-specific use)
  uint8_t rfu2;              // Reg: 0x06 - reserved for future use (or device-specific use)
  uint8_t rfu3;              // Reg: 0x07 - reserved for future use (or device-specific use)
  uint8_t rfu4;              // Reg: 0x08 - reserved for future use (or device-specific use)
  uint8_t rfu5;              // Reg: 0x09 - reserved for future use (or device-specific use)
  uint8_t rfu6;              // Reg: 0x0A - reserved for future use (or device-specific use)
#define REGISTER_SEC_SEC_I2C_ADDR       0x0B   //this one is writeable
  uint8_t secondary_i2c_addr; // Reg: 0x0B - this holds the secondary i2c address (the address where this struct can be found)
#define REGISTER_SEC_JOY_I2C_ADDR       0x0C   //this one is writeable
  uint8_t joystick_i2c_addr; // Reg: 0x0C - this holds the primary (joystick's) i2c address
  uint8_t manuf_ID;          // Reg: 0x0D - manuf_ID:device_ID:version_ID needs to be a unique ID that defines a specific device and how it will use above registers
  uint8_t device_ID;         // Reg: 0x0E -
  uint8_t version_ID;        // Reg: 0x0F - 
} i2c_secondary_registers;

struct eeprom_data_struct
{
  uint8_t manuf_ID;               // EEPROM[0]
  uint8_t device_ID;              // EEPROM[1]
  uint8_t version_ID;             // EEPROM[2]
  uint8_t sec_config_backlight;   // EEPROM[3]
  uint8_t sec_joystick_i2c_addr;  // EEPROM[4]
  uint8_t joy_adc_conf_bits;      // EEPROM[5]
  uint8_t joy_config0;            // EEPROM[6]  
  uint8_t sec_secondary_i2c_addr; // EEPROM[7]
} eeprom_data;

volatile byte g_last_sent_input0 = 0xFF;
volatile byte g_last_sent_input1 = 0xFF;
volatile byte g_last_sent_input2 = 0xFF;

volatile byte g_i2c_index_to_read = 0;
volatile byte g_i2c_command_index = 0; //Gets set when user writes an address. We then serve the spot the user requested.
#ifdef CONFIG_I2C_2NDADDR
 volatile byte g_i2c_address = 0;  //if we're using multiple i2c addresses, we need to know which one is in use
#endif


/*
 * digital inputs
 * 
 * input0
 * 
 * PC0 = IO0_0 = DPAD_UP
 * PC1 = IO0_1 = DPAD_DOWN
 * PC2 = IO0_2 = DPAD_LEFT
 * PC3 = IO0_3 = DPAD_RIGHT
 * PC4 = IO0_4 = BTN_START
 * PC5 = IO0_5 = BTN_SELECT 
 * PB6 = IO0_6 = BTN_A
 * PB7 = IO0_7 = BTN_B
 * 
 * input1       
 * 
 * PB5 = IO1_0 = BTN_POWER (AKA BTN_MODE)
 * PC1 = IO1_1 = BTN_THUMBR
 * PC2 = IO1_2 = BTN_TL2
 * PC3 = IO1_3 = BTN_TR2
 * PC4 = IO1_4 = BTN_X              
 * PC5 = IO1_5 = BTN_Y              
 * PB6 = IO1_6 = BTN_TL
 * PB7 = IO1_7 = BTN_TR
 * 
 * input2       EXTENDED DIGITAL INPUT REGISTER 
 * 
 * PC0 = IO2_0 = BTN_THUMBL
 * ??? = IO2_1 = 
 * PB2 = IO2_2 = BTN_C (when no Serial debugging)
 * PB3 = IO2_3 = BTN_Z (when no Serial debugging)
 * PA4 = IO2_4 = BTN_0 (when ADC0 not used)
 * PA5 = IO2_5 = BTN_1 (when ADC1 not used)
 * PA6 = IO2_6 = BTN_2 (when ADC2 not used)
 * PA7 = IO2_7 = BTN_3 (when ADC3 not used)
 * 
 * 
 * 
 * PA2 =         POWEROFF_OUT
 * PB4 =         nINT OUT
 * PA3 =         PWM Backlight OUT
 * 
 */


#define INPUT0_DPAD_UP    (1 << 0)      //IO0_0
#define INPUT0_DPAD_DOWN  (1 << 1)      //IO0_1
#define INPUT0_DPAD_LEFT  (1 << 2)      //IO0_2
#define INPUT0_DPAD_RIGHT (1 << 3)      //IO0_3
#define INPUT0_BTN_START  (1 << 4)      //IO0_4
#define INPUT0_BTN_SELECT (1 << 5)      //IO0_5
#define INPUT0_BTN_A      (1 << 6)      //IO0_6
#define INPUT0_BTN_B      (1 << 7)      //IO0_7

#define INPUT1_BTN_POWER  (1 << 0)      //IO1_0
#define INPUT1_BTN_THUMBR (1 << 1)      //IO1_1
#define INPUT1_BTN_TL2    (1 << 2)      //IO1_2
#define INPUT1_BTN_TR2    (1 << 3)      //IO1_3
#define INPUT1_BTN_X      (1 << 4)      //IO1_4
#define INPUT1_BTN_Y      (1 << 5)      //IO1_5
#define INPUT1_BTN_TL     (1 << 6)      //IO1_6
#define INPUT1_BTN_TR     (1 << 7)      //IO1_7


#define INPUT2_BTN_THUMBL (1 << 0)      //IO2_0
                                        //IO2_1
                                        //IO2_2
                                        //IO2_3
#define INPUT2_BTN_0      (1 << 4)      //IO2_4
#define INPUT2_BTN_1      (1 << 5)      //IO2_5
#define INPUT2_BTN_2      (1 << 6)      //IO2_6
#define INPUT0_BTN_3      (1 << 7)      //IO2_7

#define PINB_POWER_BUTTON (1 << 5)    //PB5 is the power button, but it needs to be inverted (high when pressed)
                                        

//PRESSED means that the bit is 0
#define IS_PRESSED_DPAD_UP() ((i2c_joystick_registers.input0 & INPUT0_DPAD_UP) != INPUT0_DPAD_UP)
#define IS_PRESSED_DPAD_DOWN() ((i2c_joystick_registers.input0 & INPUT0_DPAD_DOWN) != INPUT0_DPAD_DOWN)

#define IS_PRESSED_BTN_A() ((i2c_joystick_registers.input0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define IS_PRESSED_BTN_B() ((i2c_joystick_registers.input0 & INPUT0_BTN_B) != INPUT0_BTN_B)

#define IS_PRESSED_BTN_TL() ((i2c_joystick_registers.input1 & INPUT1_BTN_TL) != INPUT1_BTN_TL)
#define IS_PRESSED_BTN_TR() ((i2c_joystick_registers.input1 & INPUT1_BTN_TR) != INPUT1_BTN_TR)

#define IS_PRESSED_BTN_POWER() ((i2c_joystick_registers.input1 & INPUT1_BTN_POWER) != INPUT1_BTN_POWER)

#define IS_SPECIAL_INPUT_MODE() (IS_PRESSED_BTN_A() && IS_PRESSED_BTN_B() && IS_PRESSED_BTN_TL() && IS_PRESSED_BTN_TR())


#ifdef USE_SERIAL_DEBUG
 #define PINB_UART_MASK     (0b00001100)
 #define PINB_DIGITAL_INPUT_MASK   (0b11100000)  //PB[7..5]
#else
 #define PINB_DIGITAL_INPUT_MASK   (0b11101100)  //PB[7..5] plus PB[3..2]
 #define PINB_UART_MASK     (0b00000000)
#endif

#define PINC_DIGITAL_INPUT_MASK   (0b00111111)  //PC[5..0]

#define PINB_IN0_MASK      (0b11000000)   //the pins from PINB that are used in IN0
#define PINC_IN0_MASK      (0b00111111)   //the pins from PINC that are used in IN0

#define PINB_IN1_MASK      (0b11000000)   //the pins from PINB that are used in IN1
#define PINC_IN1_MASK      (0b00111110)   //the pins from PINC that are used in IN1

#define PINA_IN2_MASK      (0b11110000)   //the pins from PINA that are used in IN2 extended input register
#define PINB_IN2_MASK      (0b00001100)   //the pins from PINB that are used in IN2
#define PINC_IN2_MASK      (0b00000001)   //the pins from PINC that are used in IN2

#define PINA_ADC_MASK      (0b11110000)   //the pins in port A used for ADC

void resetViaSWR() {
  _PROTECTED_WRITE(RSTCTRL.SWRR,1);
}

#define BACKLIGHT_FLASH_MILLIS 150
uint8_t g_backlight_is_flashing = 0;    //set this to the number of flashes (off/on cycles) to perform

#ifdef USE_EEPROM
bool g_eeprom_needs_saving = false;
#endif
inline void eeprom_save_deferred()
{
#ifdef USE_EEPROM
  g_eeprom_needs_saving = true;
#endif
}

void eeprom_save_now()
{
#ifdef USE_EEPROM
  uint8_t *eeprom_data_ptr = (uint8_t *) &eeprom_data;
  uint8_t i;

  for(i=0;i<sizeof(eeprom_data);i++)
  {
    EEPROM.update(i, *eeprom_data_ptr++);
  }
  g_eeprom_needs_saving = false;
#endif
}

void eeprom_save_if_needed()
{
#ifdef USE_EEPROM
  if(g_eeprom_needs_saving)
    eeprom_save_now();
#endif    
}



void eeprom_restore_data()
{
#ifdef USE_EEPROM
  uint8_t *eeprom_data_ptr = (uint8_t *)&eeprom_data;
  uint8_t i;
  
  for(i=0;i<sizeof(eeprom_data);i++)
  {
    *eeprom_data_ptr = EEPROM.read(i);
    eeprom_data_ptr++;
  }
#endif
  
  if(!(eeprom_data.manuf_ID == MANUF_ID && eeprom_data.device_ID == DEVICE_ID && eeprom_data.version_ID == VERSION_NUMBER))
  {
    //load defaults
    eeprom_data.manuf_ID = MANUF_ID;
    eeprom_data.device_ID = DEVICE_ID;
    eeprom_data.version_ID = VERSION_NUMBER;

#ifdef USE_PWM_BACKLIGHT
    eeprom_data.sec_config_backlight = CONFIG_BACKLIGHT_STEP_DEFAULT;
#else
    eeprom_data.sec_config_backlight = 0;
#endif
    eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_ADDR;
    eeprom_data.sec_secondary_i2c_addr = CONFIG_I2C_2NDADDR;

    eeprom_data.joy_adc_conf_bits = ADCS_AVAILABLE;



    struct joy_config0_bit_struct *config0_ptr = (struct joy_config0_bit_struct *) &(eeprom_data.joy_config0);

#ifdef USE_DIGITAL_BUTTON_DEBOUNCING    
    config0_ptr->debounce_on = 1;
#else
    config0_ptr->debounce_on = 0;
#endif
  }


#ifndef CONFIG_I2C_2NDADDR  
  eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_ADDR;
#endif



  i2c_secondary_registers.config_backlight = eeprom_data.sec_config_backlight;
  i2c_secondary_registers.joystick_i2c_addr = eeprom_data.sec_joystick_i2c_addr;
  i2c_secondary_registers.secondary_i2c_addr = eeprom_data.sec_secondary_i2c_addr;


  if((eeprom_data.joy_adc_conf_bits & 0xF0) != ADCS_AVAILABLE)
  {
    eeprom_data.joy_adc_conf_bits = ADCS_AVAILABLE;    //keep the ADCs turned off in this odd case that shouldn't really happen (except maybe during testing)
  }
  i2c_joystick_registers.adc_conf_bits = eeprom_data.joy_adc_conf_bits;


    
  i2c_joystick_registers.config0 = eeprom_data.joy_config0; 

  eeprom_save_now();
}

enum backlight_flash_state_enum {
    NOT_FLASHING,
    FLASHING_BACKLIGHT_OFF,
    FLASHING_BACKLIGHT_ON    
};

void backlight_start_flashing(uint8_t num_flashes)
{
#ifdef PIN_BACKLIGHT_PWM  
  g_backlight_is_flashing = num_flashes;
#endif
}

void backlight_process_flashing()
{
#ifdef PIN_BACKLIGHT_PWM  
  static unsigned long last_millis = 0;
  static enum backlight_flash_state_enum backlight_flash_state = NOT_FLASHING;
  
  if(g_backlight_is_flashing == 0)
    return;
    
  unsigned long curr_millis = millis();

  if(backlight_flash_state == NOT_FLASHING)
  {
    last_millis = curr_millis;
    digitalWrite(PIN_BACKLIGHT_PWM, LOW);   //turn backlight off
    backlight_flash_state = FLASHING_BACKLIGHT_OFF;
    return;
  }
  
  if((curr_millis - last_millis) >= BACKLIGHT_FLASH_MILLIS)
  {
    if(backlight_flash_state == FLASHING_BACKLIGHT_OFF)
    {
      digitalWrite(PIN_BACKLIGHT_PWM, HIGH);   //turn backlight on
      backlight_flash_state = FLASHING_BACKLIGHT_ON;
    }
    else if(backlight_flash_state == FLASHING_BACKLIGHT_ON)
    {
      g_backlight_is_flashing--;
      if(g_backlight_is_flashing == 0)
      {
        backlight_flash_state = NOT_FLASHING;
#ifdef USE_PWM_BACKLIGHT
        analogWrite(PIN_BACKLIGHT_PWM, backlight_pwm_steps[i2c_secondary_registers.config_backlight]);
#else
        digitalWrite(PIN_BACKLIGHT_PWM, HIGH);
#endif
      }
      else
      {
        digitalWrite(PIN_BACKLIGHT_PWM, LOW);   //turn backlight off
        backlight_flash_state = FLASHING_BACKLIGHT_OFF;
      }
    }
    else
    {
      //Serial.println("backlight_process_flashing: ERROR!");
      //ERROR
    }    
    last_millis = curr_millis;
  }

#endif
}

void setup_adc0_to_adc3()
{
  if(i2c_joystick_registers.adc_conf_bits & (1 << 0))
    PORTA_PIN4CTRL &= ~PORT_PULLUPEN_bm;  //clear pullup when using ADC0
  else
  {
    PORTA_PIN4CTRL |= PORT_PULLUPEN_bm;   //set pullup when not using ADC0

    word adc = MAX_ADC / 2;
    i2c_joystick_registers.a0_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a1a0_lsb = (adc << (4 - (ADC_RESOLUTION - 8)) & 0x0F) | (i2c_joystick_registers.a1a0_lsb & 0xF0);    
  }
    
  if(i2c_joystick_registers.adc_conf_bits & (1 << 1))
    PORTA_PIN5CTRL &= ~PORT_PULLUPEN_bm;  //clear pullup when using ADC1
  else
  {
    PORTA_PIN5CTRL |= PORT_PULLUPEN_bm;   //set pullup when not using ADC1
    
    word adc = MAX_ADC / 2;
    i2c_joystick_registers.a1_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a1a0_lsb = (adc << (8 - (ADC_RESOLUTION - 8)) & 0xF0) | (i2c_joystick_registers.a1a0_lsb & 0x0F);
  }

    
  if(i2c_joystick_registers.adc_conf_bits & (1 << 2))
    PORTA_PIN6CTRL &= ~PORT_PULLUPEN_bm;  //clear pullup when using ADC2
  else
  {
    PORTA_PIN6CTRL |= PORT_PULLUPEN_bm;   //set pullup when not using ADC2

    word adc = MAX_ADC / 2;
    i2c_joystick_registers.a2_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a3a2_lsb = (adc << (4 - (ADC_RESOLUTION - 8)) & 0x0F) | (i2c_joystick_registers.a3a2_lsb & 0xF0);
  }
  
  if(i2c_joystick_registers.adc_conf_bits & (1 << 3))
    PORTA_PIN7CTRL &= ~PORT_PULLUPEN_bm;  //clear pullup when using ADC3
  else
  {
    PORTA_PIN7CTRL |= PORT_PULLUPEN_bm;   //set pullup when not using ADC3
    
    word adc = MAX_ADC / 2;
    i2c_joystick_registers.a3_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a3a2_lsb = (adc << (8 - (ADC_RESOLUTION - 8)) & 0xF0) | (i2c_joystick_registers.a3a2_lsb & 0x0F);
  }   
}

void setup_config0(void)
{
#ifndef USE_DIGITAL_BUTTON_DEBOUNCING    
  struct joy_config0_bit_struct *config0_ptr = (struct joy_config0_bit_struct *) &i2c_joystick_registers.config0;

  config0_ptr->debounce_on = 0;
#endif
}

void setup_gpio(void)
{
  //set as inputs

  PORTA_DIRCLR = PINA_ADC_MASK;
  PORTB_DIRCLR = PINB_DIGITAL_INPUT_MASK;
  PORTC_DIRCLR = PINC_DIGITAL_INPUT_MASK;

  //set pullups

//#define PINA_MASK (0b00001110) for ADC
  //PORTA_PIN0CTRL = PORT_PULLUPEN_bm;    //PA0 is UPDI
  //PORTA_PIN1CTRL = PORT_PULLUPEN_bm;    //PA1 is used for analog input (DPAD)
  //PORTA_PIN2CTRL = PORT_PULLUPEN_bm;    //PA2 = Poweroff Out _OUTPUT_
  //PORTA_PIN3CTRL = PORT_PULLUPEN_bm;    //PA3 = nINT _OUTPUT_


  setup_adc0_to_adc3();
  setup_config0();

  //PORTB_PIN0CTRL = PORT_PULLUPEN_bm;    //i2c
  //PORTB_PIN1CTRL = PORT_PULLUPEN_bm;    //i2c
#ifndef USE_SERIAL_DEBUG
  PORTB_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN3CTRL = PORT_PULLUPEN_bm;
#endif
  //PORTB_PIN4CTRL = PORT_PULLUPEN_bm;      //PB4 is nINT
#if !defined(CONFIG_INVERT_POWER_BUTTON)    //don't use a pullup on the power button
  PORTB_PIN5CTRL = PORT_PULLUPEN_bm;    //PB5 = BTN_POWER
#endif

#ifndef USE_MUX_ON_PC4_TO_PB7    //we don't need pull-ups on these, if we're using the MUX, as it will always drive high or low
  PORTB_PIN6CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN7CTRL = PORT_PULLUPEN_bm;
#endif

//#define PINC_MASK (0b00111111)
#ifndef USE_MUX_ON_PC0_TO_PC3  //we don't need pull-ups on these, if we're using the MUX, as it will always drive high or low
  PORTC_PIN0CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN1CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN3CTRL = PORT_PULLUPEN_bm;
#endif
#ifndef USE_MUX_ON_PC4_TO_PB7    //we don't need pull-ups on these, if we're using the MUX, as it will always drive high or low
  PORTC_PIN4CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN5CTRL = PORT_PULLUPEN_bm;
#endif
  //PORTC_PIN6CTRL = PORT_PULLUPEN_bm;    //there is no PC6
  //PORTC_PIN7CTRL = PORT_PULLUPEN_bm;    //there is no PC7
}

#ifdef USE_DIGITAL_BUTTON_DEBOUNCING

#define DEBOUNCE_JUST_PRESSED   0b1111111000000000
#define DEBOUNCE_JUST_RELEASED  0b0000000001111111
#define DEBOUNCE_MASK           (DEBOUNCE_JUST_PRESSED | DEBOUNCE_JUST_RELEASED)
#define DEBOUNCE_PRESSED        0b0000000000000000
#define DEBOUNCE_RELEASED       0b1111111111111111


uint16_t g_history_input0[8];
uint16_t g_history_input1[8];
uint16_t g_history_input2[8];

void debounce_inputs(uint8_t *input0, uint8_t *input1, uint8_t *input2)
{
  struct joy_config0_bit_struct *config0_ptr;
  static uint8_t prev_input0 = 0xFF;
  static uint8_t prev_input1 = 0xFF;
  static uint8_t prev_input2 = 0xFF;
  uint8_t i;

  config0_ptr = (struct joy_config0_bit_struct *) &i2c_joystick_registers.config0;
  
  if(!config0_ptr->debounce_on)
  {
    return;
  }

  for(i=0; i<8; i++)
  {
    g_history_input0[i] <<= 1;
    g_history_input1[i] <<= 1;
    g_history_input2[i] <<= 1;

    g_history_input0[i] |= (*input0 >> i) & 0x01;
    g_history_input1[i] |= (*input1 >> i) & 0x01;
    g_history_input2[i] |= (*input2 >> i) & 0x01;




    //using the almost ultimate from https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/ for now

    
    if((g_history_input0[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_PRESSED)
    { 
        g_history_input0[i] = DEBOUNCE_PRESSED;
    }
    else if((g_history_input0[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_RELEASED)
    { 
        g_history_input0[i] = DEBOUNCE_RELEASED;
    }
    

    if(g_history_input0[i] == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input0 = *input0 | (1 << i);
    }
    else if(g_history_input0[i] == DEBOUNCE_PRESSED)   //is button DOWN?
    {
      *input0 = *input0 & ~(1 << i);
    }
    else
    {
      //set it to whatever it was last time
      *input0 = (*input0 & ~(1 << i)) | (prev_input0 & (1 << i));
    }


    
    if((g_history_input1[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_PRESSED)
    { 
        g_history_input1[i] = DEBOUNCE_PRESSED;
    }
    else if((g_history_input1[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_RELEASED)
    { 
        g_history_input1[i] = DEBOUNCE_RELEASED;
    }
    

    if(g_history_input1[i] == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input1 = *input1 | (1 << i);
    }
    else if(g_history_input1[i] == DEBOUNCE_PRESSED)   //is button DOWN?
    {
      *input1 = *input1 & ~(1 << i);
    }
    else
    {
      //set it to whatever it was last time
      *input1 = (*input1 & ~(1 << i)) | (prev_input1 & (1 << i));
    }


    
    if((g_history_input2[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_PRESSED)
    { 
        g_history_input2[i] = DEBOUNCE_PRESSED;
    }
    else if((g_history_input2[i] & DEBOUNCE_MASK) == DEBOUNCE_JUST_RELEASED)
    { 
        g_history_input2[i] = DEBOUNCE_RELEASED;
    }
    

    if(g_history_input2[i] == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input2 = *input2 | (1 << i);
    }
    else if(g_history_input2[i] == DEBOUNCE_PRESSED)   //is button DOWN?
    {
      *input2 = *input2 & ~(1 << i);
    }
    else
    {
      //set it to whatever it was last time
      *input2 = (*input2 & ~(1 << i)) | (prev_input2 & (1 << i));
    }    
  }

  prev_input0 = *input0;
  prev_input1 = *input1;
  prev_input2 = *input2;  
}
#endif

/*
 * This function sets up digital input registers input0, input1, input2
 * It's trying to be as efficient as possible to do all of the digital inputs.
 * 
 * Note:  The DPAD buttons (and potentially LeftCenter and RightCenter) are 
 *        actually analog reads that get converted to bits in the input registers.
 */
void read_digital_inputs(void)
{
  //we may want to turn off interrupts during this function
  //at the least, we need to poll the registers PA, PB, PC once
  //then set up the input registers as TEMPORARY registers
  //then put them in the actual i2c_joystick_registers 
  //otherwise, if we get interrupted during the setup, things get out of whack

  uint8_t input0, input1, input2;
  uint8_t pa_in = PORTA_IN;
  uint8_t pb_mux0_in;
  uint8_t pc_mux0_in;

    

#ifdef PIN_MUX_SELECT
  digitalWrite(PIN_MUX_SELECT, LOW);
#endif
  //must read these "mux0" ports AFTER setting PIN_MUX_SELECT to 0
  pb_mux0_in = PORTB_IN;
  pc_mux0_in = PORTC_IN;

  input0 = (pc_mux0_in & PINC_IN0_MASK) | (pb_mux0_in & PINB_IN0_MASK);

#ifdef PIN_MUX_SELECT
  //need to switch MUX select pin and re-read GPIO
  digitalWrite(PIN_MUX_SELECT, HIGH);

  //must read these "mux1" ports AFTER setting PIN_MUX_SELECT to 1
  uint8_t pb_mux1_in = PORTB_IN;
  uint8_t pc_mux1_in = PORTC_IN;

 #if defined(USE_MUX_ON_PC0_TO_PC3) && defined(USE_MUX_ON_PC4_TO_PB7)
  input1 = (pc_mux1_in & PINC_IN1_MASK) | (pb_mux1_in & PINB_IN1_MASK);
 #elif defined(USE_MUX_ON_PC0_TO_PC3)
  input1 = (pc_mux1_in & PINC_IN1_MASK & 0b00001111) | 0b11110000;
 #elif defined(USE_MUX_ON_PC4_TO_PB7)
  input1 = ((pb_mux1_in & PINB_IN1_MASK) & (pc_mux1_in & PINC_IN1_MASK & 0b11110000)) | 0b00001111;
 #else
  input1 = 0b11111111;
 #endif
#else
  input1 = 0b11111111;
#endif

#ifdef CONFIG_INVERT_POWER_BUTTON
  pb_mux0_in ^= PINB_POWER_BUTTON;
#endif

  if(pb_mux0_in & PINB_POWER_BUTTON)
    input1 = (input1 | INPUT1_BTN_POWER);       //set the bit to 1
  else
    input1 = (input1 & ~INPUT1_BTN_POWER);      //set the bit to 0


 
#if defined(PIN_MUX_SELECT) && defined(USE_MUX_ON_PC0_TO_PC3)
  input2 = (pa_in & PINA_IN2_MASK) | (pb_mux0_in & PINB_IN2_MASK) | (1 << 1) | (pc_mux1_in & PINC_IN2_MASK);
#else
  input2 = (pa_in & PINA_IN2_MASK) | (pb_mux0_in & PINB_IN2_MASK) | (1 << 1) | (1 << 0);
#endif

#ifdef USE_DIGITAL_BUTTON_DEBOUNCING
  debounce_inputs(&input0, &input1, &input2);
#endif

  i2c_joystick_registers.input0 = input0;
  i2c_joystick_registers.input1 = input1;
  i2c_joystick_registers.input2 = input2;
}

#if defined(USE_ADC0) || defined(USE_ADC1) || defined(USE_ADC2) || defined(USE_ADC3)
void read_analog_inputs()
{
  word adc;

#ifdef USE_ADC0
  if(i2c_joystick_registers.adc_conf_bits & (1 << 0))
  {
    adc = analogRead(PIN_PA4);
    i2c_joystick_registers.a0_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a1a0_lsb = (adc << (4 - (ADC_RESOLUTION - 8)) & 0x0F) | (i2c_joystick_registers.a1a0_lsb & 0xF0);
  }
#endif
#ifdef USE_ADC1
  if(i2c_joystick_registers.adc_conf_bits & (1 << 1))
  {
    adc = analogRead(PIN_PA5);
    i2c_joystick_registers.a1_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a1a0_lsb = (adc << (8 - (ADC_RESOLUTION - 8)) & 0xF0) | (i2c_joystick_registers.a1a0_lsb & 0x0F);
  }
#endif
#ifdef USE_ADC2
  if(i2c_joystick_registers.adc_conf_bits & (1 << 2))
  {
    adc = analogRead(PIN_PA6);
    i2c_joystick_registers.a2_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a3a2_lsb = (adc << (4 - (ADC_RESOLUTION - 8)) & 0x0F) | (i2c_joystick_registers.a3a2_lsb & 0xF0);
  }
#endif
#ifdef USE_ADC3
  if(i2c_joystick_registers.adc_conf_bits & (1 << 3))
  {
    adc = analogRead(PIN_PA7);
    i2c_joystick_registers.a3_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a3a2_lsb = (adc << (8 - (ADC_RESOLUTION - 8)) & 0xF0) | (i2c_joystick_registers.a3a2_lsb & 0x0F);
  }
#endif  
}
#endif

#define SPECIAL_LOOP_DELAY 0x3FFF
#define POWEROFF_HOLD_SECONDS 20

void process_special_inputs()
{  
  //static uint32_t power_button_loop_counter = POWER_LOOP_DELAY;
  //static bool power_btn_prev_state = false;
  

#ifdef USE_PWM_BACKLIGHT
  static uint16_t special_inputs_loop_counter = 0;
  if(IS_SPECIAL_INPUT_MODE() && (IS_PRESSED_DPAD_UP() || IS_PRESSED_DPAD_DOWN()))
  {
    if(special_inputs_loop_counter)   
    {
      //we're delay looping
      special_inputs_loop_counter--;
    }
    else
    {
      special_inputs_loop_counter = SPECIAL_LOOP_DELAY;

      if(IS_PRESSED_DPAD_UP())
      {
        if(i2c_secondary_registers.config_backlight < (NUM_BACKLIGHT_PWM_STEPS-1))
          i2c_secondary_registers.config_backlight++;
      }
      else if(IS_PRESSED_DPAD_DOWN())
      {
        if(i2c_secondary_registers.config_backlight > 0)
          i2c_secondary_registers.config_backlight--;
      }
    } 
  }
  else
  {
    special_inputs_loop_counter = 0x10;  //tiny delay just for debounce
  }
#endif

#ifdef PIN_POWEROFF_OUT
  static unsigned long power_btn_start_millis = 0;
  static bool prev_pressed_btn_power = false;
  if(IS_PRESSED_BTN_POWER())
  {
    if(prev_pressed_btn_power)
    {
      unsigned long current_millis = millis();

      if((current_millis - power_btn_start_millis) >= (POWEROFF_HOLD_SECONDS * 1000))         //shutdown!
      {
        digitalWrite(PIN_POWEROFF_OUT,HIGH);
      }
      else if((current_millis - power_btn_start_millis) >= (POWEROFF_HOLD_SECONDS * 900))    //90% of the way to shutdown
      {
        backlight_start_flashing(1);    //this will keep flashing until the power button is released
      }
    }
    else
    {
      power_btn_start_millis = millis();
      prev_pressed_btn_power = true;
    }
  }
  else if(prev_pressed_btn_power)
  {
    prev_pressed_btn_power = false;
  }
#endif  
}


//this is the function that receives bytes from the i2c master for the PRIMARY
// i2c address (which is for the joystick functionality)
inline void receive_i2c_callback_main_address(int i2c_bytes_received)
{
  //g_i2c_command_index %= sizeof(i2c_joystick_registers);

  for (byte x = g_i2c_command_index ; x < (g_i2c_command_index + i2c_bytes_received - 1) ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    if(x == REGISTER_ADC_CONF_BITS)   //this is a writeable register
    {
      uint8_t mask;
      temp &= 0x0F;   //the top nibble is not writeable
      mask = (i2c_joystick_registers.adc_conf_bits >> 4);   //the high nibble defines what ADCs are available
      i2c_joystick_registers.adc_conf_bits = (i2c_joystick_registers.adc_conf_bits & 0xF0) | (temp & mask);     //turn on any bits that are available and requested (turn all others off)
      eeprom_data.joy_adc_conf_bits = i2c_joystick_registers.adc_conf_bits;
      setup_adc0_to_adc3();   //turn on/off ADC pullups
      eeprom_save_deferred();
    }

    if(x == REGISTER_CONFIG_BITS)   //this is a writeable register
    {
      i2c_joystick_registers.config0 = temp;
      eeprom_data.joy_config0 = i2c_joystick_registers.config0;
      setup_config0();
      eeprom_save_deferred();
    }
  }
}

#ifdef CONFIG_I2C_2NDADDR  
//this is the function that receives bytes from the i2c master for the SECONDARY
// i2c address (which is for the joystick functionality)
inline void receive_i2c_callback_secondary_address(int i2c_bytes_received)
{
  //g_i2c_command_index %= sizeof(i2c_secondary_registers);
  
  //what do we do when we receive bytes on the 2nd address?
  for (byte x = g_i2c_command_index ; x < (g_i2c_command_index + i2c_bytes_received - 1) ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    if(x == REGISTER_SEC_JOY_I2C_ADDR)
    {
      i2c_secondary_registers.joystick_i2c_addr = temp;      
    }
    else if(x == REGISTER_SEC_SEC_I2C_ADDR)
    {
      i2c_secondary_registers.secondary_i2c_addr = temp;      
    }    
    else if(x == REGISTER_SEC_POWER_CONTROL)
    {
      backlight_start_flashing(temp);     //change this at some point!
      
      //we would use this as a way for the i2c master (host system) to tell us about power related stuff (like if the battery is getting low)
      i2c_secondary_registers.power_control = temp;      
    }
#ifdef USE_PWM_BACKLIGHT
    else if(x == REGISTER_SEC_CONFIG_BACKLIGHT)   //this is a writeable register
    {
      //set backlight value
      if(temp >= 0 && temp < NUM_BACKLIGHT_PWM_STEPS)
        i2c_secondary_registers.config_backlight = temp;      
    }
#endif
  }
}
#endif

//When Qwiic Joystick receives data bytes from Master, this function is called as an interrupt
//(Serves rewritable I2C address)
void receive_i2c_callback(int i2c_bytes_received)
{
  Wire.getBytesRead(); // reset count of bytes read. We don't do anything with it here, but a write is going to reset it to a new value.

  g_i2c_command_index = Wire.read(); //Get the memory map offset from the user
  
#ifdef CONFIG_I2C_2NDADDR  
  g_i2c_address = Wire.getIncomingAddress() >> 1;

  if(g_i2c_address == CONFIG_I2C_2NDADDR)
    receive_i2c_callback_secondary_address(i2c_bytes_received);
  else
    receive_i2c_callback_main_address(i2c_bytes_received);
#else

  receive_i2c_callback_main_address(i2c_bytes_received);

#endif
}

//this is the function that sends bytes from the i2c slave to master for the PRIMARY
// i2c address (which is for the joystick functionality)
inline void request_i2c_callback_primary_address()
{
  g_i2c_index_to_read = Wire.getBytesRead() + g_i2c_command_index;
  
  g_i2c_index_to_read %= sizeof(i2c_joystick_registers);

  Wire.write((((byte *)&i2c_joystick_registers) + g_i2c_index_to_read), sizeof(i2c_joystick_registers) - g_i2c_index_to_read);

  //TODO: fixme?
  if(g_i2c_index_to_read == 0)
  {
    //if the index was 0, just assume we sent at least 2 bytes.  I don't know how to tell
    //could use getBytesRead combined with g_i2c_command_index
    g_last_sent_input0 = i2c_joystick_registers.input0;
    g_last_sent_input1 = i2c_joystick_registers.input1;
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
  else if(g_i2c_index_to_read == 1)
  {
    g_last_sent_input1 = i2c_joystick_registers.input1;
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
  else if(g_i2c_index_to_read == 2)
  {
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
}

#ifdef CONFIG_I2C_2NDADDR  
//this is the function that sends bytes from the i2c slave to master for the PRIMARY
// i2c address (which is for the joystick functionality)
inline void request_i2c_callback_secondary_address()
{
    g_i2c_index_to_read = Wire.getBytesRead() + g_i2c_command_index;
    g_i2c_index_to_read %= sizeof(i2c_joystick_registers);
    
    Wire.write((((byte *)&i2c_secondary_registers) + g_i2c_index_to_read), sizeof(i2c_secondary_registers) - g_i2c_index_to_read);
}
#endif

//Respond to GET commands
void request_i2c_callback()
{
  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.

#ifdef CONFIG_I2C_2NDADDR  
  if(g_i2c_address == CONFIG_I2C_2NDADDR)
  {
      request_i2c_callback_secondary_address();
  }
  else
  {
    request_i2c_callback_primary_address();
  }
#else
  request_i2c_callback_primary_address();
#endif
}

//Begin listening on I2C bus as I2C slave using the global variable setting_i2c_address
void startI2C()
{
  Wire.end(); //Before we can change addresses we need to stop

#ifdef CONFIG_I2C_2NDADDR
  Wire.begin(i2c_secondary_registers.joystick_i2c_addr, 0, (i2c_secondary_registers.secondary_i2c_addr << 1) | 0x01);
#else
  Wire.begin(i2c_secondary_registers.joystick_i2c_addr); //Start I2C and answer calls using address from EEPROM
#endif

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receive_i2c_callback);
  Wire.onRequest(request_i2c_callback);
}


unsigned int Period=0xFFFF;

void setFrequency(unsigned long freqInHz) {
  unsigned long tempperiod = (F_CPU / freqInHz);
  byte presc = 0;
  while (tempperiod > 65536 && presc < 7) {
    presc++;
    tempperiod      = tempperiod >> (presc > 4 ? 2 : 1);
  }
  Period            = tempperiod;
  TCA0.SINGLE.CTRLA = (presc << 1) | TCA_SINGLE_ENABLE_bm;
  TCA0.SINGLE.PER   = Period;
}

void setup() 
{
  //memset(&i2c_joystick_registers, 0, sizeof(i2c_joystick_registers));
  
  i2c_joystick_registers.adc_res = ADC_RESOLUTION;

  //default to middle
  i2c_joystick_registers.a0_msb = 0x7F;
  i2c_joystick_registers.a1_msb = 0x7F;
  i2c_joystick_registers.a1a0_lsb = 0xFF;

  i2c_joystick_registers.a2_msb = 0x7F;
  i2c_joystick_registers.a3_msb = 0x7F;
  i2c_joystick_registers.a3a2_lsb = 0xFF;

  i2c_joystick_registers.manuf_ID = MANUF_ID;
  i2c_joystick_registers.device_ID = DEVICE_ID;
  i2c_joystick_registers.version_ID = VERSION_NUMBER;

  i2c_secondary_registers.manuf_ID = MANUF_ID;
  i2c_secondary_registers.device_ID = DEVICE_ID;
  i2c_secondary_registers.version_ID = VERSION_NUMBER;

  i2c_secondary_registers.power_control = 0;

  i2c_secondary_registers.features_available = FEATURES_AVAILABLE;
  
  i2c_joystick_registers.adc_conf_bits = ADCS_AVAILABLE;


#ifdef USE_PWM_BACKLIGHT
  i2c_secondary_registers.backlight_max = NUM_BACKLIGHT_PWM_STEPS-1;
#else
  i2c_secondary_registers.backlight_max = 0;
#endif

  eeprom_restore_data();      //should be done beore startI2C()
  
  
  
#ifdef PIN_nINT
  pinMode(PIN_nINT, OUTPUT);
  digitalWrite(PIN_nINT, g_nINT_state);
#endif 


#ifdef USE_SERIAL_DEBUG
  Serial.begin(115200);
  delay(500);
  Serial.println("Freeplay Joystick Debugging");
  Serial.flush();
#endif

#if (defined(USE_ADC0) || defined(USE_ADC1) || defined(USE_ADC2) || defined(USE_ADC3)) && (ADC_RESOLUTION != 10)
  analogReadResolution(ADC_RESOLUTION);
#endif

#ifdef PIN_BACKLIGHT_PWM
  pinMode(PIN_BACKLIGHT_PWM, OUTPUT);  // sets the pin as output
#endif

#ifdef PIN_POWEROFF_OUT
  pinMode(PIN_POWEROFF_OUT, OUTPUT);
  digitalWrite(PIN_POWEROFF_OUT, LOW);
#endif

#ifdef PIN_MUX_SELECT
  pinMode(PIN_MUX_SELECT, OUTPUT);
  digitalWrite(PIN_MUX_SELECT, LOW);
#endif


  
  
#ifdef USE_PWM_BACKLIGHT
  g_pwm_step = ~i2c_secondary_registers.config_backlight; //unset it
#endif

  setup_gpio(); //make sure we call this AFTER we set restore eeprom data

  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

/*
 *  CLK       39.22 kHz
    CLK/2     19.61 kHz
    CLK/4     9804 Hz
    CLK/8     4902 Hz
    CLK/16    2451 Hz
    CLK/32    1225 Hz   (UNAVAILABLE)
    CLK/64    612.7 Hz
    CLK/128   306.4 Hz  (UNAVAILABLE)
    CLK/256   153.2 Hz
    CLK/1024  38.30 Hz
 *
 *    we want our PWM to be at 19.61kHz (instead of the default 612.7Hz), so it's in the inaudible range
 */
  TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV2_gc | TCA_SINGLE_ENABLE_bm;
}


void loop() 
{
  static unsigned long timer_start_millis = millis();
  unsigned long current_millis;
  


  read_digital_inputs();
  process_special_inputs();     //make sure special inputs are digital (input0 and input1) only
  if(g_backlight_is_flashing)
    backlight_process_flashing();

#if defined(USE_ADC0) || defined(USE_ADC1) || defined(USE_ADC2) || defined(USE_ADC3)
  read_analog_inputs();
#endif  

#ifdef PIN_BACKLIGHT_PWM

  if(g_pwm_step != i2c_secondary_registers.config_backlight)
  {
    //output the duty cycle to the pwm pin (PA3?)
    if(i2c_secondary_registers.config_backlight >= NUM_BACKLIGHT_PWM_STEPS)
      i2c_secondary_registers.config_backlight = NUM_BACKLIGHT_PWM_STEPS-1;
    
    analogWrite(PIN_BACKLIGHT_PWM, backlight_pwm_steps[i2c_secondary_registers.config_backlight]);
    g_pwm_step = i2c_secondary_registers.config_backlight;

    eeprom_data.sec_config_backlight = i2c_secondary_registers.config_backlight;
    eeprom_save_deferred();
  }
#endif

  if(eeprom_data.sec_joystick_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
  {
    if(i2c_secondary_registers.joystick_i2c_addr >= 0x10 && i2c_secondary_registers.joystick_i2c_addr < 0x70 && i2c_secondary_registers.secondary_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
    {
      eeprom_data.sec_joystick_i2c_addr = i2c_secondary_registers.joystick_i2c_addr;
      eeprom_save_now();    //save to EEPROM right away, becuase we'll reboot next
      
      resetViaSWR();    //reboot the chip to use new address
    }
    
    resetViaSWR();    //reboot the chip to use old address
  }

  if(eeprom_data.sec_secondary_i2c_addr != i2c_secondary_registers.secondary_i2c_addr)
  {
    if(i2c_secondary_registers.secondary_i2c_addr >= 0x10 && i2c_secondary_registers.secondary_i2c_addr < 0x70 && i2c_secondary_registers.secondary_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
    {
      eeprom_data.sec_secondary_i2c_addr = i2c_secondary_registers.secondary_i2c_addr;
      eeprom_save_now();    //save to EEPROM right away, becuase we'll reboot next
      
      resetViaSWR();    //reboot the chip to use new address
    }
    
    resetViaSWR();    //reboot the chip to use old address
  }  

#ifdef PIN_nINT
  bool new_nINT = ((g_last_sent_input0 == i2c_joystick_registers.input0) && (g_last_sent_input1 == i2c_joystick_registers.input1) && (g_last_sent_input2 == i2c_joystick_registers.input2));
  if(g_nINT_state != new_nINT)
  {
    digitalWrite(PIN_nINT, new_nINT);
    g_nINT_state = new_nINT;    
  }
#endif

  //run a 5-second timer to do certain periodic tasks
  current_millis = millis();
  if(current_millis - timer_start_millis >= 5000)
  {
    eeprom_save_if_needed();      //if any deferred eeprom saves occurred, write to eeprom
    
    timer_start_millis = current_millis;

#ifdef USE_SERIAL_DEBUG
  //print some debugging stuff every 5 seconds
  /*
  Serial.print("g_i2c_address=");
  Serial.print(g_i2c_address);
  Serial.print(" g_i2c_command_index=");
  Serial.print(g_i2c_command_index);
  Serial.print(" g_i2c_index_to_read=");
  Serial.print(g_i2c_index_to_read);

  Serial.println();*/
#endif    
  }
}
