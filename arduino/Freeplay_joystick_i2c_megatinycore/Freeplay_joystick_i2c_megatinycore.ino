//Freeplay_joystick_i2c_attiny
// Firmware for the ATtiny1627/ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 1627


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

#define USE_STATUS_LED_PB2
//#define USE_SERIAL_DEBUG      //UART is on PB2/PB3 which shares pins with IO2_2/IO2_3  AND PB2 is also used for the status LED (if desired)

#define USE_DIGITAL_BUTTON_DEBOUNCING

#define USE_HOTKEY_TOGGLE_MODE

//#define USE_SLEEP_MODE  //doesn't seem to work properly, yet

//#define USE_SOFTI2CMASTER

#include <Wire.h>

#ifdef USE_EEPROM
#include <EEPROM.h>
#endif

#ifdef USE_SLEEP_MODE
#include <avr/sleep.h> //Needed for sleep_mode
#endif

#define MANUF_ID         0xED
#define DEVICE_ID        0x00
#define VERSION_NUMBER   18

#define CONFIG_PERIODIC_TASK_TIMER_MILLIS 5000
#define CONFIG_INPUT_READ_TIMER_MICROS 500        //set to 0 for NO delay reading inputs, otherwise try to read inputs at least every CONFIG_INPUT_READ_TIMER_MICROS microseconds

/*
 * note that CONFIG_INPUT_READ_TIMER_MICROS and CONFIG_DEFAULT_DEBOUCE_LEVEL both create potential input lag (multiplicative when using both)
 * if CONFIG_INPUT_READ_TIMER_MICROS=500 and CONFIG_DEFAULT_DEBOUCE_LEVEL=3, that would likely be 2ms=2000us (or more) of input lag
 * CONFIG_INPUT_READ_TIMER_MICROS * (CONFIG_DEFAULT_DEBOUCE_LEVEL + 1) = us of input lag
 */

#ifdef USE_DIGITAL_BUTTON_DEBOUNCING
 #define CONFIG_DEFAULT_DEBOUCE_LEVEL 5      //must be 0 to 7   //0 means no debouncing
 #define DEBOUNCE_LEVEL_MAX 7
#else
 #define CONFIG_DEFAULT_DEBOUCE_LEVEL 0      //0 means unused/off (no debouncing)
 #define DEBOUNCE_LEVEL_MAX 0
#endif

#if DEBOUNCE_LEVEL_MAX > 7
#error Debounce Level is stored as a 3-bit integer
#endif


#define DEFAULT_CONFIG0 (0xF0 | CONFIG_DEFAULT_DEBOUCE_LEVEL)

#define CONFIG_I2C_DEFAULT_PRIMARY_ADDR                     0x30
#ifdef USE_SECONDARY_I2C_ADDR
 #define CONFIG_I2C_DEFAULT_SECONDARY_ADDR          0x40  //0x30 wouldn't work
#endif

bool g_power_button_pressed = false;

#ifdef USE_HOTKEY_TOGGLE_MODE
enum hotkey_mode_enum {
    HOTKEY_JUST_ENTERING,
    HOTKEY_ON,
    HOTKEY_JUST_EXITING,
    HOTKEY_OFF,
    HOTKEY_SPECIAL_INPUT,
    HOTKEY_SPECIAL_INPUT_ENTERING,
    HOTKEY_SPECIAL_INPUT_EXITING,
    HOTKEY_SYSTEM_STARTUP   
} g_hotkey_mode = HOTKEY_SYSTEM_STARTUP;

  byte g_hotkey_input0 = 0xFF;
  byte g_hotkey_input1 = 0xFF;
  byte g_hotkey_input2 = 0xFF;

  byte g_hotkey_combo_button = 0xFF;
#endif

#ifdef USE_SOFTI2CMASTER
#warning To use this, you need to make a small mod to the SoftI2CMaster.h library source code.
// See https://github.com/SpenceKonde/megaTinyCore/discussions/664

#if defined(USE_ADC0) || defined(USE_ADC1)
 #error Can not use ADC0 and ADC1 pins for both ADC and SoftI2CMaster
#endif
#define I2C_TIMEOUT 1000
#define I2C_PULLUP 1

// Set up PA5 for SDA and PA4 for SCL

#define SDA_DDR         0x00 //VPORTA_DIR = 0x00, VPORTB_DIR = 0x04, VPORTC_DIR = 0x08
#define SCL_DDR         0x00 //VPORTA_DIR = 0x00, VPORTB_DIR = 0x04, VPORTC_DIR = 0x08
#define SDA_OUT         0x01 //VPORTA_OUT = 0x01, VPORTB_OUT = 0x05, VPORTC_OUT = 0x09
#define SCL_OUT         0x01 //VPORTA_OUT = 0x01, VPORTB_OUT = 0x05, VPORTC_OUT = 0x09
#define SDA_IN          0x02 //VPORTA_IN  = 0x02, VPORTB_IN  = 0x06, VPORTC_IN  = 0x0A
#define SCL_IN          0x02 //VPORTA_IN  = 0x02, VPORTB_IN  = 0x06, VPORTC_IN  = 0x0A

#define SDA_PIN 5       //PA5
#define SCL_PIN 4       //PA4
#define I2C_FASTMODE 1

#include <SoftI2CMaster.h>

#define USE_QWIIC_JOYSTICK_VIA_SOFTI2CMASTER_AS_ADC0_AND_ADC1

#ifdef USE_QWIIC_JOYSTICK_VIA_SOFTI2CMASTER_AS_ADC0_AND_ADC1
#define SOFTI2C_SLAVE_7BITADDR 0x20
#endif
#endif



//#define ADC_RESOLUTION 10
//#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips
#define ADC_RESOLUTION ADC_NATIVE_RESOLUTION

#if ADC_RESOLUTION > 12
#error ADC Resolution problem
#endif

#define MAX_ADC ((1<<ADC_RESOLUTION)-1)

#ifdef USE_STATUS_LED_PB2
 #define PIN_STATUS_LED  9
#endif

#ifdef USE_INTERRUPTS
 #define PIN_nINT 7                  //AKA PB4
 bool g_nINT_state = false;
#endif

#if defined(USE_MUX_ON_PC0_TO_PC3) || defined(USE_MUX_ON_PC4_TO_PB7)
#define PIN_MUX_SELECT 18
#endif

#define CONFIG_INVERT_POWER_BUTTON      //power button is on PIN_PB5

#define PIN_POWEROFF_OUT        19    //AKA PA2

#ifdef USE_PWM_BACKLIGHT
 #define PIN_BACKLIGHT_PWM       20     //AKA PA3

 #define NUM_BACKLIGHT_PWM_STEPS 13
 uint8_t backlight_pwm_steps[NUM_BACKLIGHT_PWM_STEPS] = {0x00, 0x10, 0x18, 0x20, 0x30, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xD0, 0xE0, 0xFF};
 #define CONFIG_BACKLIGHT_STEP_DEFAULT  5

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

#define WRITE_PROTECT   0xAA
#define WRITE_UNPROTECT 0x55
struct /*i2c_joystick_register_struct */
{
  uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
  uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
  uint8_t input2;          // Reg: 0x02 - INPUT port 2 (extended digital buttons)     BTN_THUMBL and BTN_THUMBR among other things
  uint8_t a0_msb;          // Reg: 0x03 - ADC0 most significant 8 bits
  uint8_t a1_msb;          // Reg: 0x04 - ADC1 most significant 8 bits
  uint8_t a1a0_lsb;        // Reg: 0x05 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
  uint8_t a2_msb;          // Reg: 0x06 - ADC2 most significant 8 bits
  uint8_t a3_msb;          // Reg: 0x07 - ADC2 most significant 8 bits
  uint8_t a3a2_lsb;        // Reg: 0x08 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
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

struct joy_config0_bit_struct
{
    uint8_t debounce_level : 3;
    uint8_t unused3 : 1;
    uint8_t unused4 : 1;
    uint8_t unused5 : 1;
    uint8_t unused6 : 1;
    uint8_t unused7 : 1;
}  * const joy_config0_ptr = (struct joy_config0_bit_struct*)&i2c_joystick_registers.config0;

struct /*i2c_secondary_address_register_struct */
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
#define REGISTER_SEC_BATTERY_CAPACITY   0x08
  uint8_t battery_capacity;  // Reg: 0x08 - battery capacity (0-100 = battery %, 255 = UNKNOWN)
#define REGISTER_SEC_STATUS_LED         0x09   //this one is writeable
  uint8_t status_led_control;// Reg: 0x09 - turn on/off/blinkSlow/blinkFast/etc the blue status LED  (this is actuall a WRITE-ONLY "virtual" register)
#define REGISTER_SEC_WRITE_PROTECT      0x0A   //this one is writeable
  uint8_t write_protect;     // Reg: 0x0A - write 0x00 to make protected registers writeable
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
#ifdef CONFIG_I2C_DEFAULT_SECONDARY_ADDR
 volatile byte g_i2c_address = 0;  //if we're using multiple i2c addresses, we need to know which one is in use
#endif
volatile bool g_read_analog_inputs_asap = true;

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
 * PB2 = IO2_2 = BTN_C (when no Serial debugging AND no status LED output)
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
#define INPUT2_BTN_C      (1 << 2)      //IO2_2 //but this is also used for blue status LED
#define INPUT2_BTN_Z      (1 << 3)      //IO2_3
#define INPUT2_BTN_0      (1 << 4)      //IO2_4
#define INPUT2_BTN_1      (1 << 5)      //IO2_5
#define INPUT2_BTN_2      (1 << 6)      //IO2_6
#define INPUT0_BTN_3      (1 << 7)      //IO2_7

#define PINB_POWER_BUTTON (1 << 5)    //PB5 is the power button, but it needs to be inverted (high when pressed)
                                        

//PRESSED means that the bit is 0

#define IS_PRESSED_BTN_START() ((i2c_joystick_registers.input0 & INPUT0_BTN_START) != INPUT0_BTN_START)
#define IS_PRESSED_BTN_SELECT() ((i2c_joystick_registers.input0 & INPUT0_BTN_SELECT) != INPUT0_BTN_SELECT)

#define IS_PRESSED_BTN_A() ((i2c_joystick_registers.input0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define IS_PRESSED_BTN_B() ((i2c_joystick_registers.input0 & INPUT0_BTN_B) != INPUT0_BTN_B)

#define IS_PRESSED_BTN_TL() ((i2c_joystick_registers.input1 & INPUT1_BTN_TL) != INPUT1_BTN_TL)
#define IS_PRESSED_BTN_TR() ((i2c_joystick_registers.input1 & INPUT1_BTN_TR) != INPUT1_BTN_TR)
#define IS_PRESSED_BTN_Y() ((i2c_joystick_registers.input1 & INPUT1_BTN_Y) != INPUT1_BTN_Y)

//#define IS_PRESSED_BTN_POWER() ((i2c_joystick_registers.input1 & INPUT1_BTN_POWER) != INPUT1_BTN_POWER)

#define IS_PRESSED_BTN_POWER_INPUT1(in1) ((in1 & INPUT1_BTN_POWER) != INPUT1_BTN_POWER)

#ifdef USE_HOTKEY_TOGGLE_MODE
 #define IS_PRESSED_SPECIAL_INPUT_DPAD_UP()   (g_hotkey_mode == HOTKEY_SPECIAL_INPUT ? ((g_hotkey_input0 & INPUT0_DPAD_UP  ) != INPUT0_DPAD_UP  ) : ((i2c_joystick_registers.input0 & INPUT0_DPAD_UP  ) != INPUT0_DPAD_UP))
 #define IS_PRESSED_SPECIAL_INPUT_DPAD_DOWN() (g_hotkey_mode == HOTKEY_SPECIAL_INPUT ? ((g_hotkey_input0 & INPUT0_DPAD_DOWN) != INPUT0_DPAD_DOWN) : ((i2c_joystick_registers.input0 & INPUT0_DPAD_DOWN) != INPUT0_DPAD_DOWN))

 #define IS_SPECIAL_INPUT_MODE() (g_hotkey_mode == HOTKEY_SPECIAL_INPUT)
#else
 #define IS_PRESSED_SPECIAL_INPUT_DPAD_UP()   ((i2c_joystick_registers.input0 & INPUT0_DPAD_UP  ) != INPUT0_DPAD_UP)
 #define IS_PRESSED_SPECIAL_INPUT_DPAD_DOWN() ((i2c_joystick_registers.input0 & INPUT0_DPAD_DOWN) != INPUT0_DPAD_DOWN)

 #define IS_SPECIAL_INPUT_MODE() (IS_PRESSED_BTN_TL() && IS_PRESSED_BTN_TR() && IS_PRESSED_BTN_Y())
#endif

#ifdef USE_SERIAL_DEBUG
#ifdef USE_STATUS_LED_PB2
#error STATUS LED and SERIAL DEBUG share pins
#endif
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

#if defined(USE_STATUS_LED_PB2)
#define PINB_IN2_MASK      (0b00001000)   //the pins from PINB that are used in IN2
#else
#define PINB_IN2_MASK      (0b00001100)   //the pins from PINB that are used in IN2
#endif
#define PINC_IN2_MASK      (0b00000001)   //the pins from PINC that are used in IN2

#define PINA_ADC_MASK      (0b11110000)   //the pins in port A used for ADC

void resetViaSWR() {
  delay(100);   //sometimes EEPROM writes weren't finished, so we add a delay
  _PROTECTED_WRITE(RSTCTRL.SWRR,1);
}

#define BACKLIGHT_FLASH_MILLIS 150
uint8_t g_backlight_is_flashing = 0;    //set this to the number of flashes (off/on cycles) to perform


#define STATUS_LED_FLASH_MILLIS_FAST 100
#define STATUS_LED_FLASH_MILLIS_SLOW 200
uint8_t g_status_led_flashing_millis = 0;     //set to the millis to flash at

void status_led_on()
{
#if defined(USE_SERIAL_DEBUG)
  Serial.println("Status LED ON");
#elif defined(USE_STATUS_LED_PB2)
  digitalWrite(PIN_STATUS_LED, HIGH);   //turn status LED on
  g_status_led_flashing_millis = 0;
#endif
}

void status_led_off()
{
#if defined(USE_SERIAL_DEBUG)
  Serial.println("Status LED OFF");
#elif defined(USE_STATUS_LED_PB2)
  digitalWrite(PIN_STATUS_LED, LOW);   //turn status LED off
  g_status_led_flashing_millis = 0;
#endif  
}

void status_led_flash_fast()
{
#if defined(USE_SERIAL_DEBUG)
  Serial.println("Status LED FLASHING FAST");
#endif
  g_status_led_flashing_millis = STATUS_LED_FLASH_MILLIS_FAST;
}

void status_led_flash_slow()
{
#if defined(USE_SERIAL_DEBUG)
  Serial.println("Status LED FLASHING SLOW");
#endif
  g_status_led_flashing_millis = STATUS_LED_FLASH_MILLIS_SLOW;
}

void status_led_update()
{
#if defined(USE_STATUS_LED_PB2)
  
  if(g_status_led_flashing_millis == 0)
    return;

  static unsigned long last_millis = 0;

  unsigned long curr_millis = millis();

  if((curr_millis - last_millis) >= g_status_led_flashing_millis)
  {
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));   //toggle status LED 

    last_millis = curr_millis;
  }
#endif
}

void status_led_init()
{
#if defined(USE_SERIAL_DEBUG)
  Serial.println("Status LED Initialize");
#elif defined(USE_STATUS_LED_PB2)
  pinMode(PIN_STATUS_LED, OUTPUT);
#endif  
}


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

bool i2c_address_is_in_range(byte address)
{
  if(address < 0x10)
    return false;

  if(address >= 0x70)
    return false;    

  return true;
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
    eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_PRIMARY_ADDR;

#ifdef CONFIG_I2C_DEFAULT_SECONDARY_ADDR    
    eeprom_data.sec_secondary_i2c_addr = CONFIG_I2C_DEFAULT_SECONDARY_ADDR;
#else
    eeprom_data.sec_secondary_i2c_addr = 0x00;
#endif

    eeprom_data.joy_adc_conf_bits = ADCS_AVAILABLE;



    struct joy_config0_bit_struct *config0_eeprom_ptr = (struct joy_config0_bit_struct *) &(eeprom_data.joy_config0);  //EEPROM version
    eeprom_data.joy_config0 = DEFAULT_CONFIG0;
    config0_eeprom_ptr->debounce_level = CONFIG_DEFAULT_DEBOUCE_LEVEL;
  }


#ifndef USE_SECONDARY_I2C_ADDR  
  //if there is NOT a secondary address, then always force the main joystick address to be CONFIG_I2C_DEFAULT_PRIMARY_ADDR (because there is no way to change it besides recompiling/uploading)
  eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_PRIMARY_ADDR;
#endif



  i2c_secondary_registers.config_backlight = eeprom_data.sec_config_backlight;

  if(i2c_address_is_in_range(eeprom_data.sec_joystick_i2c_addr) && (eeprom_data.sec_joystick_i2c_addr != eeprom_data.sec_secondary_i2c_addr))
    i2c_secondary_registers.joystick_i2c_addr = eeprom_data.sec_joystick_i2c_addr;
  else
    i2c_secondary_registers.joystick_i2c_addr = CONFIG_I2C_DEFAULT_PRIMARY_ADDR;
    
  if(i2c_address_is_in_range(eeprom_data.sec_secondary_i2c_addr) && (eeprom_data.sec_joystick_i2c_addr != eeprom_data.sec_secondary_i2c_addr))
    i2c_secondary_registers.secondary_i2c_addr = eeprom_data.sec_secondary_i2c_addr;
  else
    i2c_secondary_registers.secondary_i2c_addr = CONFIG_I2C_DEFAULT_SECONDARY_ADDR;


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
#ifdef USE_DIGITAL_BUTTON_DEBOUNCING
  debounce_setup();
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
  //PORTA_PIN1CTRL = PORT_PULLUPEN_bm;    //PA1 is an output (select for mux)
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

#define DEBOUNCE_PRESSED        0b00000000
#define DEBOUNCE_RELEASED       (g_debounce_mask)

uint8_t g_history_input0[8];
uint8_t g_history_input1[8];
uint8_t g_history_input2[8];

uint8_t g_debounce_mask = 0b11111111;


void debounce_setup()
{

  if(joy_config0_ptr->debounce_level > DEBOUNCE_LEVEL_MAX)
    joy_config0_ptr->debounce_level = DEBOUNCE_LEVEL_MAX;

  g_debounce_mask = (0b11111111 >> (DEBOUNCE_LEVEL_MAX - joy_config0_ptr->debounce_level));   //turn on CONFIG_DEBOUNCING_HISTORY_BITS number of bits


  //i2c_secondary_registers.rfu0 = g_debounce_mask;   //testing!
}


void debounce_inputs(uint8_t *input0, uint8_t *input1, uint8_t *input2)
{
  static uint8_t prev_input0 = 0xFF;
  static uint8_t prev_input1 = 0xFF;
  static uint8_t prev_input2 = 0xFF;
  uint8_t i;

  
  if(joy_config0_ptr->debounce_level == 0)
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




    //using concepts from https://hackaday.com/2015/12/10/embed-with-elliot-debounce-your-noisy-buttons-part-ii/ 

    if((g_history_input0[i] & g_debounce_mask) == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input0 = *input0 | (1 << i);
    }
    else if((g_history_input0[i] & g_debounce_mask) == DEBOUNCE_PRESSED)   //is button DOWN?
    {
      *input0 = *input0 & ~(1 << i);
    }
    else
    {
      //set it to whatever it was last time
      *input0 = (*input0 & ~(1 << i)) | (prev_input0 & (1 << i));
    }
    

    if((g_history_input1[i] & g_debounce_mask) == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input1 = *input1 | (1 << i);
    }
    else if((g_history_input1[i] & g_debounce_mask) == DEBOUNCE_PRESSED)   //is button DOWN?
    {
      *input1 = *input1 & ~(1 << i);
    }
    else
    {
      //set it to whatever it was last time
      *input1 = (*input1 & ~(1 << i)) | (prev_input1 & (1 << i));
    }


    if((g_history_input2[i] & g_debounce_mask) == DEBOUNCE_RELEASED)   //is button UP?
    {
      *input2 = *input2 | (1 << i);
    }
    else if((g_history_input2[i] & g_debounce_mask) == DEBOUNCE_PRESSED)   //is button DOWN?
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

  static uint16_t static_power_button_held = 0;

    

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

#if defined(USE_STATUS_LED_PB2)
  input2 |= (1 << 2);
#endif

  //the low nibble of adc_conf_bits bits are 1 if the ADC is turned on.
  input2 |= (i2c_joystick_registers.adc_conf_bits & 0xF0) & (i2c_joystick_registers.adc_conf_bits << 4);  //set all BTN_0, BTN_1, BTN_2, BTN_3 to 1 for any input that has an ADC turned on (can't "press" those buttons)

#ifdef USE_DIGITAL_BUTTON_DEBOUNCING
  debounce_inputs(&input0, &input1, &input2);
#endif

  g_power_button_pressed = IS_PRESSED_BTN_POWER_INPUT1(input1);


#ifdef USE_HOTKEY_TOGGLE_MODE
  switch(g_hotkey_mode)
  {
    case HOTKEY_SYSTEM_STARTUP:
      if(!g_power_button_pressed)
        g_hotkey_mode = HOTKEY_OFF;
      break;
    case HOTKEY_JUST_ENTERING:
      if(!g_power_button_pressed)
      {
        //now, we are fully IN hotkey mode
        g_hotkey_mode = HOTKEY_ON;
        static_power_button_held = 0;
        status_led_on();    
      }
      static_power_button_held++;
      if(static_power_button_held >= 0x600)
      {
        g_hotkey_mode = HOTKEY_SPECIAL_INPUT_ENTERING;    //user is holding the power to enter a different mode
        backlight_start_flashing(2);
        static_power_button_held = 0;
      }
      g_hotkey_input0 = input0;
      g_hotkey_input1 = input1;
      g_hotkey_input2 = input2;
      break;
      
    case HOTKEY_ON:
      byte x_input0, x_input1, x_input2;
      x_input0 = (input0 ^ g_hotkey_input0);
      x_input1 = (input1 ^ g_hotkey_input1);
      x_input2 = (input2 ^ g_hotkey_input2);

      if(x_input0 || x_input1 || x_input2)   //when we press a hotkey combo button that should be passed to the host
      {
        //leave hotkey mode by reporting just the power button
        g_hotkey_mode = HOTKEY_JUST_EXITING;

        g_hotkey_input0 = ~x_input0;    //these become a mask for the hotkey combo
        g_hotkey_input1 = ~x_input1;    //these become a mask for the hotkey combo
        g_hotkey_input2 = ~x_input2;    //these become a mask for the hotkey combo
 
        i2c_joystick_registers.input0 = input0 & g_hotkey_input0;
        i2c_joystick_registers.input1 = input1 & ~INPUT1_BTN_POWER & g_hotkey_input1;//make sure the power button is "pressed" while exiting hotkey mode
        i2c_joystick_registers.input2 = input2 & g_hotkey_input2;
      }
      else
      {    
        g_hotkey_input0 = input0;
        g_hotkey_input1 = input1;
        g_hotkey_input2 = input2;
      }
      break;
      
    case HOTKEY_JUST_EXITING:
      if(!((input0 & 0xFC) != 0xFC || (input1 & 0xFD) != 0xFD))   //when we release the hotkey combo button that should be passed to the host
      {
        //leave hotkey mode by reporting just the power button
        g_hotkey_mode = HOTKEY_OFF;
        status_led_off();    
        
        i2c_joystick_registers.input0 = input0;
        i2c_joystick_registers.input1 = input1;
        i2c_joystick_registers.input2 = input2;
      }
      else
      {
        i2c_joystick_registers.input0 = input0 & g_hotkey_input0;
        i2c_joystick_registers.input1 = input1 & ~INPUT1_BTN_POWER & g_hotkey_input1;//make sure the power button is "pressed" while exiting hotkey mode
        i2c_joystick_registers.input2 = input2 & g_hotkey_input2;
      }
      break;
      
    case HOTKEY_OFF:
    default:
      if(g_power_button_pressed)
      {
        //the power button was just pressed not in hotkey mode
        g_hotkey_mode = HOTKEY_JUST_ENTERING;
        //if we're chaging to "hotkey mode" then don't report the inputs to the host
        g_hotkey_input0 = input0;
        g_hotkey_input1 = input1;
        g_hotkey_input2 = input2;
        return;        
      }
      i2c_joystick_registers.input0 = input0;
      i2c_joystick_registers.input1 = input1;
      i2c_joystick_registers.input2 = input2;
      break;

    case HOTKEY_SPECIAL_INPUT_ENTERING:
      status_led_flash_fast();    //will get repeatedly called while power button held
      if(!g_power_button_pressed)
      {
        g_hotkey_mode = HOTKEY_SPECIAL_INPUT;
      }
      g_hotkey_input0 = input0;
      g_hotkey_input1 = input1;
      g_hotkey_input2 = input2;

      //in special input mode, report as if no buttons are pressed
      i2c_joystick_registers.input0 = 0xFF;
      i2c_joystick_registers.input1 = 0xFF;
      i2c_joystick_registers.input2 = 0xFF;      
      break;

    case HOTKEY_SPECIAL_INPUT:
      if(g_power_button_pressed)
      {
        backlight_start_flashing(2);
        g_hotkey_mode = HOTKEY_SPECIAL_INPUT_EXITING;
      }
      g_hotkey_input0 = input0;
      g_hotkey_input1 = input1;
      g_hotkey_input2 = input2;
      break;
      
    case HOTKEY_SPECIAL_INPUT_EXITING:
      if(!g_power_button_pressed)
      {
        g_hotkey_mode = HOTKEY_OFF;
        status_led_off();
      }
      g_hotkey_input0 = input0;
      g_hotkey_input1 = input1;
      g_hotkey_input2 = input2;
      break;
  }
#else //!defined(USE_HOTKEY_TOGGLE_MODE)
  i2c_joystick_registers.input0 = input0;
  i2c_joystick_registers.input1 = input1;
  i2c_joystick_registers.input2 = input2;
#endif  
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
  if(IS_SPECIAL_INPUT_MODE() && (IS_PRESSED_SPECIAL_INPUT_DPAD_UP() || IS_PRESSED_SPECIAL_INPUT_DPAD_DOWN()))
  {
    if(special_inputs_loop_counter)   
    {
      //we're delay looping
      special_inputs_loop_counter--;
    }
    else
    {
      special_inputs_loop_counter = SPECIAL_LOOP_DELAY;

      if(IS_PRESSED_SPECIAL_INPUT_DPAD_UP())
      {
        if(i2c_secondary_registers.config_backlight < (NUM_BACKLIGHT_PWM_STEPS-1))
        {
          i2c_secondary_registers.config_backlight++;
          //Serial.println("Backlight +");
        }
      }
      else if(IS_PRESSED_SPECIAL_INPUT_DPAD_DOWN())
      {
        if(i2c_secondary_registers.config_backlight > 0)
        {
          i2c_secondary_registers.config_backlight--;
          //Serial.println("Backlight -");
        }
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
  
  if(g_power_button_pressed)
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
    byte temp = Wire.read(); //We might use it, we might throw it away

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
    else if(x == REGISTER_CONFIG_BITS)   //this is a writeable register
    {
      i2c_joystick_registers.config0 = temp;
      eeprom_data.joy_config0 = i2c_joystick_registers.config0;
      setup_config0();
      eeprom_save_deferred();
    }
  }
}

#ifdef USE_SECONDARY_I2C_ADDR  
//this is the function that receives bytes from the i2c master for the SECONDARY
// i2c address (which is for the joystick functionality)
inline void receive_i2c_callback_secondary_address(int i2c_bytes_received)
{
  //g_i2c_command_index %= sizeof(i2c_secondary_registers);
  
  //what do we do when we receive bytes on the 2nd address?
  for (byte x = g_i2c_command_index ; x < (g_i2c_command_index + i2c_bytes_received - 1) ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away


    if(x == REGISTER_SEC_JOY_I2C_ADDR && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)
    {
      if(i2c_address_is_in_range(temp) && temp != i2c_secondary_registers.secondary_i2c_addr)
        i2c_secondary_registers.joystick_i2c_addr = temp;      
    }
    else if(x == REGISTER_SEC_WRITE_PROTECT)
    {
      if(temp == WRITE_PROTECT || temp == WRITE_UNPROTECT)
        i2c_secondary_registers.write_protect = temp;      
    }
    else if(x == REGISTER_SEC_SEC_I2C_ADDR && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)
    {
      if(i2c_address_is_in_range(temp) && temp != i2c_secondary_registers.joystick_i2c_addr)
        i2c_secondary_registers.secondary_i2c_addr = temp;      
    }    
    else if(x == REGISTER_SEC_POWER_CONTROL && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)
    {
      //backlight_start_flashing(temp);     //change this at some point!

      //power_control bit 0 indicates LOW_BATT status (true/false)
      //power_control bits 1-7 are currently unused
      
      //we would use this as a way for the i2c master (host system) to tell us about power related stuff (like if the battery is getting low)
      i2c_secondary_registers.power_control = temp & 0x01;    //only bit 0x01 is currently used
    }
    else if(x == REGISTER_SEC_BATTERY_CAPACITY && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)
    {
      i2c_secondary_registers.battery_capacity = temp;
    }
    else if(x == REGISTER_SEC_STATUS_LED && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)
    {
      //this one is write-only!  reading will always return 0x00
      switch(temp)
      {
        default:
        case 0:
          status_led_off();
          break;
        case 1:
          status_led_on();
          break;
        case 2:
          status_led_flash_slow();
          break;
        case 3:
          status_led_flash_fast();
          break;
      }
    }
#ifdef USE_PWM_BACKLIGHT
    else if(x == REGISTER_SEC_CONFIG_BACKLIGHT && i2c_secondary_registers.write_protect == WRITE_UNPROTECT)   //this is a writeable register
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

  //This is where we could schedule the system to update all the analog registers (sample ADCs), since they don't generate interrupts
  if(i2c_joystick_registers.adc_conf_bits & 0x0F)     //if any ADCs are turned on
    g_read_analog_inputs_asap = true;                 //schedule a read ASAP
  
#ifdef USE_SECONDARY_I2C_ADDR  
  g_i2c_address = Wire.getIncomingAddress() >> 1;

  if(g_i2c_address == i2c_secondary_registers.secondary_i2c_addr)       //secondary address
    receive_i2c_callback_secondary_address(i2c_bytes_received);
  else if(g_i2c_address == i2c_secondary_registers.joystick_i2c_addr)   //primary address
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
    //if the index was 0, just assume the host read at least 3 bytes.  I don't know how to tell
    //could use getBytesRead combined with g_i2c_command_index
    g_last_sent_input0 = i2c_joystick_registers.input0;
    g_last_sent_input1 = i2c_joystick_registers.input1;
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
  else if(g_i2c_index_to_read == 1)
  {
    //if the index was 1, just assume the host read at least 2 bytes.  I don't know how to tell
    g_last_sent_input1 = i2c_joystick_registers.input1;
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
  else if(g_i2c_index_to_read == 2)
  {
    //if the index was 2, just assume the host read at least 1 bytes.  I don't know how to tell
    g_last_sent_input2 = i2c_joystick_registers.input2;
  }
}

#ifdef USE_SECONDARY_I2C_ADDR  
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

#ifdef USE_SECONDARY_I2C_ADDR  
  if(g_i2c_address == CONFIG_I2C_DEFAULT_SECONDARY_ADDR)
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

#ifdef CONFIG_I2C_DEFAULT_SECONDARY_ADDR
  Wire.begin(i2c_secondary_registers.joystick_i2c_addr, 0, (i2c_secondary_registers.secondary_i2c_addr << 1) | 0x01);
#else
  Wire.begin(i2c_secondary_registers.joystick_i2c_addr); //Start I2C and answer calls using address from EEPROM
#endif

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receive_i2c_callback);
  Wire.onRequest(request_i2c_callback);
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

  i2c_secondary_registers.battery_capacity = 255;  //255 = battery capacity is unknown

  i2c_secondary_registers.power_control = 0;
  i2c_secondary_registers.write_protect = WRITE_PROTECT;

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


  status_led_init();  


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

#ifdef USE_SLEEP_MODE
  set_sleep_mode(SLEEP_MODE_STANDBY);
  sleep_enable();
#endif
}


void loop() 
{
  static unsigned long timer_start_millis = millis();
  unsigned long current_millis;
  
  current_millis = millis();

  if(g_backlight_is_flashing)
    backlight_process_flashing();

  status_led_update();

#if defined(CONFIG_INPUT_READ_TIMER_MICROS) && (CONFIG_INPUT_READ_TIMER_MICROS > 0)
  static unsigned long timer_input_read_start_micros = micros();
  unsigned long current_micros = micros();
  if(current_micros - timer_input_read_start_micros >= CONFIG_INPUT_READ_TIMER_MICROS)
#endif
  {
    read_digital_inputs();
    process_special_inputs();     //make sure special inputs are digital (input0 and input1) only
    
#if defined(CONFIG_INPUT_READ_TIMER_MICROS) && (CONFIG_INPUT_READ_TIMER_MICROS > 0)
    timer_input_read_start_micros = current_micros;
#endif
  }

#if defined(USE_ADC0) || defined(USE_ADC1) || defined(USE_ADC2) || defined(USE_ADC3)
  //ADCs are now read only when there's a pending i2c read
  if(g_read_analog_inputs_asap)
  {
    read_analog_inputs();
    g_read_analog_inputs_asap = false;
  }
#endif  
  

#ifdef PIN_BACKLIGHT_PWM

  if(g_pwm_step != i2c_secondary_registers.config_backlight)
  {
    //output the duty cycle to the pwm pin (PA3?)
    if(i2c_secondary_registers.config_backlight >= NUM_BACKLIGHT_PWM_STEPS)
      i2c_secondary_registers.config_backlight = NUM_BACKLIGHT_PWM_STEPS-1;
    
    analogWrite(PIN_BACKLIGHT_PWM, backlight_pwm_steps[i2c_secondary_registers.config_backlight]);
    g_pwm_step = i2c_secondary_registers.config_backlight;

    if(i2c_secondary_registers.config_backlight == 0)   //if the user chose backlight totally OFF, then bootup at default next time
      eeprom_data.sec_config_backlight = CONFIG_BACKLIGHT_STEP_DEFAULT;
    else
      eeprom_data.sec_config_backlight = i2c_secondary_registers.config_backlight;
    eeprom_save_deferred();
  }
#endif

  if(eeprom_data.sec_joystick_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
  {
    if(i2c_address_is_in_range(i2c_secondary_registers.joystick_i2c_addr) && i2c_secondary_registers.secondary_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
    {
      eeprom_data.sec_joystick_i2c_addr = i2c_secondary_registers.joystick_i2c_addr;
      eeprom_save_now();    //save to EEPROM right away, becuase we'll reboot next
      
      resetViaSWR();    //reboot the chip to use new address
    }
    
    resetViaSWR();    //reboot the chip to use old address
  }

  if(eeprom_data.sec_secondary_i2c_addr != i2c_secondary_registers.secondary_i2c_addr)
  {
    if(i2c_address_is_in_range(i2c_secondary_registers.secondary_i2c_addr) && i2c_secondary_registers.secondary_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
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
  if(current_millis - timer_start_millis >= CONFIG_PERIODIC_TASK_TIMER_MILLIS)
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

#if defined(USE_SOFTI2CMASTER) && defined(USE_QWIIC_JOYSTICK_VIA_SOFTI2CMASTER_AS_ADC0_AND_ADC1)

#warning This is all just for testing, at the moment.  Reading analog values from https://learn.sparkfun.com/tutorials/qwiic-joystick-hookup-guide/all works!

  static unsigned long timer_qwiic_softi2c_read_start_micros = micros();
  unsigned long current_softi2c_micros = micros();
  if(current_micros - timer_qwiic_softi2c_read_start_micros >= CONFIG_INPUT_READ_TIMER_MICROS)
  {
    if (i2c_start_wait((SOFTI2C_SLAVE_7BITADDR << 1) | I2C_WRITE)) 
    {
      i2c_write(0x03);    //start of ADC data
      i2c_rep_start((SOFTI2C_SLAVE_7BITADDR << 1) | I2C_READ);
      
      word adc = ((i2c_read(false) << 8) | i2c_read(false)) >> 6;
      i2c_joystick_registers.a0_msb = adc >> 2;
      i2c_joystick_registers.a1a0_lsb = ((adc << 2) & 0x0F) | (i2c_joystick_registers.a1a0_lsb & 0xF0);
  
      adc = ((i2c_read(false) << 8) | i2c_read(false)) >> 6;
      i2c_joystick_registers.a1_msb = adc >> 2;
      i2c_joystick_registers.a1a0_lsb = ((adc << 2) & 0xF0) | (i2c_joystick_registers.a1a0_lsb & 0x0F);

      byte button = i2c_read(false);

      /*if(button)
        i2c_joystick_registers.input1 |= 1 << 2;
      else
        i2c_joystick_registers.input1 &= ~(1 << 2);      */
      
      i2c_stop();
      
      timer_qwiic_softi2c_read_start_micros = current_softi2c_micros;
    }
  }
#endif


#ifdef USE_SLEEP_MODE
#error I think sleep mode does not work!
  if(((i2c_joystick_registers.adc_conf_bits & 0x0F) == 0) || !g_read_analog_inputs_asap)
  {    
    static byte woke=0;
    sleep_mode(); //Stop everything and go to sleep. Wake up from Button interrupts.
    Serial.println(woke++);
  }
#endif
}
