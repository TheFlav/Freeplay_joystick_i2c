//Freeplay_joystick_i2c_attiny

/*
 * 
 * TODO:  Maybe Add poweroff control via SECONDARY i2c address
 * MAYBE: Add watchdog shutdown if no "petting" after so long
 * 
 */



#include <Wire.h>
#include <EEPROM.h>

#define MANUF_ID         0xED
#define DEVICE_ID        0x00
#define VERSION_NUMBER   0x08


// Firmware for the ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 817


#define CONFIG_I2C_DEFAULT_ADDR     0x30
#define CONFIG_I2C_2NDADDR          0x40  //0x30 wouldn't work

//#define CONFIG_SERIAL_DEBUG  //shares pins with L2/R2 IO1_4/IO1_5

#define USE_INTERRUPTS
//#define USE_PB4_RESISTOR_LADDER

//#define ADC_RESOLUTION 10
//#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips
#define ADC_RESOLUTION ADC_NATIVE_RESOLUTION
#define USE_ADC0     //or can be used as digital input on input2 if desired
#define USE_ADC1     //or can be used as digital input on input2 if desired
#define USE_ADC2     //or can be used as digital input on input2 if desired
#define USE_ADC3     //or can be used as digital input on input2 if desired



//Interrupts are only really useful for digital inputs, so the nINT_PIN will go low only for input0 and input1 changes
#define USE_INTERRUPTS

#ifdef USE_INTERRUPTS
 #define nINT_PIN 20                  //AKA PA3
 bool g_nINT_state = false;
#endif

#define PIN_POWEROFF_OUT        19    //AKA PA2
#define PIN_BACKLIGHT_PWM       9     //AKA PB2



#define CONFIG_INVERT_POWER_BUTTON      //power button is on PIN_PB5

#define NUM_BACKLIGHT_PWM_STEPS 11
uint8_t backlight_pwm_steps[NUM_BACKLIGHT_PWM_STEPS] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xD0, 0xFF};
#define CONFIG_BACKLIGHT_STEP_DEFAULT  4


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


#ifdef USE_PB4_RESISTOR_LADDER
 #define CONFIG0_USE_EXTENDED_INPUTS          (1<<7)
 #define CONFIG0_USE_PB4_RESISTOR_LADDER      (1<<6)
#else
 #define CONFIG0_USE_EXTENDED_INPUTS          (0<<7)
 #define CONFIG0_USE_PB4_RESISTOR_LADDER      (0<<6)
#endif

#define DEFAULT_CONFIG0     (CONFIG0_USE_EXTENDED_INPUTS | CONFIG0_USE_PB4_RESISTOR_LADDER)

struct i2c_joystick_register_struct 
{
  uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
  uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
  uint8_t a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
  uint8_t a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
  uint8_t a1a0_lsb;        // Reg: 0x04 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
  uint8_t a2_msb;          // Reg: 0x05 - ADC2 most significant 8 bits
  uint8_t a3_msb;          // Reg: 0x06 - ADC2 most significant 8 bits
  uint8_t a3a2_lsb;        // Reg: 0x07 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
  uint8_t input2;          // Reg: 0x08 - INPUT port 2 (extended digital buttons)     BTN_Z and BTN_C among other things
#define REGISTER_ADC_CONF_BITS 0x09   //this one is writeable
  uint8_t adc_conf_bits;   // Reg: 0x09 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                           //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                           //             (but can only turn ON ADCs that are turned on in the high nibble.)
#define REGISTER_CONFIG_BITS 0x0A   //this one is writeable
  uint8_t config0;         // Reg: 0x0A - config register (turn on/off PB4 resistor ladder)  //maybe allow PA4-7 to be digital inputs connected to input2  config0[7]=use_extended_inputs
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
#define REGISTER_SEC_POWEROFF_CTRL      0x02    //this one is writeable
  uint8_t poweroff_control;  // Reg: 0x02 - write a magic number here to turn off the system
  uint8_t features_available;// Reg: 0x03 - bit define if ADCs are available or interrups are in use, etc.
  uint8_t rfu0;              // Reg: 0x04 - reserved for future use (or device-specific use)
  uint8_t rfu1;              // Reg: 0x05 - reserved for future use (or device-specific use)
  uint8_t rfu2;              // Reg: 0x06 - reserved for future use (or device-specific use)
  uint8_t rfu3;              // Reg: 0x07 - reserved for future use (or device-specific use)
  uint8_t rfu4;              // Reg: 0x08 - reserved for future use (or device-specific use)
  uint8_t rfu5;              // Reg: 0x09 - reserved for future use (or device-specific use)
  uint8_t rfu6;              // Reg: 0x0A - reserved for future use (or device-specific use)
  uint8_t rfu7;              // Reg: 0x0B - reserved for future use (or device-specific use)
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
} eeprom_data;

volatile byte g_last_sent_input0 = 0xFF;
volatile byte g_last_sent_input1 = 0xFF;

volatile byte g_i2c_index_to_read = 0;
volatile byte g_i2c_command_index = 0; //Gets set when user writes an address. We then serve the spot the user requested.
#ifdef CONFIG_I2C_2NDADDR
 volatile byte g_i2c_address = 0;  //if we're using multiple i2c addresses, we need to know which one is in use
#endif

byte g_pwm_step = 0x00;  //100% on

/*
 * digital inputs
 * 
 * input0
 * 
 * PC0 = IO0_0 = BTN_X
 * PC1 = IO0_1 = BTN_Y
 * PC2 = IO0_2 = BTN_START
 * PC3 = IO0_3 = BTN_SELECT
 * PC4 = IO0_4 = BTN_L
 * PC5 = IO0_5 = BTN_R
 * PB6 = IO0_6 = BTN_A
 * PB7 = IO0_7 = BTN_B
 * 
 * input1       A18 means analog pin 18 (A7 = analog 7 aka PB4)
 * 
 * A18 = IO1_0 = UP
 * A18 = IO1_1 = DOWN
 * A18 = IO1_2 = LEFT
 * A18 = IO1_3 = RIGHT
 * PB3 = IO1_4 = BTN_L2
 * PB4 = IO1_5 = BTN_R2   //PB4 can be turned into A7 to do an analog resistor ladder if we need LeftCenterPress (aka BTN_C) and RightCenterPress (aka BTN_Z) buttons
 * PB5 = IO1_6 = BTN_POWER
 * --- = IO1_7 = always high
 * 
 * input2       EXTENDED DIGITAL INPUT REGISTER 
 * 
 * A7  = IO2_0 = BTN_C aka LeftCenterPress
 * A7  = IO2_1 = BTN_Z aka RightCenterPress
 * --- = IO2_2 = 
 * --- = IO2_3 = 
 * PA4 = IO2_4 = BTN_??
 * PA5 = IO2_5 = BTN_??
 * PA6 = IO2_6 = BTN_??
 * PA7 = IO2_7 = BTN_??
 * 
 * 
 * POWER_BUTTON (Hotkey AKA poweroff_in) NEEDS TO BE IN HERE SOMEWHERE SOMEHOW
 * 
 * 
 * 
 * 
 * PA2 =         POWEROFF_OUT
 * PA3 =         nINT OUT
 * PB2 =         PWM Backlight OUT
 * 
 */


#define INPUT0_BTN_X      (1 << 0)      //IO0_0
#define INPUT0_BTN_Y      (1 << 1)      //IO0_1
#define INPUT0_BTN_START  (1 << 2)      //IO0_2
#define INPUT0_BTN_SELECT (1 << 3)      //IO0_3
#define INPUT0_BTN_L      (1 << 4)      //IO0_4
#define INPUT0_BTN_R      (1 << 5)      //IO0_5
#define INPUT0_BTN_A      (1 << 6)      //IO0_6
#define INPUT0_BTN_B      (1 << 7)      //IO0_7

#define INPUT1_DPAD_UP    (1 << 0)      //IO1_0
#define INPUT1_DPAD_DOWN  (1 << 1)      //IO1_1
#define INPUT1_DPAD_LEFT  (1 << 2)      //IO1_2
#define INPUT1_DPAD_RIGHT (1 << 3)      //IO1_3
#define INPUT1_BTN_L2     (1 << 4)      //IO1_4
#define INPUT1_BTN_R2     (1 << 5)      //IO1_5
#define INPUT1_BTN_POWER  (1 << 6)      //IO1_6
#define INPUT1_BTN_C      (1 << 7)      //IO1_7


#define PINB_POWER_BUTTON (1 << 5)    //PB5 is the power button, but it needs to be inverted (high when pressed)
                                        

//PRESSED means that the bit is 0
#define IS_PRESSED_DPAD_UP() ((i2c_joystick_registers.input1 & INPUT1_DPAD_UP) != INPUT1_DPAD_UP)
#define IS_PRESSED_DPAD_DOWN() ((i2c_joystick_registers.input1 & INPUT1_DPAD_DOWN) != INPUT1_DPAD_DOWN)

#define IS_PRESSED_BTN_A() ((i2c_joystick_registers.input0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define IS_PRESSED_BTN_B() ((i2c_joystick_registers.input0 & INPUT0_BTN_B) != INPUT0_BTN_B)

#define IS_PRESSED_BTN_L() ((i2c_joystick_registers.input0 & INPUT0_BTN_L) != INPUT0_BTN_L)
#define IS_PRESSED_BTN_R() ((i2c_joystick_registers.input0 & INPUT0_BTN_R) != INPUT0_BTN_R)

#define IS_PRESSED_BTN_POWER() ((i2c_joystick_registers.input1 & INPUT1_BTN_POWER) != INPUT1_BTN_POWER)

#define IS_SPECIAL_INPUT_MODE() (IS_PRESSED_BTN_A() && IS_PRESSED_BTN_B() && IS_PRESSED_BTN_L() && IS_PRESSED_BTN_R())


#ifdef CONFIG_SERIAL_DEBUG
 #define PINB_UART_MASK     (0b00001100)
#else
 #define PINB_UART_MASK     (0b00000000)
#endif


#define PINB_IN0_MASK      (0b11000000)   //the pins from PINB that are used in IN0
#define PINC_IN0_MASK      (0b00111111)   //the pins from PINC that are used in IN0

#define PINB_IN1_MASK      (0b00111000)   //the pins from PINB that are used in IN1

#define PINB_IN1_SHL       1

#define PINB_GPIO_MASK     ((PINB_IN0_MASK | PINB_IN1_MASK) & ~PINB_UART_MASK)
#define PINC_GPIO_MASK     (PINC_IN0_MASK)

#define PINA_IN2_MASK      (0b11110000)   //the pins from PINA that are used in IN2 extended input register

#define PINA_ADC_MASK      (0b11110010)   //the pins in port A used for ADC

void resetViaSWR() {
  _PROTECTED_WRITE(RSTCTRL.SWRR,1);
}

void eeprom_save()
{
  uint8_t *eeprom_data_ptr = (uint8_t *) &eeprom_data;
  uint8_t i;

  for(i=0;i<sizeof(eeprom_data);i++)
  {
    EEPROM.update(i, *eeprom_data_ptr++);
  }
}


void eeprom_restore_data()
{
  uint8_t *eeprom_data_ptr = (uint8_t *)&eeprom_data;
  uint8_t i;

  for(i=0;i<sizeof(eeprom_data);i++)
  {
    *eeprom_data_ptr = EEPROM.read(i);
    eeprom_data_ptr++;
  }

  
  if(!(eeprom_data.manuf_ID == MANUF_ID && eeprom_data.device_ID == DEVICE_ID && eeprom_data.version_ID == VERSION_NUMBER))
  {
    //load defaults
    eeprom_data.manuf_ID = MANUF_ID;
    eeprom_data.device_ID = DEVICE_ID;
    eeprom_data.version_ID = VERSION_NUMBER;

    eeprom_data.sec_config_backlight = CONFIG_BACKLIGHT_STEP_DEFAULT;
    eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_ADDR;

    eeprom_data.joy_adc_conf_bits = ADCS_AVAILABLE;
    eeprom_data.joy_config0 = DEFAULT_CONFIG0;
  }


#ifndef CONFIG_I2C_2NDADDR  
  eeprom_data.sec_joystick_i2c_addr = CONFIG_I2C_DEFAULT_ADDR;
#endif



  i2c_secondary_registers.config_backlight = eeprom_data.sec_config_backlight;
  i2c_secondary_registers.joystick_i2c_addr = eeprom_data.sec_joystick_i2c_addr;


  if((eeprom_data.joy_adc_conf_bits & 0xF0) != ADCS_AVAILABLE)
  {
    eeprom_data.joy_adc_conf_bits = ADCS_AVAILABLE;    //keep the ADCs turned off in this odd case that shouldn't really happen (except maybe during testing)
  }
  i2c_joystick_registers.adc_conf_bits = eeprom_data.joy_adc_conf_bits;


    
  i2c_joystick_registers.config0 = eeprom_data.joy_config0;    
}

void setup_gpio(void)
{
  //set as inputs

  PORTA_DIRCLR = PINA_ADC_MASK;
  PORTB_DIRCLR = PINB_GPIO_MASK;
  PORTC_DIRCLR = PINC_GPIO_MASK;

  //set pullups

//#define PINA_MASK (0b00001110) for ADC
  //PORTA_PIN0CTRL = PORT_PULLUPEN_bm;    //PA0 is UPDI
  //PORTA_PIN1CTRL = PORT_PULLUPEN_bm;    //PA1 is used for analog input (DPAD)
  //PORTA_PIN2CTRL = PORT_PULLUPEN_bm;    //PA2 = Poweroff Out _OUTPUT_
  //PORTA_PIN3CTRL = PORT_PULLUPEN_bm;    //PA3 = nINT _OUTPUT_
#ifndef USE_ADC0
  PORTA_PIN4CTRL = PORT_PULLUPEN_bm; //ADC0   (can be used as digital input on input2 if desired)
#endif
#ifndef USE_ADC1
  PORTA_PIN5CTRL = PORT_PULLUPEN_bm; //ADC1   (can be used as digital input on input2 if desired)
#endif
#ifndef USE_ADC2
  PORTA_PIN6CTRL = PORT_PULLUPEN_bm; //ADC2   (can be used as digital input on input2 if desired)
#endif
#ifndef USE_ADC3
  PORTA_PIN7CTRL = PORT_PULLUPEN_bm; //ADC3   (can be used as digital input on input2 if desired)
#endif
  
  //PORTB_PIN0CTRL = PORT_PULLUPEN_bm;    //i2c
  //PORTB_PIN1CTRL = PORT_PULLUPEN_bm;    //i2c
#ifndef CONFIG_SERIAL_DEBUG
  //PORTB_PIN2CTRL = PORT_PULLUPEN_bm;    //PB2 is PWM backlight _OUTPUT_
  PORTB_PIN3CTRL = PORT_PULLUPEN_bm;
#endif
  //PORTB_PIN4CTRL = PORT_PULLUPEN_bm;      //PB4 will have its own 200k external pull-up 
#if !defined(CONFIG_INVERT_POWER_BUTTON)    //don't use a pullup on the power button
  PORTB_PIN5CTRL = PORT_PULLUPEN_bm;    //PB5 = BTN_POWER
#endif
  PORTB_PIN6CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN7CTRL = PORT_PULLUPEN_bm;

//#define PINC_MASK (0b00111111)
  PORTC_PIN0CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN1CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN3CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN4CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN5CTRL = PORT_PULLUPEN_bm;
  //PORTC_PIN6CTRL = PORT_PULLUPEN_bm;    //there is no PC6
  //PORTC_PIN7CTRL = PORT_PULLUPEN_bm;    //there is no PC7
}


/*
 * This function is getting out of hand.  We might have to build some macros to do it.
 * But, it's efficient.
 */
void read_all_gpio(void)
{
  //we may want to turn off interrupts during this function
  //at the least, we need to poll the registers PA, PB, PC once
  //then set up the input registers as TEMPORARY registers
  //then put them in the actual i2c_joystick_registers 
  //otherwise, if we get interrupted during the setup, things get out of whack

  uint8_t rldu = 0b1111;
  uint8_t input0, input1;
  uint8_t pb_in = PORTB_IN;
  uint8_t pc_in = PORTC_IN;

#ifdef CONFIG_INVERT_POWER_BUTTON
  pb_in ^= PINB_POWER_BUTTON;
#endif


  uint16_t adc18 = analogRead(18);

  //NOTE:  This code will not let you press Up&Down or Left&Right at the same time

  if(adc18 < 65)
  {
    //no dpad pressed
    rldu = 0b1111;
  }
  else if(adc18 < 131)
  {
    //down
    rldu = 0b1101;
  }
  else if(adc18 < 201)
  {
    //right
    rldu = 0b0111;
  }
  else if(adc18 < 260)
  {
    //down and to the right
    rldu = 0b0101;
  }  
  else if(adc18 < 313)
  {
    //left
    rldu = 0b1011;
  }  
  else if(adc18 < 398)
  {
    //down and to the left
    rldu = 0b1001;
  }  
  else if(adc18 < 536)
  {
    //up
    rldu = 0b1110;
  }    
  else if(adc18 < 578)
  {
    //up and to the right
    rldu = 0b0110;
  }
  else
  {
    //up and to the left
    rldu = 0b1010;
  }

  input0 = (pc_in & PINC_IN0_MASK) | (pb_in & PINB_IN0_MASK);

#ifndef CONFIG_SERIAL_DEBUG
  input1 = ((pb_in & PINB_IN1_MASK) << PINB_IN1_SHL) | rldu;
#else
  input1 = ((pb_in & (PINB_IN1_MASK | PINB_UART_MASK)) << PINB_IN1_SHL) | rldu);   //act like the PINB_UART_MASK pins are never pressed
#endif

#ifdef USE_INTERRUPTS
  input1 |=  (1<<7);
#endif
  





  i2c_joystick_registers.input0 = input0;
  i2c_joystick_registers.input1 = input1;



  if(i2c_joystick_registers.config0 & CONFIG0_USE_EXTENDED_INPUTS)
  {
    uint8_t zc = 0b11;
    uint8_t input2;
    uint8_t pa_in = PORTA_IN;

/*
 * A7  = IO2_0 = BTN_C aka LeftCenterPress
 * A7  = IO2_1 = BTN_Z aka RightCenterPress
 * --- = IO2_2 = 
 * --- = IO2_3 = 
 * PA4 = IO2_4 = BTN_??
 * PA5 = IO2_5 = BTN_??
 * PA6 = IO2_6 = BTN_??
 * PA7 = IO2_7 = BTN_??
 */

#ifdef USE_PB4_RESISTOR_LADDER
    #error PB4 resistor ladder not configured
#endif
 
    input2 = (pa_in & PINA_IN2_MASK) | zc;

    i2c_joystick_registers.input2 = input2;
  }
}

#define SPECIAL_LOOP_DELAY 0x3FFF
#define POWEROFF_HOLD_SECONDS 20

void process_special_inputs()
{  
  static uint16_t special_inputs_loop_counter = 0;
  //static uint32_t power_button_loop_counter = POWER_LOOP_DELAY;
  //static bool power_btn_prev_state = false;
  static unsigned long power_btn_start_millis = 0;
  

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

#ifdef PIN_POWEROFF_OUT
  if(IS_PRESSED_BTN_POWER())
  {
    unsigned long current_millis = millis();
    if((current_millis - power_btn_start_millis) >= (POWEROFF_HOLD_SECONDS * 1000))
    {
      digitalWrite(PIN_POWEROFF_OUT,HIGH);
    }
  }
  else
  {
    power_btn_start_millis = millis();
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
      i2c_joystick_registers.adc_conf_bits = i2c_joystick_registers.adc_conf_bits | (temp & mask);     //turn on any bits that are available and requested
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

    if(x == REGISTER_SEC_CONFIG_BACKLIGHT)   //this is a writeable register
    {
      //set backlight value
      if(temp >= 0 && temp < NUM_BACKLIGHT_PWM_STEPS)
        i2c_secondary_registers.config_backlight = temp;      
    }

    if(x == REGISTER_SEC_JOY_I2C_ADDR)
    {
      i2c_secondary_registers.joystick_i2c_addr = temp;      
    }
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
  }
  
  if(g_i2c_index_to_read == 1)
    g_last_sent_input1 = i2c_joystick_registers.input1;
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
  Wire.begin(i2c_secondary_registers.joystick_i2c_addr, 0, (CONFIG_I2C_2NDADDR << 1) | 0x01);
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
  i2c_joystick_registers.manuf_ID = VERSION_NUMBER;

  i2c_secondary_registers.manuf_ID = MANUF_ID;
  i2c_secondary_registers.device_ID = DEVICE_ID;
  i2c_secondary_registers.manuf_ID = VERSION_NUMBER;

  i2c_secondary_registers.poweroff_control = 0;

  i2c_secondary_registers.features_available = FEATURES_AVAILABLE;
  
  i2c_joystick_registers.adc_conf_bits = ADCS_AVAILABLE;


  i2c_secondary_registers.backlight_max = NUM_BACKLIGHT_PWM_STEPS-1;

  eeprom_restore_data();      //should be done beore startI2C()
  
  
  
#ifdef nINT_PIN
  pinMode(nINT_PIN, OUTPUT);
  digitalWrite(nINT_PIN, g_nINT_state);
#endif 


#ifdef CONFIG_SERIAL_DEBUG
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


  
  
  g_pwm_step = ~i2c_secondary_registers.config_backlight; //unset it

  setup_gpio();

  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
}


void loop() 
{
  
  word adc;

#ifdef CONFIG_SERIAL_DEBUG
  Serial.print("g_i2c_address=");
  Serial.print(g_i2c_address);
  Serial.print(" g_i2c_command_index=");
  Serial.print(g_i2c_command_index);
  Serial.print(" g_i2c_index_to_read=");
  Serial.print(g_i2c_index_to_read);

  Serial.println();
#endif

  read_all_gpio();
  process_special_inputs();

#ifdef PIN_BACKLIGHT_PWM

  if(g_pwm_step != i2c_secondary_registers.config_backlight)
  {
    //output the duty cycle to the pwm pin (PA3?)
    if(i2c_secondary_registers.config_backlight >= NUM_BACKLIGHT_PWM_STEPS)
      i2c_secondary_registers.config_backlight = NUM_BACKLIGHT_PWM_STEPS-1;
    
    analogWrite(PIN_BACKLIGHT_PWM, backlight_pwm_steps[i2c_secondary_registers.config_backlight]);
    g_pwm_step = i2c_secondary_registers.config_backlight;

    eeprom_data.sec_config_backlight = i2c_secondary_registers.config_backlight;
    eeprom_save();
  }
#endif

  if(eeprom_data.sec_joystick_i2c_addr != i2c_secondary_registers.joystick_i2c_addr)
  {
    if(i2c_secondary_registers.joystick_i2c_addr >= 0x10 && i2c_secondary_registers.joystick_i2c_addr < 0x70)
    {
      eeprom_data.sec_joystick_i2c_addr = i2c_secondary_registers.joystick_i2c_addr;
      eeprom_save();    //save to EEPROM
      
      resetViaSWR();    //reboot the chip to use new address
    }
  }
  

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

#ifdef nINT_PIN
  bool new_nINT = ((g_last_sent_input0 == i2c_joystick_registers.input0) && (g_last_sent_input1 == i2c_joystick_registers.input1));
  if(g_nINT_state != new_nINT)
  {
    digitalWrite(nINT_PIN, new_nINT);
    g_nINT_state = new_nINT;    
  }
#endif
}
