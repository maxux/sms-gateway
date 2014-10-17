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
#include <string.h>
#include "serial_misc.h"

char *strcleaner(char *buffer) {
	size_t length;
	
	// trim the end of the line/block
	length = strlen(buffer) - 1;
	
	while(buffer[length] == '\n' || buffer[length] == '\r') {
		buffer[length] = '\0';
		length--;
	}
	
	return buffer;
}

char *strduration(int value, char *output) {
	int hrs, min;
	
	hrs = (value / 3600);
	value %= 3600;
	
	min = (value / 60);
	value %= 60;
	
	sprintf(output, "%02d:%02d:%02d", hrs, min, value);
	
	return output;
}

void dump(unsigned char *data, unsigned int len) {
	unsigned int i, j;
	
	printf("[+] Data dump [%p -> %p] (%u bytes)\n", data, data + len, len);
	printf("[ ] 0x0000: ");
	
	for(i = 0; i < len;) {
		printf("0x%02x ", data[i++]);
		
		if(i % 16 == 0) {
			printf("|");
			
			for(j = i - 16; j < i; j++)
				printf("%c", ((isalnum(data[j]) ? data[j] : '.')));
						
			printf("|\n[ ] 0x%04x: ", i);
		}
	}
	
	if(i % 16) {
		printf("%-*s", 5 * (16 - (i % 16)), " ");
		
		printf("|");
		
		for(j = i - (i % 16); j < len; j++)
			printf("%c", ((isalnum(data[j]) ? data[j] : '.')));
					
		printf("%-*s|\n", 16 - (len % 16), " ");
	}
}
