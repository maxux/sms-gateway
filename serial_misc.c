#include <stdio.h>
#include <string.h>

char *strcleaner(char *buffer) {
	size_t length;
	
	// stripping header
	while(*buffer && *buffer != '\n')
		buffer++;
	
	// trim the end of the line/block
	length = strlen(buffer) - 1;
	
	while(buffer[length] == '\n' || buffer[length] == '\r') {
		buffer[length] = '\0';
		length--;
	}
	
	return ++buffer;
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
