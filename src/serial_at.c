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
#include "serial_at.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial.h"

#define zsnprintf(x, ...) snprintf(x, sizeof(x), __VA_ARGS__)

int at_single() {
	at_commit();
	writefd("AT");
	return checkok();
}

int at_commit() {
	writefdraw("\x1A");
	return 0;
}

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

int at_cmgl(char *mode) {
	char buffer[256];
	
	zsnprintf(buffer, "AT+CMGL=%s", mode);
	writefd(buffer);
	
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
	
	printf("<< %s >>\n", buffer);
	
	if(!(temp = strchr(buffer, '\n')))
		return NULL;
	
	if(!(right = strstr(buffer, "\r\n\r\n")))
		return NULL;
	
	return strndup(temp + 1, right - temp - 1);
}

int at_cmgs(char *phone, char *message) {
	char output[2048];
	
	sprintf(output, "AT+CMGS=\"%s\"\r\r\n", phone);
	writefdraw(output);
	
	printf("[+] --------------------------\n");
	printf("%s", message);
	printf("[+] --------------------------\n");
	
	writefdraw(message);
	
	// CTRL+Z
	at_commit();
	
	return 1;
}

int at_cmgd(int index, int mode) {
	char buffer[256];
	
	zsnprintf(buffer, "AT+CMGD=%d,%d", index, mode);
	writefd(buffer);
	
	return checkok();
}

int at_curc(int value) {
	char buffer[256];
	
	zsnprintf(buffer, "AT^CURC=%d", value);
	writefd(buffer);
	
	return checkok();
}

int at_echo(int value) {
	char buffer[64];
	
	zsnprintf(buffer, "ATE%d", value);
	writefd(buffer);
	
	return checkok();
}
