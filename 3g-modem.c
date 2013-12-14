#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>

#define BAUDRATE       B460800
#define MODEMDEVICE    "/dev/ttyUSB2"

typedef struct link_stats_t {
	int duration;
	int txspeed;
	int rxspeed;
	uint64_t txtotal;
	uint64_t rxtotal;
	int txqos;
	int rxqos;
	int dbm;
	float dbmpc;
	
} link_stats_t;

#define RUNNING_MAX_LOW    5
#define RUNNING_MAX_HI     5

char *__running_mode[RUNNING_MAX_LOW][RUNNING_MAX_HI] = {
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "GPRS",    "EDGE",    "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "unknown", "unknown"},
	{"unknown", "unknown", "unknown", "3G",      "HDSPA"},
};

link_stats_t global_status;

#define PARSE_OK       10
#define PARSE_STATUS   20

int fd;

void diep(char *str) {
	perror(str);
	exit(EXIT_FAILURE);
}

void dier(char *str) {
	fprintf(stderr, "[-] %s\n", str);
	exit(EXIT_FAILURE);
}

char *run(char *cmd) {
	FILE *pipe;
	char *output, buffer[2048];
	size_t length = 0;
	
	if(!(pipe = popen(cmd, "r")))
		return NULL;
	
	output = (char *) calloc(1, sizeof(char));
	
	while(fgets(buffer, sizeof(buffer), pipe)) {
		length += strlen(buffer) + 1;
		output = (char *) realloc(output, length * sizeof(char));
		
		strcat(output, buffer);
	}
	
	fclose(pipe);
	
	return output;
}

int writefdraw(char *message) {
	int value;
	size_t length = strlen(message);
	
	if((value = write(fd, message, length)) < 0)
		perror(MODEMDEVICE);
	
	return value;
}

int writefd(char *message) {
	size_t length;
	char *temp;
	
	length = strlen(message) + 3;
	
	if(!(temp = (char *) malloc(sizeof(char) * length)))
		diep("[-] malloc");
	
	strcpy(temp, message);
	strcat(temp, "\r\r\n");
	
	// printf("[+] << %s\n", message);
	
	return writefdraw(temp);
}

char *cleaner(char *buffer) {
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

int handle_stats(char *buffer) {
	char hours[32];
	
	sscanf(
		buffer,
		"%x,%x,%x,%" SCNx64 ",%" SCNx64 ",%x,%x",
		&global_status.duration,
		&global_status.txspeed,
		&global_status.rxspeed,
		&global_status.txtotal,
		&global_status.rxtotal,
		&global_status.txqos,
		&global_status.rxqos
	);
	
	printf(
		"[%s] link: %d dBm (%.1f%%), bw: ↓% 5d kbps, ↑% 5d kbps, data: ↓% 3.2f Mo, ↑% 3.2f Mo\n",
		strduration(global_status.duration, hours),
		global_status.dbm,
		global_status.dbmpc,
		global_status.rxspeed * 8,
		global_status.txspeed * 8,
		(global_status.rxtotal / 1024 / (double) 1024),
		(global_status.txtotal / 1024 / (double) 1024)
	);
	
	fflush(stdout);
	
	return 1;
}

int handle_quality(char *buffer) {
	int dbm, error;
		
	sscanf(buffer, "%d,%d", &dbm, &error);
	
	// fix dBi value
	global_status.dbmpc = (dbm * 100) / (float) 31;
	global_status.dbm   = (dbm * 2) - 113;
	
	return 1;
}

int handle_mode(char *buffer) {
	int low, hi;
	
	sscanf(buffer, "%d,%d", &low, &hi);
	
	if(low > RUNNING_MAX_LOW || hi > RUNNING_MAX_HI) {
		printf("[-] wrong value (%d,%d) for running mode\n", low, hi);
		return 1;
	}
	
	printf("[+] current mode: %s\n", __running_mode[low - 1][hi - 1]);
	return 1;
}

int handler_notify(char *buffer) {
	int index = 0;
	char temp[512];
	
	// jump to message id directly
	index = atoi(buffer + 12);
	printf("[+] message id: %d\n", index);
	
	snprintf(temp, sizeof(temp), "AT+CMGR=%d", index);
	writefd(temp);
	
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
	
	if(!(temp = strchr(buffer, '\n')))
		return NULL;
	
	if(!(right = strstr(buffer, "\r\n\r\n")))
		return NULL;
	
	return strndup(temp + 1, right - temp - 1);
}

int handler_sms_content(char *buffer) {
	char *phone = NULL, *message = NULL;
	char output[2048], *sender;
	
	// grab phone origin
	if(!(phone = at_cmgr_getphone(buffer)))
		return 0;
	
	// grab message
	if(!(message = at_cmgr_getmessage(buffer))) {
		free(phone);
		return 0;
	}
	
	printf("[+] replying to: <%s>\n", phone);
	printf("[+] message    : <%s>\n", message);
	
	// deleting (all read) messages
	writefd("AT+CMGD=0,1");
	
	if(!strncasecmp(message, "ip address", 10)) {
		sender = run("ip a | egrep 'UP|inet ' | awk '/:/ { printf \"%-5s \", $2 } /inet/ { print $2 }'");
	
	} else if(!strncasecmp(message, "uptime", 6)) {
		sender = run("uptime");
	
	} else if(!strncasecmp(message, "uname", 5)) {
		sender = run("uname -a");
	
	} else if(!strncasecmp(message, "privmsg", 7)) {
		strncpy(message, " SMS >>", 7);
		
		if(!fork()) 
			execl("/bin/bash", "/opt/scripts/irc-sender", "/opt/scripts/irc-sender", "#inpres", message + 1, NULL);
		
		goto freestuff;
	
	} else if(!strncasecmp(message, "mpc", 3)) {
		sender = run("mpc -h 192.168.10.210 | head -1");
		
	} else sender = strdup("Unknown command\n");
	
	sprintf(output, "AT+CMGS=\"%s\"\r", phone);
	writefdraw(output);
	
	printf("[+] --------------------------\n");
	printf("%s", sender);
	printf("[+] --------------------------\n");
	
	writefdraw(sender);
	free(sender);
	
	// CTRL+Z
	writefdraw("\x1A");
	
	freestuff:
		free(phone);
		free(message);
	
	return 1;
}

int parse(char *buffer) {
	buffer = cleaner(buffer);
	// printf("[-] debug: <%s>\n", buffer);
	
	//
	// when successful message arrives
	//
	if(*buffer == '+') {		
		if(!strncmp(buffer, "+CMTI:", 6)) {
			printf("[+] parser: we got a new message\n");
			return handler_notify(buffer);
		}
		
		if(!strncmp(buffer, "+CMGR:", 6)) {
			printf("[+] parser: we got a message content\n");
			return handler_sms_content(buffer);
		}
		
		if(!strncmp(buffer, "+CMGS:", 6)) {
			printf("[+] parser: message sent\n");
			return PARSE_OK;
		}
		
		if(!strncmp(buffer, "+CPMS:", 6)) {
			printf("[+] parser: storage settings set\n");
			return PARSE_OK;
		}
		
		if(!strncmp(buffer, "+CSQ:", 5)) {
			handle_quality(buffer + 6);
			return PARSE_OK;
		}
	}
	
	//
	// when a status message arrive
	//
	if(*buffer == '^') {
		if(!strncmp(buffer, "^DSFLOWRPT", 10)) {
			handle_stats(buffer + 11);
		}
		
		if(!strncmp(buffer, "^MODE", 5)) {
			handle_mode(buffer + 6);
		}

		if(*buffer == '^') {
			
		}
		
		return PARSE_STATUS;
	}
	
	//
	// something else (error, echo, ...) arrives
	//
	if(!strcmp(buffer, "OK")) {
		printf("[+] parser: ok from device\n");
		return PARSE_OK;
	}
	
	printf("[-] parser: unknown: <%s>\n", buffer);
	
	return 1;
}

int set_interface_attribs (int fd, int speed) {
	struct termios tty;
	int parity = 0;
	
	memset(&tty, 0, sizeof(tty));
	
	if(tcgetattr (fd, &tty) != 0)
		diep(MODEMDEVICE);

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	
	tty.c_iflag &= ~IGNBRK;      // ignore break signal
	tty.c_lflag  = 0;
	tty.c_oflag  = 0;
	tty.c_cc[VMIN]  = 1;
	tty.c_cc[VTIME] = 10;

	// shut off xon/xoff ctrl
	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);   // ignore modem controls, enable reading
	tty.c_cflag &= ~(PARENB | PARODD); // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if(tcsetattr (fd, TCSANOW, &tty) != 0)
		diep(MODEMDEVICE);
	
	return 0;
}

char *readfd(char *buffer, size_t length) {
	int res, saved = 0;
	
	while(1) {
		if((res = read(fd, buffer + saved, length - saved)) < 0)
			perror(MODEMDEVICE);
			
		buffer[res + saved] = '\0';
		
		// line/block is maybe not completed, waiting for a full line/block
		if(buffer[res + saved - 1] != '\n') {
			saved = res;
			continue;
			
		} else saved = 0;
		
		// printf("[+] >> %s\n", buffer);
		
		return buffer;
	}
}

int checkok() {
	char buffer[512];
	
	// loop while we got a status reply (we can process it later)
	while(parse(readfd(buffer, sizeof(buffer))) == PARSE_STATUS);
	
	return (parse(buffer) == PARSE_OK);
}

int main(int argc, char *argv[]) {
	char buffer[2048];

	if((fd = open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_SYNC)) < 0)
		diep(MODEMDEVICE);

	set_interface_attribs(fd, BAUDRATE);
	
	// setting text mode
	writefd("AT+CMGF=1");
	if(!checkok())
		dier("cannot set text mode");
		
	
	// setting storage
	writefd("AT+CPMS=\"ME\",\"ME\",\"ME\"");
	if(!checkok())
		dier("cannot set storage mode");
	
	// setting notification mode
	writefd("AT+CNMI=1,1,0,2,1");
	if(!checkok())
		dier("cannot set notification mode");
	
	// request signal quality
	writefd("AT+CSQ");

	// infinite loop on messages
	while(1) {
		readfd(buffer, sizeof(buffer));
		parse(buffer);
	}
	
	return 0;
}
