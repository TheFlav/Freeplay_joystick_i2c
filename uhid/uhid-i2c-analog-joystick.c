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

int fd;

#define I2C_BUSNAME "/dev/i2c-1"
#define I2C_ADDRESS 0x20
int i2c_file = -1;


static unsigned char rdesc[] =
{
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
	strcpy((char*)ev.u.create.name, "Freeplay Analog Joy");
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


/*
 * input0
 * PA1 = IO0_0 = UP
 * PA2 = IO0_1 = DOWN
 * PB4 = IO0_2 = LEFT
 * PB5 = IO0_3 = RIGHT
 * PB6 = IO0_4 = BTN_A
 * PB7 = IO0_5 = BTN_B
 * PA6 = IO0_6 = BTN_C ifndef USE_ADC2
 * PA7 = IO0_7 = BTN_Z ifndef USE_ADC3
 *
 * input1
 * PC0 = IO1_0 = BTN_X
 * PC1 = IO1_1 = BTN_Y
 * PC2 = IO1_2 = BTN_START
 * PC3 = IO1_3 = BTN_SELECT
 * PC4 = IO1_4 = BTN_L
 * PC5 = IO1_5 = BTN_R
 * PB2 = IO1_6 = BTN_L2 ifndef CONFIG_SERIAL_DEBUG (or can be used for UART TXD0 for debugging)  [MAY NEED TO BE PWM output!!!!]
 * PB3 = IO1_7 = BTN_R2 ifndef CONFIG_SERIAL_DEBUG (or can be used for UART RXD0 for debugging)
 */

struct i2c_register_struct {
  uint8_t input0;          // Reg: 0x00 - INPUT port 0 (digital buttons/dpad)
  uint8_t input1;          // Reg: 0x01 - INPUT port 1 (digital buttons/dpad)
  uint8_t a0_msb;          // Reg: 0x02 - ADC0 most significant 8 bits
  uint8_t a1_msb;          // Reg: 0x03 - ADC1 most significant 8 bits
  uint8_t a1a0_lsb;        // Reg: 0x04 - high nibble is a1 least significant 4 bits, low nibble is a0 least significant 4 bits
  uint8_t a2_msb;          // Reg: 0x05 - ADC2 most significant 8 bits
  uint8_t a3_msb;          // Reg: 0x06 - ADC2 most significant 8 bits
  uint8_t a3a2_lsb;        // Reg: 0x07 - high nibble is a3 least significant 4 bits, low nibble is a2 least significant 4 bits
  uint8_t adc_on_bits;     // Reg: 0x08 - turn ON bits here to activate ADC0 - ADC3 (only works if the USE_ADC# are turned on)
  uint8_t config0;         // Reg: 0x09 - Configuration port 0
  uint8_t adc_res;         // Reg: 0x0A - current ADC resolution (maybe settable?)
} i2c_registers;

struct gamepad_report_t
{
    int8_t hat_x;
    int8_t hat_y;
    uint8_t buttons7to0;
    uint8_t buttons12to8;
    uint16_t left_x;
    uint16_t left_y;
    uint16_t right_x;
    uint16_t right_y;
} gamepad_report, gamepad_report_prev;


static int send_event(int fd)
{
	struct uhid_event ev;


	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
    
	ev.u.input.size = 12;
    
	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons12to8;

    ev.u.input.data[2] = (unsigned char) gamepad_report.hat_x;
	ev.u.input.data[3] = (unsigned char) gamepad_report.hat_y;
    
    //printf("i2c_registers.input0=0x%02X hat_x=%d hat_x=%d d[2]=0x%02X d[3]=0x%02X\n", i2c_registers.input0, gamepad_report.hat_x, gamepad_report.hat_y, ev.u.input.data[2], ev.u.input.data[3]);

    
    ev.u.input.data[4] = gamepad_report.left_x & 0xFF;
    ev.u.input.data[5] = gamepad_report.left_x >> 8;
    ev.u.input.data[6] = gamepad_report.left_y & 0xFF;
    ev.u.input.data[7] = gamepad_report.left_y >> 8;
    ev.u.input.data[8] = gamepad_report.right_x & 0xFF;
    ev.u.input.data[9] = gamepad_report.right_x >> 8;
    ev.u.input.data[10] = gamepad_report.right_y & 0xFF;
    ev.u.input.data[11] = gamepad_report.right_y >> 8;

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
    
    bool btn_a, btn_b, btn_c, btn_x, btn_y, btn_z, btn_l, btn_r, btn_start, btn_select, btn_l2, btn_r2;
    
    uint8_t dpad_bits;
	
	
    int8_t dpad_u, dpad_d, dpad_l, dpad_r;
	unsigned char buf[32];
	int ret;

        memset(&buf, 0, sizeof(32));


	/* Using SMBus commands */
	//res = i2c_smbus_read_word_data(i2c_file, reg);
	//if (res < 0) {
	//  /* ERROR HANDLING: I2C transaction failed */
	//} else {
	//  /* res contains the read word */
	//}


//	ret = i2c_smbus_read_block_data(i2c_file, 0, buf);

	ret = i2c_smbus_read_i2c_block_data(i2c_file, 0, 10 /*sizeof(i2c_registers)*/, (uint8_t *)&i2c_registers);
	if(ret < 0)
		exit(1);



	ret = i2c_registers.input1 << 8 | i2c_registers.input0;
	
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
	
	
	gamepad_report.left_x = i2c_registers.a0_msb << 8 | ((i2c_registers.a1a0_lsb & 0x0F) << 4);
	gamepad_report.left_y = i2c_registers.a1_msb << 8 | (i2c_registers.a1a0_lsb & 0xF0);

	gamepad_report.right_x = i2c_registers.a2_msb << 8 | ((i2c_registers.a3a2_lsb & 0x0F) << 4);
	gamepad_report.right_y = i2c_registers.a3_msb << 8 | (i2c_registers.a3a2_lsb & 0xF0);
}





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
    
    i2c_smbus_write_byte_data(i2c_file, 0x08, 0x0F);        //turn on ADC3,2,1,0 in adc_on_bits

    
    fprintf(stderr, "Press '^C' to quit...\n");


    
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





