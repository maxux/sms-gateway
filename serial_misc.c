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
