#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "serial_status.h"
#include "serial.h"

#define RUNNING_MAX_LOW    5
#define RUNNING_MAX_HI     5
	
static char *__running_mode[RUNNING_MAX_LOW][RUNNING_MAX_HI] = {
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "GPRS",    "EDGE",    "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "3G",      "HDSPA"},
};

modem_3g_link_t modem_dsflowrpt(modem_3g_link_t link, char *buffer) {
	sscanf(
		buffer,
		"%x,%x,%x,%" SCNx64 ",%" SCNx64 ",%x,%x",
		&link.duration,
		&link.txspeed,
		&link.rxspeed,
		&link.txtotal,
		&link.rxtotal,
		&link.txqos,
		&link.rxqos
	);
	
	return link;
}

modem_3g_link_t modem_rssi(modem_3g_link_t link, char *buffer) {
	int dbm, error;
		
	sscanf(buffer, "%d,%d", &dbm, &error);
	
	// fix dBi value
	link.dbmpc = (dbm * 100) / (float) 31;
	link.dbm   = (dbm * 2) - 113;
	
	return link;
}

char *modem_mode(char *buffer) {
	int low, hi;
	
	sscanf(buffer, "%d,%d", &low, &hi);
	
	if(low > RUNNING_MAX_LOW || hi > RUNNING_MAX_HI) {
		printf("[-] wrong value (%d,%d) for running mode\n", low, hi);
		return "unknown";
	}
	
	return __running_mode[low - 1][hi - 1];
}
