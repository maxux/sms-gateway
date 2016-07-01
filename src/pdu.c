// Copyright 2011 The Avalon Project Authors. All rights reserved.
// Use of this source code is governed by the Apache License 2.0
// that can be found in the LICENSE file.
//
//  SMS encoding/decoding functions, which are based on examples from:
//  http://www.dreamfabric.com/sms/

#include "pdu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <iconv.h>
#include <math.h>
#include "serial_parser.h"
#include "serial_status.h"
#include "serial_misc.h"
#include "serial_at.h"
#include "serial.h"

short pduid = 1;

enum {
	BITMASK_7BITS = 0x7F,
	BITMASK_8BITS = 0xFF,
	BITMASK_HIGH_4BITS = 0xF0,
	BITMASK_LOW_4BITS = 0x0F,

	TYPE_OF_ADDRESS_INTERNATIONAL_PHONE = 0x91,
	TYPE_OF_ADDRESS_NATIONAL_SUBSCRIBER = 0xC8,

	SMS_DELIVER_ONE_MESSAGE = 0x04,
	SMS_SUBMIT              = 0x11,
	TP_UDHI                 = 0x40,

	SMS_MAX_7BIT_TEXT_LENGTH  = 160,
};

enum {
	PDU_MTI_0 = 0x01, // PDU_BIT_0
	PDU_MTI_1 = 0x02, // PDU_BIT_1
	PDU_MMS   = 0x04, // PDU_BIT_2
	// PDU_BIT_3
	// PDU_BIT_4
	PDU_SRI  = 0x20, // PDU_BIT_5
	PDU_UDHI = 0x40, // PDU_BIT_6
	PDU_RP   = 0x80  // PDU_BIT_7
};

// Swap decimal digits of a number (e.g. 12 -> 21).
static unsigned char SwapDecimalNibble(const unsigned char x) {
	return (x / 16) + ((x % 16) * 10);
}

// Decode PDU message by splitting 8 bit encoded buffer into 7 bit ASCII characters.
static unsigned int
DecodePDUMessage(const unsigned char *buffer, unsigned int buffer_length, char *output_sms_text, unsigned int sms_text_length)
{
	unsigned int i, output_text_length = 0;
	int cob = 1; // carry on bit
	
	// first char
	output_sms_text[output_text_length++] = BITMASK_7BITS & buffer[0];
	
	for(i = 1; i < buffer_length; i++) {
		output_sms_text[output_text_length++] = 
			BITMASK_7BITS & ((buffer[i] << cob) | (buffer[i - 1] >> (8 - cob)));

		if(++cob == 8) {
			cob = 1;
			output_sms_text[output_text_length++] = buffer[i] & BITMASK_7BITS;
		}
	}
	
	if(output_text_length < sms_text_length)
		output_sms_text[output_text_length++] = buffer[i - 1] >> (8 - cob);

	return output_text_length;
}

// Encode a digit based phone number for SMS based format.
static int
EncodePhoneNumber(const char *phone_number, unsigned char *output_buffer, int buffer_size)
{
	int output_buffer_length = 0;  
	const int phone_number_length = strlen(phone_number);

	// Check if the output buffer is big enough.
	if ((phone_number_length + 1) / 2 > buffer_size)
		return -1;

	int i = 0;
	for (; i < phone_number_length; ++i) {

		if (phone_number[i] < '0' && phone_number[i] > '9')
			return -1;

		if (i % 2 == 0) {
			output_buffer[output_buffer_length++] = BITMASK_HIGH_4BITS | (phone_number[i] - '0');
		} else {
			output_buffer[output_buffer_length - 1] =
				(output_buffer[output_buffer_length - 1] & BITMASK_LOW_4BITS) |
				((phone_number[i] - '0') << 4); 
		}
	}

	return output_buffer_length;
}

// Decode a digit based phone number for SMS based format.
static int
DecodePhoneNumber(const unsigned char* buffer, int phone_number_length, char* output_phone_number)
{
	int i = 0;
	for (; i < phone_number_length; ++i) {
		if (i % 2 == 0)
			output_phone_number[i] = (buffer[i / 2] & BITMASK_LOW_4BITS) + '0';
		else
			output_phone_number[i] = ((buffer[i / 2] & BITMASK_HIGH_4BITS) >> 4) + '0';
	}
	output_phone_number[phone_number_length] = '\0';  // Terminate C string.
	return phone_number_length;
}
			
// Encode a SMS message to PDU
int pdu_encode(pdu_t *pdu) {
	int outlen = 0;
	int length = 0;

	//
	// Stage 1: Set SMS center number
	//
	
	// Use CSCA, skipping this.
	pdu->buffer[outlen++] = 0x00;

	//
	// Stage 2: Set type of message
	//
	
	// pdu->buffer[outlen++] = SMS_SUBMIT;
	pdu->buffer[outlen++] = 0x41;
	pdu->buffer[outlen++] = 0x00;  // Message reference.

	//
	// Stage 3: Set phone number
	//
	
	pdu->buffer[outlen]     = strlen(pdu->number);
	pdu->buffer[outlen + 1] = TYPE_OF_ADDRESS_INTERNATIONAL_PHONE;
	
	length = EncodePhoneNumber(
		pdu->number,
		(unsigned char *) pdu->buffer + outlen + 2,
		pdu->buffer_size - outlen - 2
	);
	
	outlen += length + 2;


	//
	// Stage 4: Protocol identifiers
	// 
	
	pdu->buffer[outlen++] = 0x00; // TP-PID: Protocol identifier.
	
	// pdu->buffer[outlen++] = 0x00; // TP-DCS: Data coding scheme. (7-bits)
	
	pdu->buffer[outlen++] = (pdu->type == SMS_FLASH) ? 0x18 : 0x08;
	// pdu->buffer[outlen++] = 0x18; // TP-DCS: Data coding scheme. (UCS2 + Flash)
	// pdu->buffer[outlen++] = 0x08; // TP-DCS: Data coding scheme. (UCS2)
	
	// pdu->buffer[outlen++] = 0xB0; // TP-VP: Validity: 10 days
	
	pdu->buffer[outlen++] = pdu->message_size + 7;
	// pdu->buffer[outlen++] = pdu->message_size + 6;
	
	// pdu->buffer[outlen++] = 0x05; // TP-UDHL: User Data Length
	pdu->buffer[outlen++] = 0x06; // TP-UDHL: User Data Length
	
	// pdu->buffer[outlen++] = 0x00; // Concatenated SMS, 8-bit ref. num
	pdu->buffer[outlen++] = 0x08; // Concatenated SMS, 16-bit ref. num
	
	pdu->buffer[outlen++] = 0x04; // Length of the header, excluding the first two fields
	// pdu->buffer[outlen++] = 0x03; // Length of the header, excluding the first two fields
	
	pdu->buffer[outlen++] = pdu->multipart.id & 0xFF; // Unique ID
	pdu->buffer[outlen++] = pdu->multipart.id >> 8;   // Unique ID
	pdu->buffer[outlen++] = pdu->multipart.total;     // Total Part count
	pdu->buffer[outlen++] = pdu->multipart.current;   // Current Part


	//
	// Stage 5: SMS message
	//
	
	// FIXME ?
	if(*(pdu->message))
		pdu->buffer[outlen++] = 0x00;
	
	memcpy(pdu->buffer + outlen, pdu->message, pdu->message_size - 1);
	outlen += pdu->message_size - 1;

	return outlen;
}

void pdu_type_debug(char type) {
	printf("[+] pdu: PDU_MTI    : %d %d\n", (type & PDU_MTI_0) ? 1 : 0, (type & PDU_MTI_1) ? 1 : 0);
	printf("[+] pdu: PDU_MMS    : %d (more message)\n", (type & PDU_MMS) ? 1 : 0);
	printf("[+] pdu: PDU_SRI    : %d (report returned)\n", (type & PDU_SRI) ? 1 : 0);
	printf("[+] pdu: PDU_UDHI   : %d (header present)\n", (type & PDU_UDHI) ? 1 : 0);
	printf("[+] pdu: PDU_RP     : %d\n", (type & PDU_RP) ? 1 : 0);
}

pdu_t *pdu_decode(const unsigned char *buffer, int buffer_length, pdu_t *message) {
	dump((unsigned char *) buffer, buffer_length);
	
	//
	// pdu header
	//
	printf("[+] pdu: smsc length: 0x%02x\n", buffer[0]);
	printf("[+] pdu: smsc type  : 0x%02x\n", buffer[1]);
	const int sms_deliver_start = 1 + buffer[0];
	
	//
	// sms-deliver
	//
	if(sms_deliver_start + 1 > buffer_length)
		return NULL;
	
	printf("[+] pdu: sms-deliver: 0x%02x\n", buffer[sms_deliver_start]);
	pdu_type_debug(buffer[sms_deliver_start]);
	
	if(!(buffer[sms_deliver_start] & PDU_MMS))
		printf("[+] pdu: warning    : more message are waiting !\n");
	
	//
	// sender header
	//
	const int sender_number_length = buffer[sms_deliver_start + 1];
	message->number = malloc(sizeof(char) * sender_number_length);
	
	printf("[+] pdu: src length : 0x%02x\n", buffer[sms_deliver_start + 1]);
	printf("[+] pdu: src type   : 0x%02x\n", buffer[sms_deliver_start + 2]);
	
	DecodePhoneNumber(buffer + sms_deliver_start + 3, sender_number_length, message->number);

	const int sms_pid_start = sms_deliver_start + 3 + (buffer[sms_deliver_start + 1] + 1) / 2;
	
	printf("[+] pdu: protocol id: 0x%02x\n", buffer[sms_pid_start]);
	printf("[+] pdu: data coding: 0x%02x\n", buffer[sms_pid_start + 1]);

	//
	// timestamp
	//
	struct tm sms_broken_time;
	sms_broken_time.tm_year = 100 + SwapDecimalNibble(buffer[sms_pid_start + 2]);
	sms_broken_time.tm_mon  = SwapDecimalNibble(buffer[sms_pid_start + 3]) - 1;
	sms_broken_time.tm_mday = SwapDecimalNibble(buffer[sms_pid_start + 4]);
	sms_broken_time.tm_hour = SwapDecimalNibble(buffer[sms_pid_start + 5]);
	sms_broken_time.tm_min  = SwapDecimalNibble(buffer[sms_pid_start + 6]);
	sms_broken_time.tm_sec  = SwapDecimalNibble(buffer[sms_pid_start + 7]);
	const char gmt_offset   = SwapDecimalNibble(buffer[sms_pid_start + 8]);
	
	// GMT offset is expressed in 15 minutes increments.
	message->timestamp = mktime(&sms_broken_time) - gmt_offset * 15 * 60;

	//
	// protocol identifier
	//
	int sms_start = sms_pid_start + 2 + 7;
	
	printf("[+] pdu: sms length : 0x%02x (%d bytes)\n", buffer[sms_start], buffer[sms_start]);

	message->message_size = buffer[sms_start];
	int udh_length = 0;
	
	message->multipart.id      = 0;
	message->multipart.total   = 1;
	message->multipart.current = 1;
	
	if(buffer[sms_deliver_start] & PDU_UDHI) {
		udh_length = buffer[sms_start + 1];
			
		printf("[+] pdu: udhi       : present\n");
		printf("[+] pdu: udhi       : 0x%02x\n", buffer[sms_start + 1]);
		printf("[+] pdu: udhi iei   : 0x%02x\n", buffer[sms_start + 2]);
		printf("[+] pdu: udhi iedl  : 0x%02x\n", buffer[sms_start + 3]);
		
		// SMS Concatenate 8 bits ref
		if(buffer[sms_start + 2] == 0x00) {
			printf("[+] pdu: part id    : 0x%02x\n", buffer[sms_start + 4]);
			printf("[+] pdu: part total : 0x%02x\n", buffer[sms_start + 5]);
			printf("[+] pdu: part this  : 0x%02x\n", buffer[sms_start + 6]);
		}
		
		message->multipart.id      = buffer[sms_start + 4];
		message->multipart.total   = buffer[sms_start + 5];
		message->multipart.current = buffer[sms_start + 6];
	}

	//
	// payload decoding
	//
	message->message = malloc(sizeof(char) * message->message_size * 2);
	
	const unsigned int decoded_sms_text_size = DecodePDUMessage(
		buffer + sms_start + 1,
		buffer_length - (sms_start + 1),
		message->message,
		message->message_size
	);

	if(decoded_sms_text_size != message->message_size)
		return NULL;
	
	//
	// udh cleaner
	//
	if(udh_length > 0) {
		int skip_char = ceil(((udh_length + 1) * 8) / 7.0);
		
		memmove(message->message, message->message + skip_char, message->message_size - skip_char);
		message->message_size -= skip_char;
	}

	return message;
}

static const unsigned int gsm7bits_to_unicode[128] = {
  '@', 0xa3,  '$', 0xa5, 0xe8, 0xe9, 0xf9, 0xec, 0xf2, 0xc7, '\n', 0xd8, 0xf8, '\r', 0xc5, 0xe5,
0x394,  '_',0x3a6,0x393,0x39b,0x3a9,0x3a0,0x3a8,0x3a3,0x398,0x39e,    0, 0xc6, 0xe6, 0xdf, 0xc9,
  ' ',  '!',  '"',  '#', 0xa4,  '%',  '&', '\'',  '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
 0xa1,  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
  'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',  'X',  'Y',  'Z', 0xc4, 0xd6,0x147, 0xdc, 0xa7,
 0xbf,  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z', 0xe4, 0xf6, 0xf1, 0xfc, 0xe0,
};

static const unsigned int gsm7bits_extend_to_unicode[128] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,'\f',   0,   0,   0,   0,   0,
    0,   0,   0,   0, '^',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0, '{', '}',   0,   0,   0,   0,   0,'\\',
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, '[', '~', ']',   0,
  '|',   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,0x20ac, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static void utf8_build( unsigned int value, unsigned char out[6], int in_pos, int out_pos ) {
	unsigned int in_mask, out_mask;
	int byte = 0;
	while ( in_pos < 32 ) {
		in_mask = 1 << ( 31 - in_pos );
		out_mask = 1 << ( 7 - out_pos );
		if ( value & in_mask ) out[byte] |= out_mask;
		in_pos++;
		out_pos++;
		if ( out_pos > 7 ) {
			out_pos=2;
			byte++;
		}
	}
}

static int utf8_encode(unsigned int value, unsigned char out[6]) {
	int i;
	for ( i=1; i<6; ++i ) out[i] = 0x80;  /* 10xxxxxx */
	if ( value < 0x80 ) {
		out[0] = 0x0;   /* 0xxxxxxx */
		utf8_build( value, out, 25, 1 );
		return 1;
	} else if ( value < 0x800 ) {
		out[0] = 0xC0;  /* 110xxxxx */
		utf8_build( value, out, 21, 3 );
		return 2;
	} else if ( value < 0x10000 ) {
		out[0] = 0xE0;  /* 1110xxxx */
		utf8_build( value, out, 16, 4 );
		return 3;
	} else if ( value < 0x200000 ) {
		out[0] = 0xF0;  /* 11110xxx */
		utf8_build( value, out, 11, 5 );
		return 4;
	} else if ( value < 0x4000000 ) {
		out[0] = 0xF8;  /* 111110xx */
		utf8_build( value, out,  6, 6 );
		return 5;
	} else if ( value < (unsigned int ) 0x80000000 ) {
		out[0] = 0xFC;  /* 1111110x */
		utf8_build( value, out,  1, 7 );
		return 6;
	} else {
		/* error, above 2^31 bits encodable by UTF-8 */
		return 0;
	}
}

inline short utf8_charlen(unsigned char c) {
	if(c < 0x80)
		return 1;         /* 0xxxxxxx */
		
	if((c & 0xe0) == 0xc0)
		return 2;         /* 110xxxxx */

	if((c & 0xf0) == 0xe0)
		return 3;         /* 1110xxxx */
		
	if((c & 0xf8) == 0xf0 && (c <= 0xf4))
		return 4;         /* 11110xxx */
		
	return 0; /* invalid utf-8 */
}

size_t utf_to_ucs(char *input, char *output, size_t outlen) {
	iconv_t ic;
	char *ichar = "UTF-8";
	char *ochar = "UCS2";
	size_t inlen = strlen(input);
	size_t outed = outlen;
	
	printf("[+] iconv: converting from <%s> to <%s>...\n", ichar, ochar);
	
	// Opening iconv handler
	if((ic = iconv_open(ochar, ichar)) == (iconv_t) -1) {
		perror("[-] iconv_open");
		return 0;
	}
	
	if(iconv(ic, &input, &inlen, &output, &outlen) == (size_t) -1) {
		perror("[-] iconv");
		return 0;
	}
	
	iconv_close(ic);
	
	return outed - outlen;
}

int pdu_message(char *number, char *message, sms_type_t type) {
	pdu_t pdu;
	char buffer[1024];
	unsigned char data[2048], hexa[4096];
	unsigned int i, length, write, skip;
	size_t real, current, size;
	int totalpart = 1, parser;
	
	// real = textconvert(message, buffer);	
	if(!(real = utf_to_ucs(message, (char *) buffer, sizeof(buffer)))) {
		fprintf(stderr, "[-] cannot convert message !\n");
		
		// fallback
		strcpy((char *) buffer, "[parsing failed]");
		real = 16;
	}
	
	totalpart = (int) ceil(real / 130.0);
	
	// setting multi-part settings
	pdu.multipart.id      = time(NULL) % 255;
	pdu.multipart.total   = totalpart;
	pdu.multipart.current = 1;
	
	// setting destination
	pdu.number = number;
	
	// setting output buffer and type
	pdu.buffer       = (char *) data;
	pdu.buffer_size  = sizeof(data);
	pdu.type         = type;
	
	// message float window
	current = 0;
	size    = (real <= MULTIPART_CUT) ? real : MULTIPART_CUT;
	
	// handling multi-part sending
	while(pdu.multipart.current <= pdu.multipart.total) {
		// verbose data
		printf(
			"[+] pdu: id: %u, total: %d, this part: %d\n",
			pdu.multipart.id,
			pdu.multipart.total,
			pdu.multipart.current
		);
		
		pdu.message      = buffer + current;
		pdu.message_size = size;
		
		// verbose data
		printf("[+] pdu: message to <%s>\n", pdu.number);
		dump((unsigned char *) pdu.message, pdu.message_size);
		
		length = pdu_encode(&pdu);
		
		for(i = 0, write = 0; i < length; i++)
			write += sprintf((char *) hexa + write, "%02X", data[i]);
		
		skip = ((int) *data) + 1;
		
		sprintf(buffer, "AT+CMGS=%d\r", length - skip);
		writefdraw(buffer);
		
		writefdraw((char *) hexa);
		at_commit();
		
		// double check
		if((parser = checkok()) == PARSE_FAIL) {
			printf("[-] sending failed\n");
			return -1;
		}
		
		checkok();
		
		// next part
		current += size;
		size     = (real - current > MULTIPART_CUT) ? MULTIPART_CUT : real - current;
		pdu.multipart.current++;
	}
	
	return 1;
}

pdu_t *pdu_receive(char *data) {
	char readable[4096], temp[10];
	unsigned char *pdu;
	int length, charlen, i, j, k;
	pdu_t *output = malloc(sizeof(pdu_t));
	
	memset(temp, 0, sizeof(temp));
	length = strlen(data);
	
	//
	// string pdu to binary pdu
	//
	pdu = malloc(sizeof(char) * (length / 2) + 1);
	
	for(i = 0; i < length; i += 2) {
		strncpy(temp, data + i, 2);
		pdu[i / 2] = (char) strtol(temp, NULL, 16);
	}
	
	if(!pdu_decode(pdu, length / 2, output)) {
		free(pdu);
		free(output);
		
		return NULL;
	}
	
	free(pdu);
	
	//
	// charset convertion
	//
	
	k = 0; // readable index
	
	// converting gsm7 octets to utf-8
	for(i = 0; i < output->message_size; i++) {
		charlen = utf8_encode(gsm7bits_to_unicode[output->message[i]], (unsigned char *) temp);
		
		for(j = 0; j < charlen; j++)
			readable[k++] = temp[j];
	}
	
	// end of readable string
	readable[k] = '\0';
	
	free(output->message);
	output->message = strdup(readable);
	output->message_size = strlen(readable);
	
	return output;
}
