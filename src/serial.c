/* 
 * Author: Daniel Maxime (maxux.unix@gmail.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <getopt.h>
#include "serial_status.h"
#include "serial_parser.h"
#include "serial_at.h"
#include "serial.h"
#include "database.h"
#include "pdu.h"
#include "pending.h"

static struct option long_options[] = {
	{"raw",    required_argument, 0, 'r'},
	{"device", required_argument, 0, 'd'},
	{"replay", no_argument,       0, 'p'},
	{"help",   no_argument,       0, 'h'},
	{0, 0, 0, 0}
};

#define PDU_MODE

modem_3g_t __device = {
	.name = DEFAULT_DEVICE
};

//
// error handling
//
void diep(char *str) {
	fprintf(stderr, "[-] %s: [%d] %s\n", str, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

void dier(char *str) {
	fprintf(stderr, "[-] %s\n", str);
	exit(EXIT_FAILURE);
}

//
// device setter
//
int set_interface_attribs(int fd, int speed) {
	struct termios tty;
	
	memset(&tty, 0, sizeof(tty));
	
	if(tcgetattr(fd, &tty) != 0)
		diep("tcgetattr");

	// cfsetospeed(&tty, speed);
	// cfsetispeed(&tty, speed);
	
	tty.c_cflag = speed | CRTSCTS | CS8 | CLOCAL | CREAD;

	// tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	
	tty.c_iflag  = IGNPAR | ICRNL;
	tty.c_lflag  = ICANON;
	tty.c_oflag  = 0;
	tty.c_cc[VMIN]  = 1;
	tty.c_cc[VTIME] = 0;

/*
	// shut off xon/xoff ctrl
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls, enable reading
	tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
*/

	if(tcsetattr(fd, TCSANOW, &tty) != 0)
		diep("tcgetattr");
	
	return 0;
}

//
// device i/o
//
char *readfd(char *buffer, size_t length, int timeout) {
	int res, saved = 0;
	fd_set readfs;
	int selval;
	struct timeval tv, *ptv;
	char *temp;
	
	FD_ZERO(&readfs);
	
	while(1) {
		FD_SET(__device.fd, &readfs);
		
		tv.tv_sec  = 2;
		tv.tv_usec = 0;
		
		ptv = (timeout) ? &tv : NULL;
		
		if((selval = select(__device.fd + 1, &readfs, NULL, NULL, ptv)) < 0)
			diep("select");
		
		if(FD_ISSET(__device.fd, &readfs)) {		
			if((res = read(__device.fd, buffer + saved, length - saved)) < 0)
				diep(__device.name);
				
			buffer[res + saved] = '\0';
			
			// line/block is maybe not completed, waiting for a full line/block
			if(buffer[res + saved - 1] != '\n' && *buffer != '>') {
				saved = res;
				continue;
			}
			
			buffer[res + saved - 1] = '\0';
			
			for(temp = buffer; *temp == '\n'; temp++);
			memmove(buffer, temp, strlen(temp));
			
			if(!*buffer)
				continue;
			
			printf("[+] >> %s\n", buffer);
			
		} else {
			pending_check();
			unread_check();
			continue;
		}
		
		return buffer;
	}
}

int writefdraw(char *message) {
	int value;
	size_t length = strlen(message);

	printf("[+] << %s\n", message);
	
	if((value = write(__device.fd, message, length)) < 0)
		diep(__device.name);
	
	usleep(50000);
	
	return value;
}

int writefd(char *message) {
	size_t length;
	char *temp;
	
	length = strlen(message) + 4;
	
	if(!(temp = (char *) malloc(sizeof(char) * length)))
		diep("malloc");
	
	// building message with CRCRLF on suffix
	strcpy(temp, message);
	strcat(temp, "\r\r\n");
	
	// printf("[+] << %s\n", message);
	
	return writefdraw(temp);
}

int pdebug(int id, int replay) {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *pdu;
	pdu_t *data;
	
	sqlquery = sqlite3_mprintf("SELECT pdu FROM raw WHERE id = %d", id);
	
	if(!(stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		fprintf(stderr, "[-] sqlite: cannot select pending\n");
		exit(EXIT_FAILURE);
	}
		
	while(sqlite3_step(stmt) == SQLITE_ROW) {
		pdu = (char *) sqlite3_column_text(stmt, 0);
		printf("-> %s\n", pdu);
		
		if(!(data = pdu_receive(pdu))) {
			printf("PDU FAILED\n");
			return 1;
		}
		
		printf("%s >> %s\n", data->number, data->message);
		
		if(replay)
			segment_add(data);
	}

	sqlite3_free(sqlquery);
	sqlite3_finalize(stmt);
	
	return 0;
}

void print_usage(char *app) {
	(void) app;
	
	printf("serialinfo command line\n\n");
	
	printf(" --raw <pdu-raw-id>   only decode pdu raw-id from database\n");
	printf(" --replay             replay database save when used with --raw\n");
	printf(" --device <device>    serial device path\n");
	printf(" --help               print this message\n");
	
	exit(EXIT_FAILURE);
}

void init(char *device, int debug) {
	__device.name = device;
	
	printf("[+] init: opening database %s\n", SQL_DATABASE_FILE);
	sqlite_db = db_sqlite_init();
	
	if(debug) {
		printf("[+] entering debug mode\n");
		return;
	}
	
	printf("[+] init: opening device %s\n", __device.name);

	if((__device.fd = open(__device.name, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0)
		diep(__device.name);

	set_interface_attribs(__device.fd, DEFAULT_BAUDRATE);
	
	at_commit();

	if(!at_echo(0))
		dier("init: cannot disable echo");
	
	if(!at_curc(0))
		dier("init: cannot disable current status");
	
	//
	// AT+CPIN
	// CNMI MT 0 -> Poll reading
	// PDU 0x19 UCS2
	//
	
	
	// setting text mode
	#ifdef TEXT_MODE
	if(!at_cmgf(1))
		dier("init: cannot set text mode");
	#endif
	#ifdef PDU_MODE
	if(!at_cmgf(0))
		dier("init: cannot set pdu mode");
	#endif
	
	// setting storage
	if(!at_cpms("ME", "ME", "ME"))
		dier("init: cannot set storage mode");
	
	// setting notification mode
	if(!at_cnmi(1, 1, 0, 2, 1))
		dier("init: cannot set notification mode");
	
	// request signal quality
	at_csq();
}

int main(int argc, char *argv[]) {
	char buffer[2048];
	char *device = DEFAULT_DEVICE;
	int pdu = 0, replay = 0;
	int i, option_index = 0;

	while(1) {
		i = getopt_long(argc, argv, "r:d:ph", long_options, &option_index);

		if(i == -1)
			break;

		switch(i) {
			case 'r':
				pdu = atoi(optarg);
				break;
				
			case 'd':
				device = strdup(optarg);
				break;
				
			case 'p':
				replay = 1;
				break;
			
			case 'h':
			case '?':
				print_usage(argv[0]);
				return 1;
			break;

			default:
				abort();
		}
	}
	
	init(device, pdu);
	
	// debug pdu
	if(pdu)
		return pdebug(pdu, replay);

	// infinite loop on messages
	while(1) {
		readfd(buffer, sizeof(buffer), 1);
		parse(buffer);
	}
	
	return 0;
}
