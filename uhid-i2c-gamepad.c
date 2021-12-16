/*
 Setup:     sudo apt install libi2c-dev
 Compile:   gcc -o uhid-i2c-gamepad uhid-i2c-gamepad.c -li2c
 Run:       sudo ./uhid-i2c-gamepad 
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

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

#define I2C_BUSNAME "/dev/i2c-1"
#define I2C_ADDRESS 0x20
int i2c_file = -1;

static unsigned char rdesc[] = {
 0x05, 0x01, //; USAGE_PAGE (Generic Desktop)
 0x09, 0x05, //; USAGE (Gamepad)
 0xA1, 0x01, //; COLLECTION (Application)
 0x05, 0x09,// ; USAGE_PAGE (Button)
 0x19, 0x01, //; USAGE_MINIMUM (Button 1)
 0x29, 0x10, //; USAGE_MAXIMUM (Button 16)
 0x15, 0x00, //; LOGICAL_MINIMUM (0)
 0x25, 0x01, //; LOGICAL_MAXIMUM (1)
 0x75, 0x01, //; REPORT_SIZE (1)
 0x95, 0x10, //; REPORT_COUNT (16)

 0x81, 0x02,  // Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
    0x15, 0x00,  // Logical Minimum (-127)
    //0x25, 0x7F,  // Logical Maximum (127)
    0x26, 0xff, 0x00,              //     LOGICAL_MAXIMUM (0xFF)
    0x09, 0x30,  // Usage (X)
    0x09, 0x31,  // Usage (Y)
    0x09, 0x32,  // Usage (Z)
    0x09, 0x35,  // Usage (Rz)
    0x75, 0x08,  // Report Size (8)
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
	strcpy((char*)ev.u.create.name, "test-uhid-gamepad");
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
    unsigned char buttons15to8;
    int8_t left_x;
    int8_t left_y;
    int8_t right_x;
    int8_t right_y;
} gamepad_report, gamepad_report_prev;


static int send_event(int fd)
{
	struct uhid_event ev;


	memset(&ev, 0, sizeof(ev));
	ev.type = UHID_INPUT;
	ev.u.input.size = 6;

	ev.u.input.data[0] = gamepad_report.buttons7to0;
	ev.u.input.data[1] = gamepad_report.buttons15to8;

	ev.u.input.data[2] = gamepad_report.left_x;
	ev.u.input.data[3] = gamepad_report.left_y;
	ev.u.input.data[4] = gamepad_report.right_x;
	ev.u.input.data[5] = gamepad_report.right_y;

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

	ret = i2c_smbus_read_word_data(i2c_file, 0);
	if(ret < 0)
		exit(1);

	gamepad_report.buttons7to0 = ~(ret & 0xFF);
	gamepad_report.buttons15to8 = ~(ret >> 8 & 0xFF);
	//printf("gamepad_report.buttons7to0 = 0x%02X gamepad_report.buttons15to8 = 0x%02X\n", gamepad_report.buttons7to0, gamepad_report.buttons15to8);

        ret = i2c_smbus_read_word_data(i2c_file, 8);
        if(ret < 0)
                exit(1);

        //printf("left_x=%d ", ret);
	ret = ret >> 2; //0x3FF -> 0xFF
	//ret = ret - 127; //0-255  ->  -127-127
        //printf("left_x=%d\n", ret);
	gamepad_report.left_x = ret;

        ret = i2c_smbus_read_word_data(i2c_file, 10);
        if(ret < 0)
                exit(1);

	ret = ret >> 2; //0x3FF -> 0xFF
	//ret = ret - 127; //0-255  ->  -127-127
	gamepad_report.left_y = ret;
        //printf("ret = 0x%04X ", ret);

	//printf("\n");
}

int main(int argc, char **argv)
{
	int fd;
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
	while (1) {
		i2c_poll_joystick();

		//if(gamepad_report.buttons7to0 != gamepad_report_prev.buttons7to0 || gamepad_report.buttons15to8 != gamepad_report_prev.buttons15to8)
		{
			gamepad_report_prev = gamepad_report;
			send_event(fd);
		}
	}

	fprintf(stderr, "Destroy uhid device\n");
	destroy(fd);
	return EXIT_SUCCESS;
}
