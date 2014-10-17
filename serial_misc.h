#ifndef __SERIAL_MISC
	#define __SERIAL_MISC
	
	#include <ctype.h>
	
	char *strcleaner(char *buffer);
	char *strduration(int value, char *output);
	
	void dump(unsigned char *data, unsigned int len);
#endif
