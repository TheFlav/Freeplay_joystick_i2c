/*
 
 This program sets up a gamepad device in the sytem, interfaces with the attiny i2c device as a gamepad (digital only), and sends HID reports to the system.

 Setup:     sudo apt install libi2c-dev pigpio
 Compile:   gcc -o uhid-i2c-gamepad uhid-i2c-gamepad.c -li2c -lpigpio
 Run:       sudo ./uhid-i2c-gamepad

 Notes:
 	    On the Pi Zero 2 W, I had to use "git clone https://github.com/PinkFreud/WiringPi.git" to get a WiringPi that knew about the Pi02
 
 */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/uhid.h>
#include <stdint.h>

#include <signal.h>
#include <time.h>
//#include <math.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>







//prototypes
static double get_time_double(void); //get time in double (seconds)

static int uhid_create(int /*fd*/); //create uhid device TODO move header
static void uhid_destroy(int /*fd*/); //close uhid device  TODO move header
static int uhid_write(int /*fd*/, const struct uhid_event* /*ev*/); //write data to uhid device  TODO move header

void i2c_open(void); //open I2C bus
void i2c_close(void); //close I2C bus

void tty_signal_handler(int /*sig*/);

void debug_print_binary_int (int /*val*/, int /*bits*/, char* /*var*/); //print given var in binary format

//debug
bool debug = true; //TODO implement
bool debug_adv = true; //advanced debug output, benchmark
bool i2c_poll_rate_disable = true; //allow full throttle update if true

//tty output functions
double program_start_time = 0.;
#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time /*(double)clock()/CLOCKS_PER_SEC*/, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, get_time_double() - program_start_time /*(double)clock()/CLOCKS_PER_SEC*/, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr

//IRQ related
#define USE_WIRINGPI_IRQ //use wiringPi for IRQ
//#define USE_PIGPIO_IRQ //or USE_PIGPIO
//or comment out both of the above to poll

#if defined(USE_PIGPIO_IRQ) && defined(USE_WIRINGPI_IRQ)
	#error Cannot do both IRQ styles
#elif defined(USE_WIRINGPI_IRQ)
	#include <wiringPi.h>
	#define nINT_GPIO 40
#elif defined(USE_PIGPIO_IRQ)
	#include <pigpio.h>
	#define nINT_GPIO 10   //pigpio won't allow >31
#endif
bool irq_enable = false; //is set during runtime, do not edit

//Program related vars
bool kill_resquested = false; //allow clean close


//UHID related vars
const char* uhid_device_name = "Freeplay Gamepad";
int uhid_fd;
bool uhid_js_left_enable = false;
bool uhid_js_right_enable = false;




//I2C related vars
#define I2C_BUSNAME "/dev/i2c-1"
#define I2C_ADDRESS 0x20
int i2c_file = -1;
const int i2c_errors_report = 10; //error count before report to tty
int i2c_errors_count = 0; //errors count that happen in a row
int i2c_last_error = 0; //last detected error


//poll rate implement
const int i2c_poll_rate = 125; //poll per sec
const double i2c_poll_rate_time = 1. / i2c_poll_rate; //poll interval in sec
const double i2c_poll_duration_warn = 0.15; //warning if loop duration over this
double i2c_poll_duration = 0.; //poll loop duration
double poll_clock_start = 0., poll_benchmark_clock_start = -1.;
long poll_benchmark_loop = 0;




static unsigned char rdesc[] = { //TODO: dynamic build
    0x05, 0x01, //; USAGE_PAGE (Generic Desktop)
    0x09, 0x05, //; USAGE (Gamepad)
    0xA1, 0x01, //; COLLECTION (Application)
    0x05, 0x09,// ; USAGE_PAGE (Button)
    0x19, 0x01, //; USAGE_MINIMUM (Button 1)
    0x29, 0x0C, //; USAGE_MAXIMUM (Button 12)
    0x15, 0x00, //; LOGICAL_MINIMUM (0)
    0x25, 0x01, //; LOGICAL_MAXIMUM (1)
    0x75, 0x01, //; REPORT_SIZE (1)
    0x95, 0x10, //; REPORT_COUNT (16)
    0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    
        0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
        0x15, 0xFF,  // Logical Minimum (-1)
        0x25, 0x01,              //     LOGICAL_MAXIMUM (1)
        0x09, 0x34,  // Usage (X)
        0x09, 0x35,  // Usage (Y)
        0x75, 0x08,  // Report Size (8)
        0x95, 0x02,  // Report Count (2)
        0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)


       0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x26, 0xFF, 0xFF, 0x00,              //     LOGICAL_MAXIMUM (0xFFFF)
//       0x15, 0x00,  // Logical Minimum (0)
//       0x26, 0xff, 0xFF, 0x00,              //     LOGICAL_MAXIMUM (0xFFFF)
       0x09, 0x30,  // Usage (X)
       0x09, 0x31,  // Usage (Y)
       0x09, 0x32,  // Usage (Z)
       0x09, 0x33,  // Usage (?)
       0x75, 0x10,  // Report Size (16)
       0x95, 0x04,  // Report Count (4)
       0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
     

 0xC0,// ; END_COLLECTION
};



//Time related functions
static double get_time_double(void){ //get time in double (seconds)
	struct timespec tp; int result = clock_gettime(CLOCK_MONOTONIC, &tp);
	if (result == 0) {return tp.tv_sec + (double)tp.tv_nsec/1e9;}
	return 0.; //failed
}


//UHID related functions
static int uhid_create(int fd) { //create uhid device
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, uhid_device_name);
	ev.u.create.rd_data = rdesc;
	ev.u.create.rd_size = sizeof(rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

	return uhid_write(fd, &ev);
}

static void uhid_destroy(int fd){ //close uhid device
	struct uhid_event ev;

	if (fd < 0) return; //already failed

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;

	if(uhid_write(fd, &ev) == 0){fprintf(stderr, "uhid device destroyed\n");}
}

static int uhid_write(int fd, const struct uhid_event *ev){ //write data to uhid device
	ssize_t ret = write(fd, ev, sizeof(*ev));
	if (ret < 0) {
		print_stderr(/*fprintf(stderr, */"write to uhid device failed with errno:%d (%m)\n", -ret);
		return -errno;
	} else if (ret != sizeof(*ev)) {
		print_stderr(/*fprintf(stderr, */"wrong size wrote to uhid device: %zd != %zu\n", ret, sizeof(ev));
		return -EFAULT;
	} else {return 0;}
}

















/* This parses raw output reports sent by the kernel to the device. A normal
 * uhid program shouldn't do this but instead just forward the raw report.
 * However, for ducomentational purposes, we try to detect LED events here and
 * print debug messages for it. */
static void handle_output(struct uhid_event *ev)
{
	/* LED messages are adverised via OUTPUT reports; ignore the rest */
	if (ev->u.output.rtype != UHID_OUTPUT_REPORT)
		return;
	/* LED reports have length 2 bytes */
	if (ev->u.output.size != 2)
		return;
	/* first byte is report-id which is 0x02 for LEDs in our rdesc */
	if (ev->u.output.data[0] != 0x2)
		return;

	/* print flags payload */
	print_stderr(/*fprintf(stderr, */"LED output report received with flags %x\n", ev->u.output.data[1]);
}

static int event(int fd)
{
	struct uhid_event ev;
	ssize_t ret;

	memset(&ev, 0, sizeof(ev));
	ret = read(fd, &ev, sizeof(ev));
	if (ret == 0) {
		print_stderr(/*fprintf(stderr, */"Read HUP on uhid-cdev\n");
		return -EFAULT;
	} else if (ret < 0) {
		print_stderr(/*fprintf(stderr, */"Cannot read uhid-cdev: %m\n");
		return -errno;
	} else if (ret != sizeof(ev)) {
		print_stderr(/*fprintf(stderr, */"Invalid size read from uhid-dev: %zd != %zu\n", ret, sizeof(ev));
		return -EFAULT;
	}

	switch (ev.type) {
	case UHID_START:
		print_stderr(/*fprintf(stderr, */"UHID_START from uhid-dev\n");
		break;
	case UHID_STOP:
		print_stderr(/*fprintf(stderr, */"UHID_STOP from uhid-dev\n");
		break;
	case UHID_OPEN:
		print_stderr(/*fprintf(stderr, */"UHID_OPEN from uhid-dev\n");
		break;
	case UHID_CLOSE:
		print_stderr(/*fprintf(stderr, */"UHID_CLOSE from uhid-dev\n");
		break;
	case UHID_OUTPUT:
		print_stderr(/*fprintf(stderr, */"UHID_OUTPUT from uhid-dev\n");
		handle_output(&ev);
		break;
	case UHID_OUTPUT_EV:
		print_stderr(/*fprintf(stderr, */"UHID_OUTPUT_EV from uhid-dev\n");
		break;
	default:
		print_stderr(/*fprintf(stderr, */"Invalid event from uhid-dev: %u\n", ev.type);
	}

	return 0;
}
/*
static bool btn1_down;
static bool btn2_down;
static bool btn3_down;
static signed char abs_hor;
static signed char abs_ver;
static signed char wheel;
*/
struct i2c_register_struct {
    uint8_t input0;          // Reg: 0x00 - INPUT port 0
    uint8_t input1;          // Reg: 0x01 - INPUT port 1
    uint8_t a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
    uint8_t a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
    uint8_t a2_msb;          // Reg: 0x04 - ADC2 most significant 8 bits
    uint8_t a3_msb;          // Reg: 0x05 - ADC3 most significant 8 bits
    uint8_t a0_lsb;          // Reg: 0x06 - ADC0 least significant 8 bits
    uint8_t a1_lsb;          // Reg: 0x07 - ADC1 least significant 8 bits
    uint8_t a2_lsb;          // Reg: 0x08 - ADC2 least significant 8 bits
    uint8_t a3_lsb;          // Reg: 0x09 - ADC3 least significant 8 bits
    uint8_t adc_on_bits;     // Reg: 0x0A - turn ON bits here to activate ADC0 - ADC3 (only works if the USE_ADC# are turned on)
    uint8_t config0;         // Reg: 0x0B - Configuration port 0
    uint8_t configPWM;       // Reg: 0x0C - set PWM duty cycle
    uint8_t adc_res;         // Reg: 0x0D - current ADC resolution (maybe settable?)
} i2c_registers;

struct gamepad_report_t {
    int8_t hat_x;
    int8_t hat_y;
    uint8_t buttons7to0;
    uint8_t buttons15to8;
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
} gamepad_report, gamepad_report_prev;


static int send_event(int fd) {
	struct uhid_event ev;
	int index = 4;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
    ev.u.input.size = 4;
	if (uhid_js_left_enable) {ev.u.input.size += 4;}
	if (uhid_js_right_enable) {ev.u.input.size += 4;}

	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons15to8;

	ev.u.input.data[2] = (unsigned char) gamepad_report.hat_x;
	ev.u.input.data[3] = (unsigned char) gamepad_report.hat_y;

	if (uhid_js_left_enable) {
		ev.u.input.data[index] = gamepad_report.left_x & 0xFF;
		ev.u.input.data[index+1] = gamepad_report.left_x >> 8;
		ev.u.input.data[index+2] = gamepad_report.left_y & 0xFF;
		ev.u.input.data[index+3] = gamepad_report.left_y >> 8;
		index += 4;
	}

	if (uhid_js_right_enable) {
		ev.u.input.data[index] = gamepad_report.right_x & 0xFF;
		ev.u.input.data[index+1] = gamepad_report.right_x >> 8;
		ev.u.input.data[index+2] = gamepad_report.right_y & 0xFF;
		ev.u.input.data[index+3] = gamepad_report.right_y >> 8;
		//index += 4;
	}

	return uhid_write(fd, &ev);
}




//I2C related
void i2c_open(void){ //open I2C bus
	i2c_file = open(I2C_BUSNAME, O_RDWR);
	if (i2c_file < 0) {
		print_stderr(/*fprintf(stderr, */"FATAL: failed to open '%s' with errno:%d (%m)\n", I2C_BUSNAME, -i2c_file);
		exit(EXIT_FAILURE);
	}

	int ret = ioctl(i2c_file, I2C_SLAVE, I2C_ADDRESS);
	if (ret < 0) {
		close(i2c_file);
		print_stderr(/*fprintf(stderr, */"FATAL: ioctl failed for I2C adress:0x%02x with errno:%d (%m)\n", I2C_ADDRESS, -ret);
		exit(EXIT_FAILURE);
	} else {
		ret = i2c_smbus_read_byte_data(i2c_file, 0);
		if (ret < 0) {
			close(i2c_file);
			print_stderr(/*fprintf(stderr, */"FATAL: failed to read from I2C adress:0x%02x with errno:%d (%m)\n", I2C_ADDRESS, -ret);
			exit(EXIT_FAILURE);
		}
	}
}

void i2c_close(void){ //close I2C bus
	if (i2c_file < 0) {print_stderr(/*fprintf(stderr, */"nothing to close\n");
	} else {
		if (close(i2c_file) < 0){print_stderr(/*fprintf(stderr, */"failed to close I2C handle\n");
		} else {print_stderr(/*fprintf(stderr, */"I2C handle succefully closed\n");}
	}
}



/*  From the attiny code
 
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





void i2c_poll_joystick(){
    //int8_t dpad_u, dpad_d, dpad_l, dpad_r;
    
    //bool btn_a, btn_b, btn_c, btn_x, btn_y, btn_z, btn_l, btn_r, btn_start, btn_select, btn_l2, btn_r2;

    //uint8_t buttons_count = 24;
	//bool btn [buttons_count];
	//for (uint8_t i=0; i < buttons_count; i++){btn[i]=false;} //bool reset

	//int ret = i2c_smbus_read_word_data(i2c_file, 0);
	int ret = i2c_smbus_read_i2c_block_data(i2c_file, 0, 10 /*sizeof(i2c_registers)*/, (uint8_t *)&i2c_registers);
	if (ret < 0) {
		i2c_errors_count++;

		if (ret == -6) {
			print_stderr(/*fprintf(stderr, */"FATAL: i2c_smbus_read_word_data() failed with errno %d : %m\n", -ret);
			kill_resquested = true;
		}

		if (i2c_errors_count >= i2c_errors_report) { //report i2c heavy fail
			print_stderr(/*fprintf(stderr, */"WARNING: I2C requests failed %d times in a row\n", i2c_errors_count);
			i2c_errors_count = 0;
		}

		i2c_last_error = ret; return;
	}

	if (i2c_errors_count > 0){
		print_stderr(/*fprintf(stderr, */"DEBUG: last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));
		i2c_last_error = i2c_errors_count = 0; //reset error count
	}
	
	int8_t dpad[4];
	for (int8_t i=0; i<4; i++){dpad[i] = (~i2c_registers.input0 >> i & 0x01);}
	gamepad_report.hat_y = dpad[0] - dpad[1];
	gamepad_report.hat_x = dpad[3] - dpad[2];

	gamepad_report.buttons7to0 = ~(i2c_registers.input0 >> 4 | i2c_registers.input1 << 4);
	gamepad_report.buttons15to8 = ~(i2c_registers.input1 >> 4);

	if (uhid_js_left_enable) {
		gamepad_report.left_x = i2c_registers.a0_msb << 8 | i2c_registers.a0_lsb;
		gamepad_report.left_y = i2c_registers.a1_msb << 8 | i2c_registers.a1_lsb;
	}

	if (uhid_js_right_enable) {
		gamepad_report.right_x = i2c_registers.a2_msb << 8 | i2c_registers.a2_lsb;
		gamepad_report.right_y = i2c_registers.a3_msb << 8 | i2c_registers.a3_lsb;
	}

	int report_val = 0, report_prev_val = 1;
	if (!(uhid_js_left_enable + uhid_js_right_enable)) {
		report_val = gamepad_report.buttons7to0 + gamepad_report.buttons15to8 + gamepad_report.hat_x + gamepad_report.hat_y;
		report_prev_val = gamepad_report_prev.buttons7to0 + gamepad_report_prev.buttons15to8 + gamepad_report_prev.hat_x + gamepad_report_prev.hat_y;
	}

	//report
	if (report_val != report_prev_val){
		gamepad_report_prev = gamepad_report;
		send_event(uhid_fd);
	}

/*
    ret = ~ret;         //invert all bits 1=pressed 0=unpressed

    dpad_u = (dpad_bits >> 0 & 0x01);
    dpad_d = (dpad_bits >> 1 & 0x01);
    
    dpad_l = (dpad_bits >> 2 & 0x01);
    dpad_r = (dpad_bits >> 3 & 0x01);

    ret >>= 4;
    btn_a = ret & 0b1;
    ret >>= 1;
    btn_b = ret & 0b1;
    ret >>= 1;
    btn_l2 = ret & 0b1;
    ret >>= 1;
    btn_r2 = ret & 0b1;
    ret >>= 1;
    btn_x = ret & 0b1;
    ret >>= 1;
    btn_y = ret & 0b1;
    ret >>= 1;
    btn_start = ret & 0b1;
    ret >>= 1;
    btn_select = ret & 0b1;
    ret >>= 1;
    btn_l = ret & 0b1;
    ret >>= 1;
    btn_r = ret & 0b1;
    ret >>= 1;
    btn_z = ret & 0b1;
    ret >>= 1;
    btn_c = ret & 0b1;
    
    gamepad_report.buttons7to0 = (btn_r << 7) | (btn_l << 6) | (btn_z << 5) | (btn_y << 4) | (btn_x << 3) | (btn_c << 2) | (btn_b << 1) | btn_a;
    gamepad_report.buttons12to8 = (btn_select << 3) | (btn_start << 2) | (btn_r2 << 1) | btn_l2;

    gamepad_report.hat_x = dpad_r - dpad_l;
    gamepad_report.hat_y = dpad_u - dpad_d;
*/
}


#ifdef USE_PIGPIO_IRQ
//#error Somewhat untested code here, use at your own risk
void gpio_callback(int gpio, int level, uint32_t tick) { //look to work
	switch (level) {
		case 0:
			print_stderr(/*fprintf(stderr, */"DEBUG: GPIO%d low\n", gpio);
			i2c_poll_joystick();
			break;
		case 1:
			print_stderr(/*fprintf(stderr, */"DEBUG: GPIO%d high\n", gpio);
			break;
		case 2:
			print_stderr(/*fprintf(stderr, */"DEBUG: GPIO%d WATCHDOG\n", gpio);
			break;
	}
}
#endif

#ifdef USE_WIRINGPI_IRQ
void attiny_irq_handler(void) {
	print_stderr(/*fprintf(stderr, */"DEBUG: GPIO%d triggered\n", nINT_GPIO);
    i2c_poll_joystick();
}
#endif

void tty_signal_handler(int sig) { //handle signal func
	print_stderr(/*fprintf(stderr, */"DEBUG: signal received: %d\n", sig);
	kill_resquested = true;
}



void debug_print_binary_int (int val, int bits, char* var) { //print given var in binary format
	printf("DEBUG: BIN: %s : ", var);
	for(int mask_shift = bits-1; mask_shift > -1; mask_shift--){printf("%d", (val >> mask_shift) & 0b1);}
	printf("\n");
}







int main(int argc, char **argv) {
	program_start_time = get_time_double();
	const char *path = "/dev/uhid";
	int ret, main_return = EXIT_SUCCESS;

	//tty signal handling
	signal(SIGINT, tty_signal_handler); //ctrl-c
	signal(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
	//signal(SIGKILL, tty_signal_handler); //doesn't work, program get killed before able to handle

	if (argc >= 2) { //TODO
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			fprintf(stderr, "Usage: %s [%s]\n", argv[0], path);
			return EXIT_SUCCESS;
		} else {
			path = argv[1];
		}
	}

	i2c_open(); //open I2C bus
    print_stderr(/*fprintf(stderr, */"I2C bus '%s', address:0x%02x opened\n", I2C_BUSNAME, I2C_ADDRESS);

	//detect analog config
	ret = i2c_smbus_read_byte_data(i2c_file, 0x0a);
	if (ret < 0){
		i2c_close();
		print_stderr(/*fprintf(stderr, */"FATAL: reading ADC configuration failed with errno %d (%s)\n", -ret, strerror(-ret));
		return EXIT_FAILURE;
	} else {
		bool tmp_adc[4];
		for (uint8_t i=0; i<4; i++){tmp_adc[i] = (ret >> i) & 0b1;}
		uhid_js_left_enable = tmp_adc[0] && tmp_adc[1];
		uhid_js_right_enable = tmp_adc[2] && tmp_adc[3];

		if (uhid_js_left_enable + uhid_js_right_enable) {
			printf("detected ADC configuration: %s %s\n", uhid_js_left_enable?"left":"", uhid_js_right_enable?"right":"");
			#if defined(USE_WIRINGPI_IRQ) || defined(USE_PIGPIO_IRQ)
			printf("IRQ disabled\n");
			#endif
		}
	}

	uhid_fd = open(path, O_RDWR | O_CLOEXEC);
	if (uhid_fd < 0) {
		print_stderr(/*fprintf(stderr, */"failed to open uhid-cdev %s with errno:%d (%m)\n", path, -uhid_fd); return EXIT_FAILURE;
	} else {print_stderr(/*fprintf(stderr, */"uhid-cdev %s opened\n", path);}

	ret = uhid_create(uhid_fd);
	if (ret) {close(uhid_fd); return EXIT_FAILURE;
	} else {print_stderr(/*fprintf(stderr, */"uhid device created\n");}

	i2c_poll_joystick(); //initial poll

	if (!(uhid_js_left_enable + uhid_js_right_enable)) {
		#ifdef USE_WIRINGPI_IRQ
		#define WIRINGPI_CODES 1 //allow error code return
		int err;
		if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
			print_stderr("failed to initialize wiringPi (errno:%d)\n", -err);
		} else {
			if ((err = wiringPiISR(nINT_GPIO, INT_EDGE_FALLING, &attiny_irq_handler)) < 0){
				print_stderr("wiringPi failed to set callback for GPIO%d\n", nINT_GPIO);
			} else {
				fprintf(stderr, "using wiringPi IRQ\n");
				irq_enable = true;
			}
		}
		#endif
		
		#ifdef USE_PIGPIO_IRQ
		int ver, err; bool irq_failed=false;
		if ((ver = gpioInitialise()) > 0){
			print_stderr("pigpio: version: %d\n",ver);
		} else {
			print_stderr("failed to detect pigpio version\n"); irq_failed = true
		}

		if (!irq_failed && nINT_GPIO > 31){
			print_stderr("pigpio limited to GPIO0-31, asked for %d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetMode(nINT_GPIO, PI_INPUT)) != 0){ //set as input
			print_stderr("FATAL: pigpio failed to set GPIO%d to input\n", nINT_GPIO); irq_failed = true;
		}
		
		if (!irq_failed && (err = gpioSetPullUpDown(nINT_GPIO, PI_PUD_UP)) != 0){ //set pull up
			print_stderr("FATAL: pigpio failed to set PullUp for GPIO%d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioGlitchFilter(nINT_GPIO, 100)) != 0){ //glitch filter to avoid bounce
			print_stderr("FATAL: pigpio failed to set glitch filter for GPIO%d\n", nINT_GPIO); irq_failed = true;
		}

		if (!irq_failed && (err = gpioSetAlertFunc(nINT_GPIO, gpio_callback)) != 0){ //callback setup
			print_stderr("FATAL: pigpio failed to set callback for GPIO%d\n", nINT_GPIO); irq_failed = true;
		} else {
			fprintf(stderr, "using pigpio IRQ\n");
			gpioSetSignalFunc(SIGINT, tty_signal_handler); //ctrl-c
			gpioSetSignalFunc(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
			irq_enable = true;
		}
		#endif
	}

    fprintf(stderr, "Press '^C' to quit...\n");
	
	if (irq_enable){
		while (!kill_resquested){usleep(100000);} //sleep until app close requested
	} else {
		if (!i2c_poll_rate_disable){fprintf(stderr, "polling at %dhz\n", i2c_poll_rate);
		} else {fprintf(stderr, "poll speed not limited\n");}

		while (!kill_resquested) {
			poll_clock_start = get_time_double();
			if (debug_adv && poll_benchmark_clock_start < 0.) {poll_benchmark_clock_start = poll_clock_start;} //benchmark

			i2c_poll_joystick();

			//poll rate implement
			
			if (kill_resquested) break;
			i2c_poll_duration = get_time_double() - poll_clock_start;
			//printf ("DEBUG: poll_clock_start:%lf, i2c_poll_duration:%lf\n", poll_clock_start, i2c_poll_duration);

			if (!i2c_poll_rate_disable){
				if (i2c_poll_duration > i2c_poll_duration_warn){printf ("WARNING: extremely long loop duration: %dms\n", (int)(i2c_poll_duration*1000));}
				if (i2c_poll_duration < 0){i2c_poll_duration = 0;} //hum, how???
				if (i2c_poll_duration < i2c_poll_rate_time){usleep((useconds_t) ((double)(i2c_poll_rate_time - i2c_poll_duration) * 1000000));} //need to sleep to match poll rate
				//printf ("DEBUG: loop duration:%lf, i2c_poll_rate_time:%lf\n", get_time_double() - poll_clock_start, i2c_poll_rate_time);
			}// else {printf ("DEBUG: loop duration:%lf\n", get_time_double() - poll_clock_start);}

			if (debug_adv){ //benchmark mode
				poll_benchmark_loop++;
				if ((get_time_double() - poll_benchmark_clock_start) > 2.) { //report every seconds
					print_stderr("DEBUG: poll loops per sec (2secs samples) : %ld\n", poll_benchmark_loop/2);
					poll_benchmark_loop = 0; poll_benchmark_clock_start = -1.;
				}
			}
		}
	}

	app_close:
	if (i2c_last_error != 0) {print_stderr(/*fprintf(stderr, */"DEBUG: last detected I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));}

	i2c_close();
	uhid_destroy(uhid_fd);
	return main_return;
}
