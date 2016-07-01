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
#include <inttypes.h>
#include <unistd.h>
#include <sys/wait.h>
#include "serial_misc.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial_at.h"
#include "serial.h"
#include "pdu.h"
#include "pending.h"

int csq_current = 0;
int x = 0;

int checkok() {
	char buffer[512];
	int value = 0;
	
	// loop while we got a status reply (we can process it later)
	while((value = parse(readfd(buffer, sizeof(buffer), 0)))) {
		if(value != PARSE_EMPTY && value != PARSE_UNKNOWN)
			return value;
	}
	
	return 0;
}


int handler_notify(char *buffer) {
	int index = 0;
	char temp[512];
	
	// jump to message id directly
	index = atoi(buffer + 12);
	printf("[+] message id: %d\n", index);
	
	snprintf(temp, sizeof(temp), "AT+CMGR=%d", index);
	writefd(temp);
	
	return 1;
}

int handler_inbox(char *buffer) {
	char newread[1024], *pdu;
	int index;
	(void) buffer;
	
	// read pdu, but ignore it and request to read the message
	readfd(newread, sizeof(newread), 0);
	pdu = strcleaner(strdup(newread));
	
	printf("[+] saving raw pdu\n");
	pdu_add(pdu);
	
	// jump to message id directly
	index = atoi(buffer + 7);
	printf("[+] message id: %d\n", index);
	
	// request to read that message
	snprintf(newread, sizeof(newread), "AT+CMGR=%d", index);
	writefd(newread);
	
	return 1;
}

int handler_sms_content(char *buffer) {
	char *pdu, newread[1024];
	pdu_t *data;
	(void) buffer;

	// grab message
	// FIXME
	readfd(newread, sizeof(newread), 0);
	pdu = strcleaner(strdup(newread));
	
	printf("[+] saving raw pdu\n");
	pdu_add(pdu);

	if(!(data = pdu_receive(pdu))) {
		fprintf(stderr, "[-] cannot decode sms pdu\n");
		failed_add(pdu);
		return 0;
	}
	
	printf("[+] message from <%s>: <%s>\n", data->number, data->message);
	
	segment_add(data);
	
	// FIXME: free ...
	free(data);
	
	// deleting (all read) messages
	at_cmgd(0, 1);
	
	return 1;
}

int parse(char *buffer) {
	char temp[128];
	
	// buffer = strcleaner(buffer);
	// printf("[-] debug: <%s>\n", buffer);
	if(!strcmp(buffer, "OK")) {
		printf("[+] parser: ok from device\n");
		return PARSE_OK;
	}
	
	//
	// when successful message arrives
	//
	if(*buffer == '+') {
		if(!strncmp(buffer, "+CMTI:", 6)) {
			printf("[+] parser: we got a new message\n");
			return handler_notify(buffer);
		}
		
		if(!strncmp(buffer, "+CMGR:", 6)) {
			printf("[+] parser: we got a message content\n");
			return handler_sms_content(buffer);
		}
		
		if(!strncmp(buffer, "+CMGS:", 6)) {
			printf("[+] parser: message sent\n");
			return PARSE_OK;
		}
		
		if(!strncmp(buffer, "+CPMS:", 6)) {
			printf("[+] parser: storage settings set\n");
			return PARSE_OK;
		}
		
		if(!strncmp(buffer, "+CMGL:", 6)) {
			printf("[+] parser: message from memory request\n");
			return handler_inbox(buffer);
		}
		
		
		if(!strncmp(buffer, "+CMS ERROR:", 11)) {
			printf("[+] parser: error code\n");
			return PARSE_FAIL;
		}
		
		
		// link quality status
		if(!strncmp(buffer, "+CSQ:", 5)) {
			__device.link = modem_rssi(__device.link, buffer + 6);
			return PARSE_OK;
		}
	}
	
	//
	// when a status message arrive
	//
	if(*buffer == '^') {
		// device flow repport status
		if(!strncmp(buffer, "^DSFLOWRPT", 10)) {
			__device.link = modem_dsflowrpt(__device.link, buffer + 11);
			
			printf(
				"[%s] link: %d dBm (%.1f%%), bw: ↓% 5d kbps, "
				"↑% 5d kbps, data: ↓% 3.2f Mo, ↑% 3.2f Mo\n",
				strduration(__device.link.duration, temp),
				__device.link.dbm,
				__device.link.dbmpc,
				__device.link.rxspeed * 8,
				__device.link.txspeed * 8,
				(__device.link.rxtotal / 1024 / (double) 1024),
				(__device.link.txtotal / 1024 / (double) 1024)
			);
			
			// delayed pending
			pending_check();
		}
		
		// current changed
		if(!strncmp(buffer, "^MODE", 5)) {
			__device.link.mode = modem_mode(buffer + 6);
		}

		// any others repport not parsed
		if(*buffer == '^') {
			// TODO
			if(!csq_current || (csq_current % 42) == 0) {
				at_commit();
				at_single();
				at_csq();
			}
			
			csq_current++;
		}
		
		return PARSE_STATUS;
	}
	
	//
	// something else (error, echo, ...) arrives
	//	
	if(!strncmp(buffer, "COMMAND NOT SUPPORT", 19)) {
		fprintf(stderr, "[-] error: command not supported !\n");
		return PARSE_FAIL;
	}
	
	printf("[-] parser: unknown: <%s>\n", buffer);
	
	return PARSE_UNKNOWN;
}
