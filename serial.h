#ifndef __SERIAL_H
	#define __SERIAL_H
	
	#define DEFAULT_BAUDRATE    B460800
	#define DEFAULT_DEVICE      "/dev/ttyUSB2"
	
	typedef struct modem_3g_t {
		char *name;
		int fd;
		modem_3g_link_t link;
		
	} modem_3g_t;
	
	extern modem_3g_t __device;
	
	void diep(char *str);
	void dier(char *str);
	
	char *readfd(char *buffer, size_t length, int timeout);
	int writefdraw(char *message);
	int writefd(char *message);
#endif
