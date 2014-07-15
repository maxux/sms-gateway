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
#include "serial_status.h"
#include "serial_parser.h"
#include "serial_at.h"
#include "serial.h"
#include "database.h"
#include "pending.h"
#include "pdu.h"

#define PDU_MODE

modem_3g_t __device = {
	.name = DEFAULT_DEVICE
};

//
// error handling
//
void diep(char *str) {
	fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
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
	int parity = 0;
	
	memset(&tty, 0, sizeof(tty));
	
	if(tcgetattr(fd, &tty) != 0)
		diep("tcgetattr");

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	
	tty.c_iflag &= ~IGNBRK | ~ICRNL;      // ignore break signal
	tty.c_lflag  = ICANON;
	tty.c_oflag  = 0;
	tty.c_cc[VMIN]  = 1;
	tty.c_cc[VTIME] = 10;

	// shut off xon/xoff ctrl
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls, enable reading
	tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if(tcsetattr(fd, TCSANOW, &tty) != 0)
		diep("tcgetattr");
	
	return 0;
}

//
// device i/o
//
char *readfd(char *buffer, size_t length) {
	int res, saved = 0;
	
	while(1) {
		if((res = read(__device.fd, buffer + saved, length - saved)) < 0)
			diep(__device.name);
			
		buffer[res + saved] = '\0';
		
		// line/block is maybe not completed, waiting for a full line/block
		if(buffer[res + saved - 1] != '\n') {
			saved = res;
			continue;
			
		} else saved = 0;
		
		// printf("[+] >> %s\n", buffer);
		
		return buffer;
	}
}

int writefdraw(char *message) {
	int value;
	size_t length = strlen(message);

	printf("[+] << %s\n", message);
	
	if((value = write(__device.fd, message, length)) < 0)
		diep(__device.name);
	
	usleep(10000);
	
	return value;
}

int writefd(char *message) {
	size_t length;
	char *temp;
	
	length = strlen(message) + 3;
	
	if(!(temp = (char *) malloc(sizeof(char) * length)))
		diep("malloc");
	
	// building message with CRCRLF on suffix
	strcpy(temp, message);
	strcat(temp, "\r\r\n\n");
	
	printf("[+] << %s\n", message);
	
	return writefdraw(temp);
}

int main(int argc, char *argv[]) {
	char buffer[2048];
	
	// setting device name from stdin if set
	if(argc > 1)
		__device.name = argv[1];
	
	printf("[+] init: opening database %s\n", SQL_DATABASE_FILE);
	sqlite_db = db_sqlite_init();
	
	printf("[+] init: opening device %s\n", __device.name);

	if((__device.fd = open(__device.name, O_RDWR | O_NOCTTY | O_SYNC)) < 0)
		diep(__device.name);

	set_interface_attribs(__device.fd, DEFAULT_BAUDRATE);
	
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

	// infinite loop on messages
	while(1) {
		readfd(buffer, sizeof(buffer));
		if(parse(buffer) == PARSE_STATUS) {
			pending_check();
		}
	}
	
	return 0;
}
