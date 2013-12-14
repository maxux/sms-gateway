#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include "serial_at.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial.h"

#define zsnprintf(x, ...) snprintf(x, sizeof(x), __VA_ARGS__)

int at_cmgf(int mode) {
	char buffer[256];
	
	zsnprintf(buffer, "AT+CMGF=%d", mode);
	writefd(buffer);
	
	return checkok();
}

int at_cpms(char *s1, char *s2, char *s3) {
	char buffer[256];
	
	zsnprintf(buffer, "AT+CPMS=\"%s\",\"%s\",\"%s\"", s1, s2, s3);
	writefd(buffer);
	
	return checkok();
}

int at_cnmi(int mode, int i1, int i2, int i3, int i4) {
	char buffer[256];
	
	zsnprintf(buffer, "AT+CNMI=%d,%d,%d,%d,%d", mode, i1, i2, i3, i4);
	writefd(buffer);
	
	return checkok();
}

int at_csq() {
	writefd("AT+CSQ");
	return 1;
}


char *at_cmgr_getphone(char *buffer) {
	char *temp, *right, *output;
	size_t length = 0;
	
	if(!(temp = strchr(buffer, ',')))
		return NULL;
	
	if(!(right = strchr(temp + 2, '"')))
		return NULL;
	
	length = right - temp - 2;
	output = (char *) calloc(length + 1, sizeof(char));
	
	strncpy(output, temp + 2, length);
	
	return output;
}

char *at_cmgr_getmessage(char *buffer) {
	char *temp, *right;
	
	if(!(temp = strchr(buffer, '\n')))
		return NULL;
	
	if(!(right = strstr(buffer, "\r\n\r\n")))
		return NULL;
	
	return strndup(temp + 1, right - temp - 1);
}
