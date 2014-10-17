// Copyright 2011 The Avalon Project Authors. All rights reserved.
// Use of this source code is governed by the Apache License 2.0
// that can be found in the LICENSE file.
#ifndef SMS_PDU_H_
#define SMS_PDU_H_

#include <time.h>

typedef struct multipart_t {
	short id;
	char total;
	char current;
	
} multipart_t;

typedef enum sms_type_t {
	SMS_STANDARD = 0x00,
	SMS_FLASH    = 0x01
	
} sms_type_t;

typedef struct pdu_t {
	char *destination;
	unsigned char *message;
	size_t message_size;
	
	char *buffer;
	size_t buffer_size;
	
	multipart_t multipart;
	sms_type_t type;
	
} pdu_t;

#define MULTIPART_CUT   130

int pdu_message(char *number, char *message, sms_type_t type);
char *pdu_receive(char *data, char **message, char **phone);

#endif   // SMS_SMS_H_
