//Freeplay_joystick_i2c_attiny

/*
 * 
 * TODO:  Add poweroff_in, poweroff_out functionality
 *        Add poweroff control via SECONDARY i2c address
 * 
 * MAYBE: Add watchdog shutdown if no "petting" after so long
 */


#include <Wire.h>

#define VERSION_MAJOR   0
#define VERSION_MINOR   2

#define CONFIG_SPEED_PROFILING


// Firmware for the ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 817

#define ADC_RESOLUTION 10
//#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips

#define CONFIG_I2C_ADDR     0x20
#define CONFIG_I2C_2NDADDR  0x40  //0x30 wouldn't work

//#define CONFIG_SERIAL_DEBUG  //shares pins with L2/R2 IO1_6/IO1_7

#define USE_INTERRUPTS
//#define USE_ADC

// How many ADC's do you want?
#ifdef USE_ADC
 #define USE_ADC0
 #define USE_ADC1
 #define USE_ADC2
 #define USE_ADC3
#endif

#if defined(USE_ADC0) && defined(USE_INTERRUPTS)
 #error ADC0 and nINT_PIN share the same pin
#endif

#define CONFIG_INVERT_POWER_BUTTON



struct i2c_joystick_register_struct {
  byte input0;          // Reg: 0x00 - INPUT port 0
  byte input1;          // Reg: 0x01 - INPUT port 1
  byte a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
  byte a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
  byte a2_msb;          // Reg: 0x04 - ADC2 most significant 8 bits
  byte a3_msb;          // Reg: 0x05 - ADC3 most significant 8 bits
  byte a0_lsb;          // Reg: 0x06 - ADC0 least significant 8 bits
  byte a1_lsb;          // Reg: 0x07 - ADC1 least significant 8 bits
  byte a2_lsb;          // Reg: 0x08 - ADC2 least significant 8 bits
  byte a3_lsb;          // Reg: 0x09 - ADC3 least significant 8 bits
  byte adc_on_bits;     // Reg: 0x0A - turn ON bits here to activate ADC0 - ADC3 (only works if the USE_ADC# are turned on)
  byte config0;         // Reg: 0x0B - Configuration port 0
  byte adc_res;         // Reg: 0x0D - current ADC resolution (maybe settable?)
#ifdef CONFIG_SPEED_PROFILING  
  uint32_t num_loops;
#endif
} i2c_joystick_registers;

struct i2c_secondary_address_register_struct {
  byte magic;  //set to some magic value (0xED), so we know this is the right chip we're talking to
  byte ver_major;
  byte ver_minor;
  byte config_PWM;        // Reg: 0x03
  byte poweroff_control;  // Reg: 0x04  - write some magic number here to turn off the system
} i2c_secondary_registers;

volatile byte g_last_sent_input0 = 0xFF;
volatile byte g_last_sent_input1 = 0xFF;

volatile byte g_i2c_index_to_read = 0;
volatile byte g_i2c_command_index = 0; //Gets set when user writes an address. We then serve the spot the user requested.
#ifdef CONFIG_I2C_2NDADDR
 volatile byte g_i2c_address = 0;  //if we're using multiple i2c addresses, we need to know which one is in use
#endif

byte g_pwm_duty_cycle = 0x00;  //100% on

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



//I feel like we only want to do interrupts if we're not doing ADCs, but this can change around

#if !defined(USE_ADC0) && defined(USE_INTERRUPTS)
 #define nINT_PIN 0 //also PIN_PA4
 bool g_nINT_state = false;
#endif

#define PIN_POWEROFF_OUT   1
#define PIN_PWM             20




//if you change any of these, then you need to change the pullups in setup()
#define PINA_ADC0_MASK      (0b00010000)
#define PINA_ADC1_MASK      (0b00100000)
#define PINA_ADC2_MASK      (0b01000000)
#define PINA_ADC3_MASK      (0b10000000)

#ifdef CONFIG_SERIAL_DEBUG
 #define PINB_UART_MASK     (0b00001100)
#else
 #define PINB_UART_MASK     (0b00000000)
#endif

#define PINA_IN0_MASK      (0b00000110)   //the pins from PINA that are used in IN0
#define PINB_IN0_MASK      (0b11110000)   //the pins from PINB that are used in IN0

#define PINA_IN1_MASK      (0b00100000)   //the pins from PINA that are used in IN1
#define PINB_IN1_MASK      (0b00000100)   //the pins from PINB that are used in IN1
#define PINC_IN1_MASK      (0b00111111)   //the pins from PINC that are used in IN1

#define PINA_GPIO_MASK     (PINA_IN0_MASK)
#define PINB_GPIO_MASK     ((PINB_IN0_MASK | PINB_IN1_MASK) & ~PINB_UART_MASK)
#define PINC_GPIO_MASK     (PINC_IN1_MASK)

#ifdef CONFIG_INVERT_POWER_BUTTON
 #define PINB_INVERT_MASK (1 << 2)    //PB2 is the power button, but it needs to be inverted (high when pressed)
#endif

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
  PORTA_PIN1CTRL = PORT_PULLUPEN_bm;
  PORTA_PIN2CTRL = PORT_PULLUPEN_bm;
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
 pb_in ^= PINB_INVERT_MASK;
#endif

  input0 = ((pa_in & PINA_IN0_MASK) >> PINA_IN0_SHR) | ((pb_in & PINB_IN0_MASK) >> PINB_IN0_SHR);

#ifndef CONFIG_SERIAL_DEBUG
  input1 = ((pb_in & PINB_IN1_MASK) << PINB_IN1_SHL) |  (pc_in & PINC_IN1_MASK) | ((pa_in & PINA_IN1_MASK) << PINA_IN1_SHL);
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


  i2c_joystick_registers.input0 = input0;
  i2c_joystick_registers.input1 = input1;

}

//this is the function that receives bytes from the i2c master for the PRIMARY
// i2c address (which is for the joystick functionality)
inline void receive_i2c_callback_main_address(int i2c_bytes_received)
{
  //g_i2c_command_index %= sizeof(i2c_joystick_registers);

  for (byte x = g_i2c_command_index ; x < (g_i2c_command_index + i2c_bytes_received - 1) ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    if(x == 0x0A)   //this is a writeable register
    {
      i2c_joystick_registers.adc_on_bits = temp;
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

    if(x == 0x03)   //this is a writeable register
    {
      //set PWM duty cycle
      i2c_secondary_registers.config_PWM = temp;      
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


  i2c_joystick_registers.adc_res = ADC_RESOLUTION;
  i2c_joystick_registers.adc_on_bits = 0x00;

  i2c_secondary_registers.magic = 0xED;
  i2c_secondary_registers.ver_major = VERSION_MAJOR;
  i2c_secondary_registers.ver_minor = VERSION_MINOR;
  i2c_secondary_registers.config_PWM = g_pwm_duty_cycle;
  g_pwm_duty_cycle = ~i2c_secondary_registers.config_PWM; //unset it
  i2c_secondary_registers.poweroff_control = 0;

  i2c_joystick_registers.num_loops=0;
}


void loop() 
{
  #ifdef CONFIG_SPEED_PROFILING
    i2c_joystick_registers.num_loops++;
  #endif
  
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
  
#ifdef PIN_PWM
  if(g_pwm_duty_cycle != i2c_secondary_registers.config_PWM)
  {
    //output the duty cycle to the pwm pin (PA3?)
    analogWrite(PIN_PWM, i2c_secondary_registers.config_PWM);
    g_pwm_duty_cycle = i2c_secondary_registers.config_PWM;
  }
#endif
  
  read_all_gpio();

#ifdef USE_ADC0
  if(i2c_joystick_registers.adc_on_bits & (1 << 0))
  {
    adc = analogRead(PIN_PA4);
    i2c_joystick_registers.a0_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a0_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
  }
#endif
#ifdef USE_ADC1
  if(i2c_joystick_registers.adc_on_bits & (1 << 1))
  {
    adc = analogRead(PIN_PA5);
    i2c_joystick_registers.a1_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a1_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
  }
#endif
#ifdef USE_ADC2
  if(i2c_joystick_registers.adc_on_bits & (1 << 2))
  {
    adc = analogRead(PIN_PA6);
    i2c_joystick_registers.a2_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a2_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
  }
#endif
#ifdef USE_ADC3
  if(i2c_joystick_registers.adc_on_bits & (1 << 3))
  {
    adc = analogRead(PIN_PA7);
    i2c_joystick_registers.a3_msb = adc >> (ADC_RESOLUTION - 8);
    i2c_joystick_registers.a3_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
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
