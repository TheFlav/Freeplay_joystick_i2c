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
#include <termios.h>
#include <unistd.h>
#include <linux/uhid.h>
#include <stdint.h>

#include <signal.h>
#include <time.h>
//#include <math.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>


#define print_stderr(fmt, ...) do {fprintf(stderr, "%lf: %s:%d: %s(): " fmt, (double)clock()/CLOCKS_PER_SEC, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr
#define print_stdout(fmt, ...) do {fprintf(stdout, "%lf: %s:%d: %s(): " fmt, (double)clock()/CLOCKS_PER_SEC, __FILE__, __LINE__, __func__, ##__VA_ARGS__);} while (0) //Flavor: print advanced debug to stderr

//prototypes

static int uhid_create(int /*fd*/); //create uhid device TODO move header
static void uhid_destroy(int /*fd*/); //close uhid device  TODO move header
static int uhid_write(int /*fd*/, const struct uhid_event* /*ev*/); //write data to uhid device  TODO move header

void i2c_open(void); //open I2C bus
void i2c_close(void); //close I2C bus

void tty_signal_handler(int /*sig*/);


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

int fd;

#define I2C_BUSNAME "/dev/i2c-1"
#define I2C_ADDRESS 0x20
int i2c_file = -1;

const char* uhid_device_name = "Freeplay Gamepad";


const int i2c_poll_rate = 125; //poll per sec
const double i2c_poll_rate_time = 1. / i2c_poll_rate; //poll interval in sec
const double i2c_poll_duration_warn = 0.15; //warning if loop duration over this
double i2c_poll_duration = 0.; //poll loop duration
clock_t poll_clock_start/*, poll_clock_end*/;

bool kill_resquested = false; //allow clean close
const int i2c_errors_report = 10; //error count before report to tty
int i2c_errors_count = 0; //errors count that happen in a row
int i2c_last_error = 0; //last detected error



static unsigned char rdesc[] = {
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
 0xC0,// ; END_COLLECTION
};






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

static bool btn1_down;
static bool btn2_down;
static bool btn3_down;
static signed char abs_hor;
static signed char abs_ver;
static signed char wheel;

struct gamepad_report_t
{
    unsigned char buttons7to0;
    unsigned char buttons12to8;
    int8_t hat_x;
    int8_t hat_y;
} gamepad_report, gamepad_report_prev;


static int send_event(int fd)
{
	struct uhid_event ev;


	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
    ev.u.input.size = 4;
    
	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons12to8;
    
    ev.u.input.data[2] = (unsigned char) gamepad_report.hat_x;
    ev.u.input.data[3] = (unsigned char) gamepad_report.hat_y;

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





void i2c_poll_joystick()
{
    int8_t dpad_u, dpad_d, dpad_l, dpad_r;
    
    bool btn_a, btn_b, btn_c, btn_x, btn_y, btn_z, btn_l, btn_r, btn_start, btn_select, btn_l2, btn_r2;
    
    uint8_t dpad_bits;

	int ret=0;

	ret = i2c_smbus_read_word_data(i2c_file, 0);
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

		i2c_last_error=ret; return;
	}

	if (i2c_errors_count > 0){
		print_stderr(/*fprintf(stderr, */"DEBUG: last I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));
		i2c_last_error = i2c_errors_count = 0; //reset error count
	}
	



    ret = ~ret;         //invert all bits 1=pressed 0=unpressed

    dpad_bits = ret & 0x000F;
    
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

    dpad_u = (dpad_bits >> 0 & 0x01);
    dpad_d = (dpad_bits >> 1 & 0x01);
    
    dpad_l = (dpad_bits >> 2 & 0x01);
    dpad_r = (dpad_bits >> 3 & 0x01);

    gamepad_report.hat_x = dpad_r - dpad_l;
    gamepad_report.hat_y = dpad_u - dpad_d;

	//report
	int report_val = gamepad_report.buttons7to0 + gamepad_report.buttons12to8 + gamepad_report.hat_x + gamepad_report.hat_y;
	int report_prev_val = gamepad_report_prev.buttons7to0 + gamepad_report_prev.buttons12to8 + gamepad_report_prev.hat_x + gamepad_report_prev.hat_y;
	if (report_val != report_prev_val){
		gamepad_report_prev = gamepad_report;
		send_event(fd);
	}
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




int main(int argc, char **argv) {
	const char *path = "/dev/uhid";
	//struct pollfd pfds[2]; //used for?
	int ret, main_return = EXIT_SUCCESS;
	struct termios state;

	ret = tcgetattr(STDIN_FILENO, &state);
	if (ret) {print_stderr(/*fprintf(stderr, */"Cannot get tty state\n");
	} else {
		state.c_lflag &= ~ICANON;
		state.c_cc[VMIN] = 1;
		ret = tcsetattr(STDIN_FILENO, TCSANOW, &state);
		if (ret) print_stderr(/*fprintf(stderr, */"Cannot set tty state\n");
	}

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

	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		print_stderr(/*fprintf(stderr, */"failed to open uhid-cdev %s with errno:%d (%m)\n", path, -fd);return EXIT_FAILURE;
	} else {print_stderr(/*fprintf(stderr, */"uhid-cdev %s opened\n", path);}

	ret = uhid_create(fd);
	if (ret) {close(fd); return EXIT_FAILURE;
	} else {print_stderr(/*fprintf(stderr, */"uhid device created\n");}

	//used for?
	/*
	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;
	pfds[1].fd = fd;
	pfds[1].events = POLLIN;
*/

	i2c_open(); //open I2C bus
    print_stderr(/*fprintf(stderr, */"I2C bus '%s', address:0x%02x opened\n", I2C_BUSNAME, I2C_ADDRESS);

	i2c_poll_joystick(); //initial poll

#ifdef USE_WIRINGPI_IRQ
	#define WIRINGPI_CODES 1 //allow error code return
	int err;
	if ((err = wiringPiSetupGpio()) < 0){ //use BCM numbering
		print_stderr("FATAL: failed to initialize wiringPi (errno:%d)\n", -err);
		main_return = EXIT_FAILURE; goto app_close;
	}

	if ((err = wiringPiISR(nINT_GPIO, INT_EDGE_FALLING, &attiny_irq_handler)) < 0){
		print_stderr("FATAL: wiringPi failed to set callback for GPIO%d\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	} else {fprintf(stderr, "using wiringPi IRQ\n");}
#endif
    
#ifdef USE_PIGPIO_IRQ
	int ver, err;
	if ((ver = gpioInitialise()) > 0){print_stderr("pigpio: version: %d\n",ver);
	} else {print_stderr("FATAL: failed to detect pigpio version\n"); exit(EXIT_FAILURE);}

	if (nINT_GPIO > 31){ //check pin
		print_stderr("FATAL: pigpio limited to GPIO0-31, asked for %d\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	}

	if ((err = gpioSetMode(nINT_GPIO, PI_INPUT)) != 0){ //set as input
		print_stderr("FATAL: pigpio failed to set GPIO%d to input\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	}
	
	if ((err = gpioSetPullUpDown(nINT_GPIO, PI_PUD_UP)) != 0){ //set pull up
		print_stderr("FATAL: pigpio failed to set PullUp for GPIO%d\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	}

	if ((err = gpioGlitchFilter(nINT_GPIO, 100)) != 0){ //glitch filter to avoid bounce
		print_stderr("FATAL: pigpio failed to set glitch filter for GPIO%d\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	}

	if ((err = gpioSetAlertFunc(nINT_GPIO, gpio_callback)) != 0){ //callback setup
		print_stderr("FATAL: pigpio failed to set callback for GPIO%d\n", nINT_GPIO);
		main_return = EXIT_FAILURE; goto app_close;
	} else {fprintf(stderr, "using pigpio IRQ\n");}

	//signal callbacks setup
	gpioSetSignalFunc(SIGINT, tty_signal_handler); //ctrl-c
	gpioSetSignalFunc(SIGTERM, tty_signal_handler); //SIGTERM from htop or other
#endif
    
    fprintf(stderr, "Press '^C' to quit...\n");
	
#if defined USE_WIRINGPI_IRQ || defined USE_PIGPIO_IRQ
	while (!kill_resquested){usleep(100000);} //sleep until app close requested
#else
	fprintf(stderr, "no IRQ defined, polling at %dhz\n", i2c_poll_rate);
	while (!kill_resquested) {
		poll_clock_start = clock();

		i2c_poll_joystick();

		//poll rate implement
		if (kill_resquested) break;
		i2c_poll_duration = (double)(clock() - poll_clock_start) / CLOCKS_PER_SEC;
		if (i2c_poll_duration > i2c_poll_duration_warn){printf ("WARNING: extremily long loop duration: %dms\n", (int)(i2c_poll_duration*1000));}
		if (i2c_poll_duration < 0){i2c_poll_duration = i2c_poll_rate_time + 1.;} //rollover, force update
		if (i2c_poll_duration < i2c_poll_rate_time){usleep((useconds_t) ((double)(i2c_poll_rate_time - i2c_poll_duration) * 1000000));} //need to sleep to match poll rate
	}
#endif

	app_close:
	if (i2c_last_error!=0) {print_stderr(/*fprintf(stderr, */"DEBUG: last detected I2C error: %d (%s)\n", -i2c_last_error, strerror(-i2c_last_error));}

	i2c_close();
	uhid_destroy(fd);
	return main_return;
}
