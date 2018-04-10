/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <sys/param.h>
#include <sys/ioctl.h>

#ifdef BTDIAG_SUPPORT_BT_USB
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#endif

#include "connection.h"

#define FLOW_CTL	0x0001
#define ENABLE_PM	1
#define DISABLE_PM	0


#ifndef BTDIAG_SUPPORT_BT_USB
#define SOL_HCI 0
#define HCI_MAX_EVENT_SIZE 260
#define HCI_COMMAND_HDR_SIZE 3
#define HCI_EVENT_PKT 0x03
#define HCI_FILTER 2
typedef struct {
    unsigned char     evt;
    unsigned char     plen;
} __attribute__ ((packed))  hci_event_hdr;
 
int hci_devid(const char *str)
{
	//printf("hci_devid()\n");
	return -1;
}
int hci_get_route(bdaddr_t *bdaddr)
{
	//printf("hci_get_route()\n");
	return -1;
}
int hci_open_dev(int dev_id)
{
	//printf("hci_open_dev()\n");
	return -1;
}

int hci_close_dev(int dd)
{
	//printf("hci_close_dev()\n");
	return close(dd);
}
static inline void hci_filter_clear(struct hci_filter *f)
{
	//printf("hci_filter_clear()\n");
}
static inline void hci_filter_set_ptype(int t, struct hci_filter *f)
{
	//printf("hci_filter_set_ptype()\n");
}
static inline void hci_filter_all_events(struct hci_filter *f)
{
	//printf("hci_filter_all_events()\n");
}
int hci_send_cmd(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
	//printf("hci_send_cmd()\n");
	return -1;
}
#endif

struct uart_t {
	char *type;
	int  init_speed;
	int  speed;
	int  flags;
	char *bdaddr;
	int  (*init) (int fildes, struct uart_t *u, struct termios *ti);
};
	
static void sig_alarm(int sig)
{
	fprintf(stderr, "Initialization timed out.\n");
	exit(1);
}

unsigned char change_to_tcio_baud(int cfg_baud, int *baud)
{
	if (cfg_baud == 115200)
		*baud = B115200;
	else if (cfg_baud == 4000000)
		*baud = B4000000;
	else if (cfg_baud == 3000000)
		*baud = B3000000;
	else if (cfg_baud == 2000000)
		*baud = B2000000;
	else if (cfg_baud == 1000000)
		*baud = B1000000;
	else if (cfg_baud == 921600)
		*baud = B921600;
	else if (cfg_baud == 460800)
		*baud = B460800;
	else if (cfg_baud == 230400)
		*baud = B230400;
	else if (cfg_baud == 57600)
		*baud = B57600;
	else if (cfg_baud == 19200)
		*baud = B19200;
	else if (cfg_baud == 9600)
		*baud = B9600;
	else if (cfg_baud == 1200)
		*baud = B1200;
	else if (cfg_baud == 600)
		*baud = B600;
	else
	{
		printf( "unsupported baud: %i, use default baud value - B115200\n", cfg_baud);
		*baud = B115200;
		return 1;
	}

	return 0;
}

int set_baud_rate(int fildes, struct termios *ti, int speed)
{
	int tcio_baud;
	
	change_to_tcio_baud(speed, &tcio_baud);

	if (cfsetospeed(ti, tcio_baud) < 0)
		goto fail;

	if (cfsetispeed(ti, tcio_baud) < 0)
		goto fail;

	if (tcsetattr(fildes, TCSANOW, ti) < 0)
		goto fail;

	return 0;
	
fail:
    return -errno;
}

int read_hci_event(int fildes, unsigned char* databuf, int len)
{
	int received_bytes = 0;
	int remain, ret = 0;

	if (len <= 0)
		goto fail;

	while (1) {
	    ret = read(fildes, databuf, 1);
		
		if (ret <= 0)
			goto fail;
		else if (databuf[0] == 4)
			break;
	}
	received_bytes++;
	
	while (received_bytes < 3) {
		ret = read(fildes, databuf + received_bytes, 3 - received_bytes);
		if (ret <= 0)
		    goto fail;
			
		received_bytes += ret;
	}

	/* Now we read the parameters. */
	if (databuf[2] < (len - 3))
	    remain = databuf[2];
	else
		remain = len - 3;

	while ((received_bytes - 3) < remain) {
		ret = read(fildes, databuf + received_bytes, remain - (received_bytes - 3));
		
		if (ret <= 0)
			goto fail;
		received_bytes += ret;
	}

	return received_bytes;

fail:
    return -1;	
}

static int qca(int fildes, struct uart_t *u, struct termios *ti)
{
    fprintf(stderr,"qca\n");
    return qca_soc_init(fildes, u->speed, u->bdaddr);
}

struct uart_t uart[] = {
    /* QCA ROME */
    { "qca", 115200, 115200, FLOW_CTL, NULL, qca},
	{ NULL, 0 }
};

static struct uart_t * get_type_from_table(char *type)
{
	int i = 0;

	// get type from table
	while(uart[i].type)
	{
	    if (!strcmp(uart[i].type, type))
			return &uart[i];
	    i++;
	}
	
	return NULL;
}

void set_rtscts_flag(struct termios *ti, bool enable)
{
	if (enable)
		ti->c_cflag |= CRTSCTS;
	else
		ti->c_cflag &= ~CRTSCTS;
}

static int initport(char *dev, struct uart_t *u, int sendbreak)
{
	int fildes;
	struct termios term_attr;

	if ((fildes = open(dev, O_RDWR | O_NOCTTY)) == -1)
	{
		printf("Uart: Open serial port failed\n");
		return -1;
	}

	//Flush Terminal Input or Output
	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");
		
	if (tcgetattr(fildes, &term_attr) != 0) {
		printf("Uart: Get port settings failed\n");
		return -1;
	}

	cfmakeraw(&term_attr);
	
	term_attr.c_cflag |= CLOCAL;
	if (u->flags & FLOW_CTL)
	{
		// flow control via CTS/RTS
		set_rtscts_flag(&term_attr, 1);
	}
	else
		set_rtscts_flag(&term_attr, 0);
	
	if (tcsetattr(fildes, TCSANOW, &term_attr) != 0) {
		printf("Uart: Set port settings failed\n");
		return -1;
	}

	if (set_baud_rate(fildes, &term_attr, u->init_speed) < 0) {
		printf("Uart: Set initial baud rate failed\n");
		return -1;
	}
	
	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");
	
	if (sendbreak) {
		tcsendbreak(fildes, 0);
		usleep(400000);
	}

	if (u->init && u->init(fildes, u, &term_attr) < 0)
		return -1;

	if (tcflush(fildes, TCIOFLUSH) != 0)
		perror("tcflush error");

	return fildes;
}

/*
 *  The value of below parameters are referred to hciattach.c and hciattach_rome.c(BlueZ):
 *    (1)Device type is "qca"
 *    (2)Flow control is turn "ON" 
 *    (3)Waiting time is "120 sec"
 */
int SerialConnection(char *dev_path, char *baudrate)
{
	struct uart_t *u = NULL;
	int opt, i, n, ld, err;
	int to = 10;
	int init_speed = 0;
	int sendbreak = 0;
	pid_t pid;
	struct sigaction sa;
	struct pollfd p;
	sigset_t sigs;
	char dev[PATH_MAX];
	
	/* Set the parameters */
	strcpy(dev, dev_path);
    u = get_type_from_table("qca");  	
	u->speed = atoi(baudrate);    
	u->flags |=  FLOW_CTL;
    to = 120;

	if (!u) {
		fprintf(stderr, "Unknown device type or id\n");
		exit(-1);
	}

	/* If user specified a initial speed, use that instead of
	   the hardware's default */
	if (init_speed)
            u->init_speed = init_speed;

	if (u->speed)
            u->init_speed = u->speed;

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags   = SA_NOCLDSTOP;
	sa.sa_handler = sig_alarm;
	sigaction(SIGALRM, &sa, NULL);

	/* 10 seconds should be enough for initialization */
	alarm(to);

	n = initport(dev, u, sendbreak);
	if (n < 0) {
		perror("Can't initialize device");
		exit(-1);
	}
	
	printf("Download Patch firmware and NVM file completed\n");

	alarm(0);

	return n;
}

/* Get device descriptor */
int USBConnection(char *dev_name)
{
    int dd,dev_id;
	
    dev_id = hci_devid(dev_name);
	if (dev_id < 0) 
	{
		perror("Invalid device");
	    exit(-1);
	}
	
    if (dev_id < 0)
	 	dev_id = hci_get_route(NULL);	
	
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Device open failed");
	 	exit(-1);
	}
	
	return dd;
}

/*
 * Send an HCI command to (USB) device.
 *
 */
int usb_send_hci_cmd(char *dev_name, unsigned char *rawbuf, int rawdatalen)
{
    unsigned char buf[HCI_MAX_EVENT_SIZE];
	unsigned char *ptr = buf;
	unsigned char *param_ptr = NULL;
	struct hci_filter flt;
	hci_event_hdr *hdr;
	int i, opt, len, dd, dev_id;
	uint16_t ocf, opcode;
	uint8_t ogf;	
	
	dev_id = hci_devid(dev_name);
	if (dev_id < 0) 
	{
		perror("Invalid device");
		exit(EXIT_FAILURE);
	}
	
    if (dev_id < 0)
		dev_id = hci_get_route(NULL);	
	
	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Device open failed");
		exit(EXIT_FAILURE);
	}
		
	/* calculate OGF and OCF: ogf is bit 10~15 of opcode, ocf is bit 0~9 of opcode */
	errno = 0;	
	opcode = rawbuf[1] | (rawbuf[2] << 8);
	ogf = opcode >> 10;
	ocf = opcode & 0x3ff;
	
	if (errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff)) {
		printf("wrong input format\n");
		exit(EXIT_FAILURE);
	}
	
	/* copy parameter to buffer and calculate parameter total length */	
	param_ptr = rawbuf + (1 + HCI_COMMAND_HDR_SIZE);
	//parameter length = HCI raw packet length - HCI_PACKET_TYPE length - HCI_COMMAND_HDR length
	len = rawdatalen - (1 + HCI_COMMAND_HDR_SIZE);
	memcpy(buf, param_ptr, len);	
	
	/* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}

	if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}

	hci_close_dev(dd);
	return 0;
}

/*
 * Read an HCI raw data from the given device descriptor.
 *
 * return value: actual recived data length
 */
int read_raw_data(int dd, unsigned char *buf, int size)
{
    struct hci_filter flt;
	int ret;
	
    /* Setup filter */
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_all_events(&flt);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("HCI filter setup failed");
		exit(EXIT_FAILURE);
	}
	
	ret = read(dd, buf, size);
	return ret;
}
