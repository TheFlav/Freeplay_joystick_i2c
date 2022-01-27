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


#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

//#define USE_WIRINGPI_IRQ //use wiringPi for IRQ
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
#define I2C_ADDRESS 0x30
int i2c_file = -1;


/*
 * digital inputs
 *
 * input0
 *
 * PC0 = IO0_0 = BTN_X
 * PC1 = IO0_1 = BTN_Y
 * PC2 = IO0_2 = BTN_START
 * PC3 = IO0_3 = BTN_SELECT
 * PC4 = IO0_4 = BTN_L    (AKA BTN_TL in Linux)
 * PC5 = IO0_5 = BTN_R    (AKA BTN_TR in Linux)
 * PB6 = IO0_6 = BTN_A
 * PB7 = IO0_7 = BTN_B
 *
 * input1       A18 means analog pin 18 (A7 = analog 7 aka PB4)
 *
 * A18 = IO1_0 = UP
 * A18 = IO1_1 = DOWN
 * A18 = IO1_2 = LEFT
 * A18 = IO1_3 = RIGHT
 * PB3 = IO1_4 = BTN_L2    (AKA BTN_TL2 in Linux)
 * PB4 = IO1_5 = BTN_R2    (AKA BTN_TR2 in Linux)   //PB4 can be turned into A7 to do an analog resistor ladder if we need BTN_THUMBL and BTN_THUMBR buttons
 * PB5 = IO1_6 = BTN_POWER
 * --- = IO1_7 = always high
 *
 * input2       EXTENDED DIGITAL INPUT REGISTER
 *
 * A7  = IO2_0 = BTN_THUMBL
 * A7  = IO2_1 = BTN_THUMBR
 * --- = IO2_2 = always high
 * --- = IO2_3 = always high
 * PA4 = IO2_4 = BTN_0 (when ADC0 not used)
 * PA5 = IO2_5 = BTN_1 (when ADC1 not used)
 * PA6 = IO2_6 = BTN_2 (when ADC2 not used)
 * PA7 = IO2_7 = BTN_3 (when ADC3 not used)
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

struct input0_bit_struct
{
    uint8_t btn_x : 1;
    uint8_t btn_y : 1;
    uint8_t btn_start : 1;
    uint8_t btn_select : 1;
    uint8_t btn_tl : 1;
    uint8_t btn_tr : 1;
    uint8_t btn_a : 1;
    uint8_t btn_b : 1;
};

struct input1_bit_struct
{
    uint8_t dpad_u : 1;
    uint8_t dpad_d : 1;
    uint8_t dpad_l : 1;
    uint8_t dpad_r : 1;
    uint8_t btn_tl2 : 1;
    uint8_t btn_tr2 : 1;
    uint8_t btn_mode : 1;
    uint8_t unused7 : 1;
};

/*
struct input2_bit_struct
{
    uint8_t btn_thumbl : 1;
    uint8_t btn_thumbr : 1;
    uint8_t unused2 : 1;
    uint8_t unused3 : 1;
    uint8_t btn_0 : 1;
    uint8_t btn_1 : 1;
    uint8_t btn_2 : 1;
    uint8_t btn_3 : 1;
};*/

struct digital_inputs_struct
{
    struct input0_bit_struct input0;
    struct input1_bit_struct input1;
};

union digital_inputs_union
{
    uint16_t digital_inputs_word;
    struct digital_inputs_struct digital_inputs;
};

#define BUTTON_PRESSED 0        //0 means pressed, 1 means unpressed
#define IS_PRESSED(btn) (btn == BUTTON_PRESSED)


static unsigned char rdesc[] = {
    0x05, 0x01, //; USAGE_PAGE (Generic Desktop)
    0x09, 0x05, //; USAGE (Gamepad)
    0xA1, 0x01, //; COLLECTION (Application)
    0x05, 0x09,// ; USAGE_PAGE (Button)
    0x19, 0x01, //; USAGE_MINIMUM (Button 1)
    0x29, 0x0B, //; USAGE_MAXIMUM (Button 11)
    0x15, 0x00, //; LOGICAL_MINIMUM (0)
    0x25, 0x01, //; LOGICAL_MAXIMUM (1)
    0x75, 0x01, //; REPORT_SIZE (1)
    0x95, 0x10, //; REPORT_COUNT (16)
    0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    
        0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
        0x15, 0xFF,  // Logical Minimum (-1)
        0x25, 0x01,              //     LOGICAL_MAXIMUM (1)
        0x09, 0x30,  // Usage (X)
        0x09, 0x31,  // Usage (Y)
        0x75, 0x08,  // Report Size (8)
        0x95, 0x02,  // Report Count (2)
        0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
 0xC0,// ; END_COLLECTION
};

static int uhid_write(int fd, const struct uhid_event *ev)
{
	ssize_t ret;

	ret = write(fd, ev, sizeof(*ev));
	if (ret < 0) {
		fprintf(stderr, "Cannot write to uhid: %m\n");
		return -errno;
	} else if (ret != sizeof(*ev)) {
		fprintf(stderr, "Wrong size written to uhid: %zd != %zu\n",
			ret, sizeof(ev));
		return -EFAULT;
	} else {
		return 0;
	}
}

static int create(int fd)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_CREATE;
	strcpy((char*)ev.u.create.name, "Freeplay Gamepad");
	ev.u.create.rd_data = rdesc;
	ev.u.create.rd_size = sizeof(rdesc);
	ev.u.create.bus = BUS_USB;
	ev.u.create.vendor = 0x15d9;
	ev.u.create.product = 0x0a37;
	ev.u.create.version = 0;
	ev.u.create.country = 0;

	return uhid_write(fd, &ev);
}

static void destroy(int fd)
{
	struct uhid_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_DESTROY;

	uhid_write(fd, &ev);
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
	fprintf(stderr, "LED output report received with flags %x\n",
		ev->u.output.data[1]);
}

static int event(int fd)
{
	struct uhid_event ev;
	ssize_t ret;

	memset(&ev, 0, sizeof(ev));
	ret = read(fd, &ev, sizeof(ev));
	if (ret == 0) {
		fprintf(stderr, "Read HUP on uhid-cdev\n");
		return -EFAULT;
	} else if (ret < 0) {
		fprintf(stderr, "Cannot read uhid-cdev: %m\n");
		return -errno;
	} else if (ret != sizeof(ev)) {
		fprintf(stderr, "Invalid size read from uhid-dev: %zd != %zu\n",
			ret, sizeof(ev));
		return -EFAULT;
	}

	switch (ev.type) {
	case UHID_START:
		fprintf(stderr, "UHID_START from uhid-dev\n");
		break;
	case UHID_STOP:
		fprintf(stderr, "UHID_STOP from uhid-dev\n");
		break;
	case UHID_OPEN:
		fprintf(stderr, "UHID_OPEN from uhid-dev\n");
		break;
	case UHID_CLOSE:
		fprintf(stderr, "UHID_CLOSE from uhid-dev\n");
		break;
	case UHID_OUTPUT:
		fprintf(stderr, "UHID_OUTPUT from uhid-dev\n");
		handle_output(&ev);
		break;
	case UHID_OUTPUT_EV:
		fprintf(stderr, "UHID_OUTPUT_EV from uhid-dev\n");
		break;
	default:
		fprintf(stderr, "Invalid event from uhid-dev: %u\n", ev.type);
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
    unsigned char buttons11to8;
    int8_t hat_x;
    int8_t hat_y;
} gamepad_report, gamepad_report_prev;


static int send_event(int fd)
{
	struct uhid_event ev;
//printf("In send_event\n");

	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = 4;
	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons11to8;
	ev.u.input.data[2] = (unsigned char) gamepad_report.hat_x;
	ev.u.input.data[3] = (unsigned char) gamepad_report.hat_y;

	return uhid_write(fd, &ev);
}


void i2c_open()
{
	i2c_file = open(I2C_BUSNAME, O_RDWR);
	if (i2c_file < 0) {
	  /* ERROR HANDLING; you can check errno to see what went wrong */
	  exit(1);
	}

	if (ioctl(i2c_file, I2C_SLAVE, I2C_ADDRESS) < 0) {
	  /* ERROR HANDLING; you can check errno to see what went wrong */
	  exit(1);
	}
}



void i2c_poll_joystick()
{
	int ret;

//printf("i2c_poll_joystick: reading i2c\n");
	ret = i2c_smbus_read_word_data(i2c_file, 0);
	if(ret < 0)
	{
		printf("i2c_poll_joystick: exiting (ret=%d)\n", ret);
		exit(1);
	}
    
    union digital_inputs_union digital_inputs;
    digital_inputs.digital_inputs_word = ret;
    
    //printf("i2c_poll_joystick: u16=0x%02X input0=0x%02X input1=0x%02X btn_a=%d\n", digital_inputs.digital_inputs_word, digital_inputs.digital_inputs.input0, digital_inputs.digital_inputs.input1, digital_inputs.digital_inputs.input0.btn_a);


    gamepad_report.buttons7to0 = (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_tr) << 7)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_tl) << 6)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_select) << 5)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_y) << 4)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_x) << 3)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input1.btn_mode) << 2)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_b) << 1)
                                | IS_PRESSED(digital_inputs.digital_inputs.input0.btn_a);
    gamepad_report.buttons11to8 = (IS_PRESSED(digital_inputs.digital_inputs.input0.btn_start) << 2)
                                | (IS_PRESSED(digital_inputs.digital_inputs.input1.btn_tr2) << 1)
                                | IS_PRESSED(digital_inputs.digital_inputs.input1.btn_tl2);


    gamepad_report.hat_x = IS_PRESSED(digital_inputs.digital_inputs.input1.dpad_r) - IS_PRESSED(digital_inputs.digital_inputs.input1.dpad_l);
    gamepad_report.hat_y = IS_PRESSED(digital_inputs.digital_inputs.input1.dpad_u) - IS_PRESSED(digital_inputs.digital_inputs.input1.dpad_d);
}


#ifdef USE_PIGPIO_IRQ
#error Somewhat untested code here, use at your own risk
void gpio_callback(int gpio, int level, uint32_t tick) {
    printf("GPIO %d ",gpio);
    switch (level) {
        case 0: printf("LOW\n");
                i2c_poll_joystick();
                gamepad_report_prev = gamepad_report;
                send_event(fd);
                break;
        case 1: printf("HIGH\n");
                break;
        case 2: printf("WATCHDOG\n");
                break;
    }
}
#endif

#ifdef USE_WIRINGPI_IRQ
void attiny_irq_handler(void)
{
    i2c_poll_joystick();
    gamepad_report_prev = gamepad_report;
    send_event(fd);
}
#endif

int main(int argc, char **argv)
{
	const char *path = "/dev/uhid";
	struct pollfd pfds[2];
	int ret;
	struct termios state;

	ret = tcgetattr(STDIN_FILENO, &state);
	if (ret) {
		fprintf(stderr, "Cannot get tty state\n");
	} else {
		state.c_lflag &= ~ICANON;
		state.c_cc[VMIN] = 1;
		ret = tcsetattr(STDIN_FILENO, TCSANOW, &state);
		if (ret)
			fprintf(stderr, "Cannot set tty state\n");
	}

	if (argc >= 2) {
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			fprintf(stderr, "Usage: %s [%s]\n", argv[0], path);
			return EXIT_SUCCESS;
		} else {
			path = argv[1];
		}
	}

	fprintf(stderr, "Open uhid-cdev %s\n", path);
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		fprintf(stderr, "Cannot open uhid-cdev %s: %m\n", path);
		return EXIT_FAILURE;
	}

	fprintf(stderr, "Create uhid device\n");
	ret = create(fd);
	if (ret) {
		close(fd);
		return EXIT_FAILURE;
	}

	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;
	pfds[1].fd = fd;
	pfds[1].events = POLLIN;

	i2c_open();
    
    fprintf(stderr, "Press '^C' to quit...\n");

   
#ifdef USE_WIRINGPI_IRQ
	printf("Using WiringPi interrupts on GPIO %d\n", nINT_GPIO);
    wiringPiSetupGpio();        //use BCM numbering
    wiringPiISR(nINT_GPIO, INT_EDGE_FALLING, &attiny_irq_handler);
    i2c_poll_joystick();//make sure it's cleared out
    gamepad_report_prev = gamepad_report;
    send_event(fd);
    while(1)
    {
        
    }
#endif
    
#ifdef USE_PIGPIO_IRQ
 {
        int ver;
        int err;
        
        if ((ver =gpioInitialise()) > 0)
            printf("pigpio version: %d\n",ver);
        else
            exit(1);

        // sets the GPIO to input
        if ((err = gpioSetMode(nINT_GPIO,PI_INPUT)) != 0)
            exit(1);

        // sets the pull up to high so I can ground it with the push button
        if ((err = gpioSetPullUpDown(nINT_GPIO,PI_PUD_UP)) != 0)
            exit(1);

        // this is here because I'm testing with a push button that will bounce
        if ((err = gpioGlitchFilter(nINT_GPIO,100)) != 0)
            exit(1);

        // set the call back function on the GPIO level change
        #error Investigate why this wont work with nINT_GPIO set to 40!
        if ((err = gpioSetAlertFunc(nINT_GPIO,gpio_callback)) != 0)
            exit(1);

        // sleep indefinitely
        while (1)
            time_sleep(0.1);
    }
#endif
    
	while (1) {
		i2c_poll_joystick();

		if(memcmp(&gamepad_report, &gamepad_report_prev, sizeof(gamepad_report)) != 0)
		{
			gamepad_report_prev = gamepad_report;
			send_event(fd);
		}
	}

	fprintf(stderr, "Destroy uhid device\n");
	destroy(fd);
	return EXIT_SUCCESS;
}
