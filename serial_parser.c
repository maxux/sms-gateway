#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include "serial_misc.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial_at.h"
#include "serial.h"

char *run(char *cmd) {
	FILE *pipe;
	char *output, buffer[2048];
	size_t length = 0;
	
	if(!(pipe = popen(cmd, "r")))
		return NULL;
	
	output = (char *) calloc(1, sizeof(char));
	
	while(fgets(buffer, sizeof(buffer), pipe)) {
		length += strlen(buffer) + 1;
		output = (char *) realloc(output, length * sizeof(char));
		
		strcat(output, buffer);
	}
	
	fclose(pipe);
	
	return output;
}



int checkok() {
	char buffer[512];
	
	// loop while we got a status reply (we can process it later)
	while(parse(readfd(buffer, sizeof(buffer))) == PARSE_STATUS);
	
	return (parse(buffer) == PARSE_OK);
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

int handler_sms_content(char *buffer) {
	char *phone = NULL, *message = NULL;
	char output[2048], *sender;
	
	// grab phone origin
	if(!(phone = at_cmgr_getphone(buffer)))
		return 0;
	
	// grab message
	if(!(message = at_cmgr_getmessage(buffer))) {
		free(phone);
		return 0;
	}
	
	printf("[+] replying to: <%s>\n", phone);
	printf("[+] message    : <%s>\n", message);
	
	// deleting (all read) messages
	writefd("AT+CMGD=0,1");
	
	if(!strncasecmp(message, "ip address", 10)) {
		sender = run("ip a | egrep 'UP|inet ' | awk '/:/ { printf \"%-5s \", $2 } /inet/ { print $2 }'");
	
	} else if(!strncasecmp(message, "uptime", 6)) {
		sender = run("uptime");
	
	} else if(!strncasecmp(message, "uname", 5)) {
		sender = run("uname -a");
	
	} else if(!strncasecmp(message, "privmsg", 7)) {
		strncpy(message, " SMS >>", 7);
		
		if(!fork()) 
			execl("/bin/bash", "/opt/scripts/irc-sender", "/opt/scripts/irc-sender", "#inpres", message + 1, NULL);
		
		goto freestuff;
	
	} else if(!strncasecmp(message, "mpc", 3)) {
		sender = run("mpc -h 192.168.10.210 | head -1");
		
	} else sender = strdup("Unknown command\n");
	
	sprintf(output, "AT+CMGS=\"%s\"\r", phone);
	writefdraw(output);
	
	printf("[+] --------------------------\n");
	printf("%s", sender);
	printf("[+] --------------------------\n");
	
	writefdraw(sender);
	free(sender);
	
	// CTRL+Z
	writefdraw("\x1A");
	
	freestuff:
		free(phone);
		free(message);
	
	return 1;
}

int parse(char *buffer) {
	char temp[128];
	
	buffer = strcleaner(buffer);
	// printf("[-] debug: <%s>\n", buffer);
	
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
		}
		
		// current changed
		if(!strncmp(buffer, "^MODE", 5)) {
			__device.link.mode = modem_mode(buffer + 6);
		}

		// any others repport not parsed
		if(*buffer == '^') {
			// TODO
		}
		
		return PARSE_STATUS;
	}
	
	//
	// something else (error, echo, ...) arrives
	//
	if(!strcmp(buffer, "OK")) {
		printf("[+] parser: ok from device\n");
		return PARSE_OK;
	}
	
	if(!strncmp(buffer, "COMMAND NOT SUPPORT", 19)) {
		fprintf(stderr, "[-] error: command not supported !\n");
		return PARSE_FAIL;
	}
	
	printf("[-] parser: unknown: <%s>\n", buffer);
	
	return 1;
}
