//Freeplay_joystick_i2c_attiny

// Firmware for the ATtiny817/ATtiny427/etc. to emulate the behavior of the PCA9555 i2c GPIO Expander
//currently testing on Adafruit 817

#define ADC_RESOLUTION 10
//#define ADC_RESOLUTION 12  //can do analogReadResolution(12) on 2-series 427/827 chips

#define CONFIG_I2C_ADDR     0x20
//#define CONFIG_I2C_2NDADDR  0x40  //0x30 wouldn't work

//#define CONFIG_SERIAL_DEBUG 1     //this shares a pin with the nINT, so be careful


#include <Wire.h>


struct i2c_register_struct {
  byte input0;          // Reg: 0x00 - INPUT port 0
  byte input1;          // Reg: 0x01 - INPUT port 1
  byte a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
  byte a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
  byte a2_msb;          // Reg: 0x04 - ADC2 most significant 8 bits
  byte a3_msb;          // Reg: 0x05 - ADC3 most significant 8 bits
  byte a0_lsb;          // Reg: 0x02 - ADC0 least significant 8 bits
  byte a1_lsb;          // Reg: 0x03 - ADC1 least significant 8 bits
  byte a2_lsb;          // Reg: 0x04 - ADC2 least significant 8 bits
  byte a3_lsb;          // Reg: 0x05 - ADC3 least significant 8 bits
  byte config0;         // Reg: 0x06 - Configuration port 0
  byte config1;         // Reg: 0x07 - Configuration port 1
  byte configPWM;       // Reg: 0x08 - set PWM duty cycle
  byte adc_res;         // Reg: 0x10 - current ADC resolution (maybe settable?)
} i2c_registers;

volatile byte g_last_sent_input0 = 0xFF;
volatile byte g_last_sent_input1 = 0xFF;

volatile byte g_i2c_command_index; //Gets set when user writes an address. We then serve the spot the user requested.

byte g_pwm_duty_cycle = 0xFF;  //100% on

/*
 * PA1 = IO0_0 = UP
 * PA2 = IO0_1 = DOWN
 * PB4 = IO0_2 = LEFT
 * PB5 = IO0_3 = RIGHT
 * PB6 = IO0_4 = BTN_A
 * PB7 = IO0_5 = BTN_B
 * PA6 = IO0_6 = BTN_C ifndef USE_ADC2
 * PA7 = IO0_7 = BTN_Z ifndef USE_ADC3
 * 
 * PC0 = IO1_0 = BTN_X
 * PC1 = IO1_1 = BTN_Y
 * PC2 = IO1_2 = BTN_START
 * PC3 = IO1_3 = BTN_SELECT
 * PC4 = IO1_4 = BTN_L
 * PC5 = IO1_5 = BTN_R
 * PB2 = IO1_6 = BTN_L2 ifndef CONFIG_SERIAL_DEBUG (or can be used for UART TXD0 for debugging)  [MAY NEED TO BE PWM output!!!!]
 * PB3 = IO1_7 = BTN_R2 ifndef CONFIG_SERIAL_DEBUG (or can be used for UART RXD0 for debugging)
 */


// How many ADC's do you want?
//#define USE_ADC0
//#define USE_ADC1
//#define USE_ADC2
//#define USE_ADC3

//I feel like we only want to do interrupts if we're not doing ADCs, but this can change around

#ifndef USE_ADC0
#define nINT_PIN 0 //also PIN_PA4
#endif

#if defined(USE_ADC0) && defined(nINT_PIN)
 #error ADC0 and nINT_PIN share the same pin
#endif


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

#define PINB_IN1_MASK      (0b00001100)   //the pins from PINB that are used in IN1
#define PINC_IN1_MASK      (0b00111111)   //the pins from PINB that are used in IN1

#define PINA_GPIO_MASK     (PINA_IN0_MASK)
#define PINB_GPIO_MASK     ((PINB_IN0_MASK | PINB_IN1_MASK) & ~PINB_UART_MASK)
#define PINC_GPIO_MASK     (PINC_IN1_MASK)


#define PINA_IN0_SHR       1
#define PINB_IN0_SHR       2
#define PINB_IN1_SHL       4

void setup_gpio(void)
{
  //set as inputs
  PORTA_DIRCLR = PINA_GPIO_MASK | PINA_ADC0_MASK | PINA_ADC1_MASK | PINA_ADC2_MASK | PINA_ADC3_MASK;
  PORTB_DIRCLR = PINB_GPIO_MASK;
  PORTC_DIRCLR = PINC_GPIO_MASK;

  //set pullups

//#define PINA_MASK (0b00001110) for ADC
  //PORTA_PIN0CTRL = PORT_PULLUPEN_bm;
  PORTA_PIN1CTRL = PORT_PULLUPEN_bm;
  PORTA_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTA_PIN3CTRL = PORT_PULLUPEN_bm;
  PORTA_PIN4CTRL = PORT_PULLUPEN_bm; //ADC0
  PORTA_PIN5CTRL = PORT_PULLUPEN_bm; //ADC1
  PORTA_PIN6CTRL = PORT_PULLUPEN_bm; //ADC2
  PORTA_PIN7CTRL = PORT_PULLUPEN_bm; //ADC3
  
//#define PINB_MASK (0b01110000)
  //PORTB_PIN0CTRL = PORT_PULLUPEN_bm;
  //PORTB_PIN1CTRL = PORT_PULLUPEN_bm;
#ifndef CONFIG_SERIAL_DEBUG
  PORTB_PIN2CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN3CTRL = PORT_PULLUPEN_bm;
#endif
  PORTB_PIN4CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN5CTRL = PORT_PULLUPEN_bm;
  PORTB_PIN6CTRL = PORT_PULLUPEN_bm;
  //PORTB_PIN7CTRL = PORT_PULLUPEN_bm;

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

void read_all_gpio(void)
{
  i2c_registers.input0 = ((PORTA_IN & PINA_IN0_MASK) >> PINA_IN0_SHR) | ((PORTB_IN & PINB_IN0_MASK) >> PINB_IN0_SHR);

#ifndef CONFIG_SERIAL_DEBUG
  i2c_registers.input1 = ((PORTB_IN & PINB_IN1_MASK) << PINB_IN1_SHL) |  (PORTC_IN & PINC_IN1_MASK);
#else
  i2c_registers.input1 = ((PORTB_IN & PINB_IN1_MASK | PINB_UART_MASK) << PINB_IN1_SHL) |  (PORTC_IN & PINC_IN1_MASK);   //act like the PINB_UART_MASK pins are never pressed
#endif

#if !defined(USE_ADC2) && !defined(USE_ADC3)
  i2c_registers.input0 |= (PORTA_IN & (PINA_ADC2_MASK | PINA_ADC3_MASK));
#else
 #ifndef USE_ADC2
  i2c_registers.input0 |= (PORTA_IN & PINA_ADC2_MASK);
 #endif
 #ifndef USE_ADC3
  i2c_registers.input0 |= (PORTA_IN & PINA_ADC3_MASK);
 #endif
#endif
}

//When Qwiic Joystick receives data bytes from Master, this function is called as an interrupt
//(Serves rewritable I2C address)
void receive_i2c_callback(int i2c_bytes_received)
{
  g_i2c_command_index = Wire.read(); //Get the memory map offset from the user

  for (byte x = 0 ; x < i2c_bytes_received - 1 ; x++)
  {
    byte temp = Wire.read(); //We might record it, we might throw it away

    //  byte configPWM;       // Reg: 0x08 - set PWM duty cycle
    if(x == 0x08)
    {
      i2c_registers.configPWM = temp;
      
    }
  }
  
  return;
}


//Respond to GET commands
void request_i2c_callback()
{
  //This will write the entire contents of the register map struct starting from
  //the register the user requested, and when it reaches the end the master
  //will read 0xFFs.

  Wire.write((((byte *)&i2c_registers) + g_i2c_command_index), sizeof(i2c_registers) - g_i2c_command_index);

  //TODO: fixme?
  if(g_i2c_command_index == 0)
  {
    //if the index was 0, just assume we sent at least 2 bytes.  I don't know how to tell
    //could use getBytesRead combined with g_i2c_command_index
    g_last_sent_input0 = i2c_registers.input0;
    g_last_sent_input1 = i2c_registers.input1;
  }
  
  if(g_i2c_command_index == 1)
    g_last_sent_input1 = i2c_registers.input1;
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


void set_pwm_duty_cycle(byte new_duty_cycle)
{
  //output the duty cycle to the pwm pin (PA3?)
}



void setup() {
#ifdef nINT_PIN
  pinMode(nINT_PIN, OUTPUT);
#endif 

  startI2C(); //Determine the I2C address we should be using and begin listening on I2C bus

#ifdef CONFIG_SERIAL_DEBUG
  Serial.begin(115200);
  delay(500);
  Serial.println("PCA9555 Emulation");
  Serial.flush();
#endif

  analogReadResolution(ADC_RESOLUTION);
  i2c_registers.adc_res = ADC_RESOLUTION;
}

void loop() {
  word adc;
  
  if(g_pwm_duty_cycle != i2c_registers.configPWM)
  {
    set_pwm_duty_cycle(i2c_registers.configPWM);
    g_pwm_duty_cycle = i2c_registers.configPWM;
  }
  read_all_gpio();

#ifdef USE_ADC0
  adc = analogRead(PIN_PA4);
  i2c_registers.a0_msb = adc >> (ADC_RESOLUTION - 8);
  i2c_registers.a0_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
#endif
#ifdef USE_ADC1
  adc = analogRead(PIN_PA5);
  i2c_registers.a1_msb = adc >> (ADC_RESOLUTION - 8);
  i2c_registers.a1_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
#endif
#ifdef USE_ADC2
  adc = analogRead(PIN_PA6);
  i2c_registers.a2_msb = adc >> (ADC_RESOLUTION - 8);
  i2c_registers.a2_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
#endif
#ifdef USE_ADC3
  adc = analogRead(PIN_PA7);
  i2c_registers.a3_msb = adc >> (ADC_RESOLUTION - 8);
  i2c_registers.a3_lsb = adc << (8 - (ADC_RESOLUTION - 8)) & 0xFF;
#endif

#ifdef nINT_PIN
  digitalWrite(nINT_PIN, ((g_last_sent_input0 == i2c_registers.input0) && (g_last_sent_input1 == i2c_registers.input1)));
#endif
}
