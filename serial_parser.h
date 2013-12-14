#ifndef __SERIAL_PARSER
	#define __SERIAL_PARSER
	
	#define PARSE_OK       10
	#define PARSE_STATUS   20
	#define PARSE_FAIL     30

	int checkok();
	int handler_notify(char *buffer);
	int handler_sms_content(char *buffer);
	int parse(char *buffer);
#endif
