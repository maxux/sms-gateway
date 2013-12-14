#ifndef __SERIAL_STATUS
	#define __SERIAL_STATUS

	typedef struct modem_3g_link_t {
		int duration;
		int txspeed;
		int rxspeed;
		uint64_t txtotal;
		uint64_t rxtotal;
		int txqos;
		int rxqos;
		int dbm;
		float dbmpc;
		char *mode;
		
	} modem_3g_link_t;	
	
	// modem ds flow repport
	modem_3g_link_t modem_dsflowrpt(modem_3g_link_t link, char *buffer);
	
	// modem rssi/signal quality
	modem_3g_link_t modem_rssi(modem_3g_link_t link, char *buffer);
	
	// modem running mode
	char *modem_mode(char *buffer);
#endif
