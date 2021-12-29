//Freeplay_joystick_i2c_attiny

/*
 * 
 * TODO:  Maybe Add poweroff control via SECONDARY i2c address
 * MAYBE: Add watchdog shutdown if no "petting" after so long
 * 
 */



#include <Wire.h>
#include <EEPROM.h>

#define VERSION_MAJOR   0
#define VERSION_MINOR   5


// Firmware for the ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 817


#define CONFIG_I2C_ADDR     0x20
#define CONFIG_I2C_2NDADDR  0x40  //0x30 wouldn't work

//#define CONFIG_SERIAL_DEBUG  //shares pins with L2/R2 IO1_6/IO1_7

//#define USE_INTERRUPTS
#define USE_ADC

#ifdef USE_ADC
 //#define ADC_RESOLUTION 10
 //#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips
 #define ADC_RESOLUTION ADC_NATIVE_RESOLUTION
 #define USE_ADC0
 #define USE_ADC1
 #define USE_ADC2
 #define USE_ADC3
 #define USE_ADC18
 #define USE_ADC19
#else
 #define ADC_RESOLUTION 0
#endif


#if defined(USE_ADC0) && defined(USE_INTERRUPTS)
 #error ADC0 and nINT_PIN share the same pin
#endif

//I feel like we only want to do interrupts if we're not doing ADCs, but this can change around
#if !defined(USE_ADC0) && defined(USE_INTERRUPTS)
 #define nINT_PIN 0 //also PIN_PA4
 bool g_nINT_state = false;
#endif


#define POWEROFF_OUT_PIN 8 //PIN_PB3  //pin num 8

#define CONFIG_INVERT_POWER_BUTTON      //power button is on PIN_PB3

#define NUM_BACKLIGHT_PWM_STEPS 11
uint8_t backlight_pwm_steps[NUM_BACKLIGHT_PWM_STEPS] = {0x00, 0x10, 0x20, 0x30, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xD0, 0xFF};
#define CONFIG_BACKLIGHT_STEP_DEFAULT  4



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
#define REGISTER_ADC_CONF_BITS 0x08
  uint8_t adc_conf_bits;   // Reg: 0x08 - High Nibble is read-only.  ADC PRESENT = It tells which ADCs are available.
                           //             Low Nibble is read/write.  ADC ON/OFF = The system can read/write what ADCs are sampled and used for a#_msb and lsb above
                           //             (but can only turn ON ADCs that are turned on in the high nibble.)
  uint8_t config0;         // Reg: 0x09 - Configuration port 0
  uint8_t adc_res;         // Reg: 0x0A - current ADC resolution (maybe settable?)
} volatile i2c_joystick_registers;

struct i2c_secondary_address_register_struct 
{
  uint8_t magic;             //set to some magic value (0xED), so we know this is the right chip we're talking to
  uint8_t ver_major;
  uint8_t ver_minor;
#define REGISTER_CONFIG_BACKLIGHT 0x03
  uint8_t config_backlight;  // Reg: 0x03
  uint8_t backlight_max;     // Reg: 0x04 
  uint8_t poweroff_control;  // Reg: 0x05  - write some magic number here to turn off the system
  uint8_t features_available;// Reg: 0x06  - bit define if ADCs are available or interrups are in use, etc. 
} i2c_secondary_registers;

volatile byte g_last_sent_input0 = 0xFF;
volatile byte g_last_sent_input1 = 0xFF;

volatile byte g_i2c_index_to_read = 0;
volatile byte g_i2c_command_index = 0; //Gets set when user writes an address. We then serve the spot the user requested.
#ifdef CONFIG_I2C_2NDADDR
 volatile byte g_i2c_address = 0;  //if we're using multiple i2c addresses, we need to know which one is in use
#endif

byte g_pwm_step = 0x00;  //100% on

/*
 * PA1 = IO0_0 = UP
 * PA2 = IO0_1 = DOWN
 * PB4 = IO0_2 = LEFT
 * PB5 = IO0_3 = RIGHT
 * PB6 = IO0_4 = BTN_A
 * PB7 = IO0_5 = BTN_B
 * PA6 = IO0_6 = BTN_L2  ifndef USE_ADC2
 * PA7 = IO0_7 = BTN_R2  ifndef USE_ADC3
 * 
 * PC0 = IO1_0 = BTN_X
 * PC1 = IO1_1 = BTN_Y
 * PC2 = IO1_2 = BTN_START
 * PC3 = IO1_3 = BTN_SELECT
 * PC4 = IO1_4 = BTN_L
 * PC5 = IO1_5 = BTN_R
 * PB2 = IO1_6 = POWER_BUTTON (Hotkey AKA poweroff_in)   ifndef CONFIG_SERIAL_DEBUG (or can be used for UART TXD0 for debugging)
 * PA5 = IO1_7 = BTN_C ifndef USE_ADC1
 * 
 * PB3 =         POWEROFF_OUT
 * PA3 =         PWM Backlight OUT
 * 
 */

#define INPUT0_DPAD_UP    (1 << 0)      //IO0_0
#define INPUT0_DPAD_DOWN  (1 << 1)      //IO0_1
#define INPUT0_BTN_A      (1 << 4)      //IO0_4
#define INPUT0_BTN_B      (1 << 5)      //IO0_5
#define INPUT1_BTN_L      (1 << 4)      //IO1_4
#define INPUT1_BTN_R      (1 << 5)      //IO1_5
#define INPUT1_BTN_POWER  (1 << 6)      //IO1_6



#define PINA_DPAD_UP    (1 << 1)      //PA1
#define PINA_DPAD_DOWN  (1 << 2)      //PA2
#define PINA_BTN_C      (1 << 5)      //PA5
#define PINA_BTN_L2     (1 << 6)      //PA6
#define PINA_BTN_R2     (1 << 7)      //PA7

#define PINB_BTN_PWR    (1 << 2)      //PB2
#define PINB_DPAD_LEFT  (1 << 4)      //PB4
#define PINB_DPAD_RIGHT (1 << 5)      //PB5
#define PINB_BTN_A      (1 << 6)      //PB6
#define PINB_BTN_B      (1 << 7)      //PB7

#define PINC_BTN_X      (1 << 0)      //PC0
#define PINC_BTN_Y      (1 << 1)      //PC1
#define PINC_BTN_START  (1 << 2)      //PC2
#define PINC_BTN_SELECT (1 << 3)      //PC3
#define PINC_BTN_L      (1 << 4)      //PC4
#define PINC_BTN_R      (1 << 5)      //PC5


//PRESSED means that the bit is 0
#define IS_PRESSED_PINB_BTN_A(pinb_test) ((pinb_test & PINB_BTN_A) != PINB_BTN_A)
#define IS_PRESSED_PINB_BTN_B(pinb_test) ((pinb_test & PINB_BTN_B) != PINB_BTN_B)

#define IS_PRESSED_PINC_BTN_L(pinc_test) ((pinc_test & PINC_BTN_L) != PINC_BTN_L)
#define IS_PRESSED_PINC_BTN_R(pinc_test) ((pinc_test & PINC_BTN_R) != PINC_BTN_R)

#define IS_PRESSED_PINA_DPAD_UP(pina_test) ((pina_test & PINA_DPAD_UP) != PINA_DPAD_UP)
#define IS_PRESSED_PINA_DPAD_DOWN(pina_test) ((pina_test & PINA_DPAD_DOWN) != PINA_DPAD_DOWN)


#define IS_PRESSED_DPAD_UP() ((i2c_joystick_registers.input0 & INPUT0_DPAD_UP) != INPUT0_DPAD_UP)
#define IS_PRESSED_DPAD_DOWN() ((i2c_joystick_registers.input0 & INPUT0_DPAD_DOWN) != INPUT0_DPAD_DOWN)

#define IS_PRESSED_BTN_A() ((i2c_joystick_registers.input0 & INPUT0_BTN_A) != INPUT0_BTN_A)
#define IS_PRESSED_BTN_B() ((i2c_joystick_registers.input0 & INPUT0_BTN_B) != INPUT0_BTN_B)

#define IS_PRESSED_BTN_L() ((i2c_joystick_registers.input1 & INPUT1_BTN_L) != INPUT1_BTN_L)
#define IS_PRESSED_BTN_R() ((i2c_joystick_registers.input1 & INPUT1_BTN_R) != INPUT1_BTN_R)

#define IS_PRESSED_BTN_POWER() ((i2c_joystick_registers.input1 & INPUT1_BTN_POWER) != INPUT1_BTN_POWER)

#define IS_SPECIAL_INPUT_MODE() (IS_PRESSED_BTN_A() && IS_PRESSED_BTN_B() && IS_PRESSED_BTN_L() && IS_PRESSED_BTN_R())




#define PIN_POWEROFF_OUT   1
#define PIN_PWM             20




//if you change any of these, then you need to change the pullups in setup()
//#define PINA_ADC0_MASK      (0b00010000)    //don't need this, because PA4 is never a digital BTN input (only nINT or ADC0)
#define PINA_ADC1_MASK      (0b00100000)
#define PINA_ADC2_MASK      (0b01000000)
#define PINA_ADC3_MASK      (0b10000000)

#ifdef CONFIG_SERIAL_DEBUG
 #define PINB_UART_MASK     (0b00001100)
#else
 #define PINB_UART_MASK     (0b00000000)
#endif

#define PINA_IN0_MASK      (0b00000110)   //the pins from PINA that are used in IN0     (not counting ADC pins)
#define PINB_IN0_MASK      (0b11110000)   //the pins from PINB that are used in IN0

#ifdef USE_ADC1
#define PINA_IN1_MASK      (0b00000000)   //the pins from PINA that are used in IN1
#else
#define PINA_IN1_MASK      (0b00100000)   //the pins from PINA that are used in IN1
#endif
#define PINB_IN1_MASK      (0b00000100)   //the pins from PINB that are used in IN1
#define PINC_IN1_MASK      (0b00111111)   //the pins from PINC that are used in IN1

#define PINA_GPIO_MASK     (PINA_IN0_MASK)
#define PINB_GPIO_MASK     ((PINB_IN0_MASK | PINB_IN1_MASK) & ~PINB_UART_MASK)
#define PINC_GPIO_MASK     (PINC_IN1_MASK)

#define PINB_POWER_BUTTON (1 << 2)    //PB2 is the power button, but it needs to be inverted (high when pressed)

#define PINA_IN0_SHR       1
#define PINB_IN0_SHR       2
#define PINB_IN1_SHL       4
#define PINA_IN1_SHL       2

void setup_gpio(void)
{
  //set as inputs

  PORTA_DIRCLR = PINA_GPIO_MASK | /*PINA_ADC0_MASK |*/ PINA_ADC1_MASK | PINA_ADC2_MASK | PINA_ADC3_MASK;

  PORTB_DIRCLR = PINB_GPIO_MASK;
  PORTC_DIRCLR = PINC_GPIO_MASK;

  //set pullups

//#define PINA_MASK (0b00001110) for ADC
  //PORTA_PIN0CTRL = PORT_PULLUPEN_bm;
#ifndef USE_ADC18
  PORTA_PIN1CTRL = PORT_PULLUPEN_bm;
#endif
#ifndef USE_ADC19
  PORTA_PIN2CTRL = PORT_PULLUPEN_bm;
#endif
  PORTA_PIN3CTRL = PORT_PULLUPEN_bm;
#ifndef USE_ADC0
  PORTA_PIN4CTRL = PORT_PULLUPEN_bm; //ADC0
#endif
#ifndef USE_ADC1
  PORTA_PIN5CTRL = PORT_PULLUPEN_bm; //ADC1
#endif
#ifndef USE_ADC2
  PORTA_PIN6CTRL = PORT_PULLUPEN_bm; //ADC2
#endif
#ifndef USE_ADC3
  PORTA_PIN7CTRL = PORT_PULLUPEN_bm; //ADC3
#endif
  
//#define PINB_MASK (0b01110000)
  //PORTB_PIN0CTRL = PORT_PULLUPEN_bm;
  //PORTB_PIN1CTRL = PORT_PULLUPEN_bm;
#ifndef CONFIG_SERIAL_DEBUG
 #if !defined(CONFIG_INVERT_POWER_BUTTON)    //don't use a pullup on the power button
  PORTB_PIN2CTRL = PORT_PULLUPEN_bm;
 #endif
  PORTB_PIN3CTRL = PORT_PULLUPEN_bm;
#endif
  PORTB_PIN4CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN5CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN6CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN7CTRL = PORT_PULLUPEN_bm;

//#define PINC_MASK (0b00111111)
  PORTC_PIN0CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN1CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN3CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN4CTRL = PORT_PULLUPEN_bm;
  PORTC_PIN5CTRL = PORT_PULLUPEN_bm;
  //PORTC_PIN6CTRL = PORT_PULLUPEN_bm;
  //PORTC_PIN7CTRL = PORT_PULLUPEN_bm;
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
  
  byte input0, input1;
  byte pa_in = PORTA_IN;
  byte pb_in = PORTB_IN;
  byte pc_in = PORTC_IN;

#ifdef CONFIG_INVERT_POWER_BUTTON
 pb_in ^= PINB_POWER_BUTTON;
#endif

  input0 = ((pa_in & PINA_IN0_MASK) >> PINA_IN0_SHR) | ((pb_in & PINB_IN0_MASK) >> PINB_IN0_SHR);

#ifndef CONFIG_SERIAL_DEBUG
  input1 = ((pb_in & PINB_IN1_MASK) << PINB_IN1_SHL) |  (pc_in & PINC_IN1_MASK) | ((pa_in & PINA_IN1_MASK) << PINA_IN1_SHL);
 #ifdef USE_ADC1
  input1 |= (PINA_ADC1_MASK << PINA_IN1_SHL);
 #endif  
#else
  input1 = ((pb_in & (PINB_IN1_MASK | PINB_UART_MASK)) << PINB_IN1_SHL) |  (pc_in & PINC_IN1_MASK) | ((pa_in & PINA_IN1_MASK) << PINA_IN1_SHL);   //act like the PINB_UART_MASK pins are never pressed
#endif

#if defined(USE_ADC2) && defined(USE_ADC3)
  input0 |= (PINA_ADC3_MASK | PINA_ADC2_MASK);
#elif !defined(USE_ADC2) && !defined(USE_ADC3)
  input0 |= (pa_in & (PINA_ADC2_MASK | PINA_ADC3_MASK));
#elif defined(USE_ADC3) && !defined(USE_ADC2)
  input0 |= (pa_in & PINA_ADC2_MASK) | PINA_ADC3_MASK;
#elif defined(USE_ADC2) && !defined(USE_ADC3)
  input0 |= (pa_in & PINA_ADC3_MASK) | PINA_ADC2_MASK;
#else
 #error How can this be?
#endif


#if defined(USE_ADC18) && defined(USE_ADC19)
#warning TESTING USE_ADC18/19

  uint16_t adc18 = analogRead(18);
  
  uint8_t rl = 0;
  uint8_t du = 0;

  if(adc18 > 682)
  {
    rl = 0b1000;
  }
  else if(adc18 > 341)
  {
    rl = 0b1100;
  }
  else
  {
    rl = 0b0100;
  }

  uint16_t adc19 = analogRead(19);
  if(adc19 > 682)
  {
    du = 0b0001;
  }
  else if(adc19 > 341)
  {
    du = 0b0011;
  }
  else
  {
    du = 0b0010;
  }  

  input0 &= 0b11110000;
  input0 |= rl | du;

#endif


  i2c_joystick_registers.input0 = input0;
  i2c_joystick_registers.input1 = input1;
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

  if(IS_PRESSED_BTN_POWER())
  {
    unsigned long current_millis = millis();
    if((current_millis - power_btn_start_millis) >= (POWEROFF_HOLD_SECONDS * 1000))
    {
      digitalWrite(POWEROFF_OUT_PIN,LOW);
    }
  }
  else
  {
    power_btn_start_millis = millis();
  }
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

    if(x == REGISTER_CONFIG_BACKLIGHT)   //this is a writeable register
    {
      //set backlight value
      if(temp >= 0 && temp < NUM_BACKLIGHT_PWM_STEPS)
        i2c_secondary_registers.config_backlight = temp;      
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
  Wire.begin(CONFIG_I2C_ADDR, 0, (CONFIG_I2C_2NDADDR << 1) | 0x01);
#else
  Wire.begin(CONFIG_I2C_ADDR); //Start I2C and answer calls using address from EEPROM
#endif

  //The connections to the interrupts are severed when a Wire.begin occurs. So re-declare them.
  Wire.onReceive(receive_i2c_callback);
  Wire.onRequest(request_i2c_callback);
}





void setup() 
{
  //memset(&i2c_joystick_registers, 0, sizeof(i2c_joystick_registers));
  
  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus
  setup_gpio();
  
  
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

  pinMode(PIN_PWM, OUTPUT);  // sets the pin as output
  
  pinMode(POWEROFF_OUT_PIN, OUTPUT);
  digitalWrite(POWEROFF_OUT_PIN, HIGH);

  i2c_joystick_registers.adc_res = ADC_RESOLUTION;

  //default to middle
  i2c_joystick_registers.a0_msb = 0x7F;
  i2c_joystick_registers.a1_msb = 0x7F;
  i2c_joystick_registers.a1a0_lsb = 0xFF;

  i2c_joystick_registers.a2_msb = 0x7F;
  i2c_joystick_registers.a3_msb = 0x7F;
  i2c_joystick_registers.a3a2_lsb = 0xFF;

  i2c_secondary_registers.magic = 0xED;
  i2c_secondary_registers.ver_major = VERSION_MAJOR;
  i2c_secondary_registers.ver_minor = VERSION_MINOR;

  if(EEPROM[0] == 0xED && EEPROM[1] == VERSION_MAJOR && EEPROM[2] == VERSION_MINOR)
  {
    i2c_secondary_registers.config_backlight = EEPROM[3];
  }
  else
  {
    EEPROM[0] = 0xED;
    EEPROM[1] = VERSION_MAJOR;
    EEPROM[2] = VERSION_MINOR;
    i2c_secondary_registers.config_backlight = CONFIG_BACKLIGHT_STEP_DEFAULT;
    EEPROM[3] = i2c_secondary_registers.config_backlight;
  }
  
  g_pwm_step = ~i2c_secondary_registers.config_backlight; //unset it
  i2c_secondary_registers.poweroff_control = 0;

  i2c_secondary_registers.features_available = 0;
  i2c_joystick_registers.adc_conf_bits = 0;

#ifdef USE_ADC0
  i2c_secondary_registers.features_available |= 1 << 0;
  i2c_joystick_registers.adc_conf_bits |= 1 << 4;   //lowest bit of the high nibble
#endif
#ifdef USE_ADC1
  i2c_secondary_registers.features_available |= 1 << 1;
  i2c_joystick_registers.adc_conf_bits |= 1 << 5;   //second bit of the high nibble
#endif
#ifdef USE_ADC2
  i2c_secondary_registers.features_available |= 1 << 2;
  i2c_joystick_registers.adc_conf_bits |= 1 << 6;   //third bit of the high nibble
#endif
#ifdef USE_ADC3
  i2c_secondary_registers.features_available |= 1 << 3;
  i2c_joystick_registers.adc_conf_bits |= 1 << 7;   //top bit of the high nibble
#endif
#ifdef USE_INTERRUPTS
  i2c_secondary_registers.features_available |= 1 << 4;
#endif

  i2c_secondary_registers.backlight_max = NUM_BACKLIGHT_PWM_STEPS-1;
}


void loop() 
{
  
#ifdef USE_ADC
  word adc;
#endif

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

#ifdef PIN_PWM

  if(g_pwm_step != i2c_secondary_registers.config_backlight)
  {
    //output the duty cycle to the pwm pin (PA3?)
    if(i2c_secondary_registers.config_backlight >= NUM_BACKLIGHT_PWM_STEPS)
      i2c_secondary_registers.config_backlight = NUM_BACKLIGHT_PWM_STEPS-1;
    
    analogWrite(PIN_PWM, backlight_pwm_steps[i2c_secondary_registers.config_backlight]);
    g_pwm_step = i2c_secondary_registers.config_backlight;

    EEPROM[3] = i2c_secondary_registers.config_backlight;
  }
#endif
  

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
