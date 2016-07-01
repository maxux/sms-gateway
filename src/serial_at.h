#ifndef __SERIAL_AT
	#define __SERIAL_AT
	
	int at_single();
	int at_commit();
	
	int at_cmgf(int mode);
	int at_cpms(char *s1, char *s2, char *s3);
	int at_cnmi(int mode, int i1, int i2, int i3, int i4);
	int at_csq();
	int at_cmgl(char *mode);
	int at_cmgs(char *phone, char *message);
	int at_cmgd(int index, int mode);
	int at_curc(int value);
	int at_echo(int value);
	
	char *at_cmgr_getphone(char *buffer);
	char *at_cmgr_getmessage(char *buffer);
#endif
