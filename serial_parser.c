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
	char sender[4096], *cmd, *temp, *argv;
	int pipes[2], res;
	
	// grab phone origin
	if(!(phone = at_cmgr_getphone(buffer)))
		return 0;
	
	// grab message
	if(!(message = at_cmgr_getmessage(buffer))) {
		free(phone);
		return 0;
		
	} else argv = message;
	
	printf("[+] replying to: <%s>\n", phone);
	printf("[+] message    : <%s>\n", message);
	
	// deleting (all read) messages
	at_cmgd(0, 1);
	
	// building command name
	if((temp = strchr(message, ' '))) {
		cmd  = strndup(message, temp - message);
		argv = temp + 1;
		
	} else cmd = strdup(message);
	
	// starting external program to handle command
	if(pipe(pipes) < 0)
		diep("pipe");
	
	if(!fork()) {
		dup2(pipes[1], STDOUT_FILENO);
		close(pipes[0]);

		execl("/bin/bash", "./sms-handler.sh", "./sms-handler.sh", cmd, argv, NULL);
		
	} else {
		close(pipes[1]);
		
		if((res = read(pipes[0], sender, sizeof(sender))) < 0)
			diep("external output read");
		
		sender[res] = '\0';
		wait(NULL);
	}
	
	// sending reply
	at_cmgs(phone, sender);
	
	free(phone);
	free(message);
	free(cmd);
	
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
