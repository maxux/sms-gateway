// Copyright 2011 The Avalon Project Authors. All rights reserved.
// Use of this source code is governed by the Apache License 2.0
// that can be found in the LICENSE file.
//
//  SMS encoding/decoding functions, which are based on examples from:
//  http://www.dreamfabric.com/sms/

#include "pdu.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include "serial_status.h"
#include "serial_at.h"
#include "serial.h"

enum {
	BITMASK_7BITS = 0x7F,
	BITMASK_8BITS = 0xFF,
	BITMASK_HIGH_4BITS = 0xF0,
	BITMASK_LOW_4BITS = 0x0F,

	TYPE_OF_ADDRESS_INTERNATIONAL_PHONE = 0x91,
	TYPE_OF_ADDRESS_NATIONAL_SUBSCRIBER = 0xC8,

	SMS_DELIVER_ONE_MESSAGE = 0x04,
	SMS_SUBMIT              = 0x11,

	SMS_MAX_7BIT_TEXT_LENGTH  = 160,
};

// Swap decimal digits of a number (e.g. 12 -> 21).
static unsigned char 
SwapDecimalNibble(const unsigned char x)
{
	return (x / 16) + ((x % 16) * 10);
}

// Encode/Decode PDU: Translate ASCII 7bit characters to 8bit buffer.
// SMS encoding example from: http://www.dreamfabric.com/sms/.
//
// 7-bit ASCII: "hellohello"
// [0]:h   [1]:e   [2]:l   [3]:l   [4]:o   [5]:h   [6]:e   [7]:l   [8]:l   [9]:o
// 1101000 1100101 1101100 1101100 1101111 1101000 1100101 1101100 1101100 1101111
//               |             |||           ||||| |               |||||||  ||||||
// /-------------/   ///-------///     /////-///// \------------\  |||||||  \\\\\\ .
// |                 |||               |||||                    |  |||||||   ||||||
// input buffer position
// 10000000 22111111 33322222 44443333 55555333 66666655 77777776 98888888 --999999
// |                 |||               |||||                    |  |||||||   ||||||
// 8bit encoded buffer
// 11101000 00110010 10011011 11111101 01000110 10010111 11011001 11101100 00110111
// E8       32       9B       FD       46       97       D9       EC       37


// Encode PDU message by merging 7 bit ASCII characters into 8 bit octets.
static int
EncodePDUMessage(const char* sms_text, int sms_text_length, unsigned char* output_buffer, int buffer_size)
{
	// Check if output buffer is big enough.
	if ((sms_text_length * 7 + 7) / 8 > buffer_size)
		return -1;

	int output_buffer_length = 0;
	int carry_on_bits = 1;
	int i = 0;

	for (; i < sms_text_length - 1; ++i) {
		output_buffer[output_buffer_length++] =
			((sms_text[i] & BITMASK_7BITS) >> (carry_on_bits - 1)) |
			((sms_text[i + 1] & BITMASK_7BITS) << (8 - carry_on_bits));
		carry_on_bits++;
		if (carry_on_bits == 8) {
			carry_on_bits = 1;
			++i;
		}
	}

	if (i <= sms_text_length)
		output_buffer[output_buffer_length++] = (sms_text[i] & BITMASK_7BITS) >> (carry_on_bits - 1);

	return output_buffer_length;
}

// Decode PDU message by splitting 8 bit encoded buffer into 7 bit ASCII
// characters.
static int
DecodePDUMessage(const unsigned char* buffer, int buffer_length, char* output_sms_text, int sms_text_length)
{
	int output_text_length = 0;
	if (buffer_length > 0)
		output_sms_text[output_text_length++] = BITMASK_7BITS & buffer[0];

	int carry_on_bits = 1;
	int i = 1;
	for (; i < buffer_length; ++i) {

		output_sms_text[output_text_length++] = BITMASK_7BITS & ((buffer[i] << carry_on_bits) | (buffer[i - 1] >> (8 - carry_on_bits)));

		if (output_text_length == sms_text_length) break;

		carry_on_bits++;

		if (carry_on_bits == 8) {
			carry_on_bits = 1;
			output_sms_text[output_text_length++] = buffer[i] & BITMASK_7BITS;
			if (output_text_length == sms_text_length) break;
		}

	}
	if (output_text_length < sms_text_length)  // Add last remainder.
		output_sms_text[output_text_length++] = buffer[i - 1] >> (8 - carry_on_bits);

	return output_text_length;
}

// Encode a digit based phone number for SMS based format.
static int
EncodePhoneNumber(const char* phone_number, unsigned char* output_buffer, int buffer_size)
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
int
pdu_encode(const char* service_center_number, const char* phone_number, const char* sms_text,
	   unsigned int text_length, unsigned char* output_buffer, int buffer_size)
{       
	if (buffer_size < 2)
		return -1;

	int output_buffer_length = 0;

	// 1. Set SMS center number.
	int length = 0;
	if (service_center_number && strlen(service_center_number) > 0) {
		output_buffer[1] = TYPE_OF_ADDRESS_INTERNATIONAL_PHONE;
		length = EncodePhoneNumber(service_center_number,
					   output_buffer + 2, buffer_size - 2);
		if (length < 0 && length >= 254)
			return -1;
		length++;  // Add type of address.
	}
	output_buffer[0] = length;
	output_buffer_length = length + 1;
	if (output_buffer_length + 4 > buffer_size)
		return -1;  // Check if it has space for four more bytes.

	// 2. Set type of message.
	output_buffer[output_buffer_length++] = SMS_SUBMIT;
	output_buffer[output_buffer_length++] = 0x00;  // Message reference.

	// 3. Set phone number.
	output_buffer[output_buffer_length] = strlen(phone_number);
	output_buffer[output_buffer_length + 1] = TYPE_OF_ADDRESS_INTERNATIONAL_PHONE;
	length = EncodePhoneNumber(phone_number,
				   output_buffer + output_buffer_length + 2,
				   buffer_size - output_buffer_length - 2);
	output_buffer_length += length + 2;
	if (output_buffer_length + 4 > buffer_size)
		return -1;  // Check if it has space for four more bytes.


	// 4. Protocol identifiers.
	output_buffer[output_buffer_length++] = 0x00;  // TP-PID: Protocol identifier.
	output_buffer[output_buffer_length++] = 0x00;  // TP-DCS: Data coding scheme.
	output_buffer[output_buffer_length++] = 0xB0;  // TP-VP: Validity: 10 days

	// 5. SMS message.
	// const int sms_text_length = strlen(sms_text);
	const int sms_text_length = text_length;
	if (sms_text_length > SMS_MAX_7BIT_TEXT_LENGTH)
		return -1;
	output_buffer[output_buffer_length++] = sms_text_length;
	length = EncodePDUMessage(sms_text, sms_text_length,
				  output_buffer + output_buffer_length, 
				  buffer_size - output_buffer_length);
	if (length < 0)
		return -1;
	output_buffer_length += length;

	return output_buffer_length;
}

int pdu_decode(const unsigned char* buffer, int buffer_length,
	       time_t* output_sms_time,
	       char* output_sender_phone_number, int sender_phone_number_size,
	       char* output_sms_text, int sms_text_size)
{

	if (buffer_length <= 0)
		return -1;

	const int sms_deliver_start = 1 + buffer[0];
	if (sms_deliver_start + 1 > buffer_length) return -1;
	if ((buffer[sms_deliver_start] & SMS_DELIVER_ONE_MESSAGE) != SMS_DELIVER_ONE_MESSAGE) return -1;

	const int sender_number_length = buffer[sms_deliver_start + 1];
	if (sender_number_length + 1 > sender_phone_number_size) return -1;  // Buffer too small to hold decoded phone number.

	// const int sender_type_of_address = buffer[sms_deliver_start + 2];  
	DecodePhoneNumber(buffer + sms_deliver_start + 3, sender_number_length,  output_sender_phone_number);

	const int sms_pid_start = sms_deliver_start + 3 + (buffer[sms_deliver_start + 1] + 1) / 2;

	// Decode timestamp.
	struct tm sms_broken_time;
	sms_broken_time.tm_year = 100 + SwapDecimalNibble(buffer[sms_pid_start + 2]);
	sms_broken_time.tm_mon  = SwapDecimalNibble(buffer[sms_pid_start + 3]) - 1;
	sms_broken_time.tm_mday = SwapDecimalNibble(buffer[sms_pid_start + 4]);
	sms_broken_time.tm_hour = SwapDecimalNibble(buffer[sms_pid_start + 5]);
	sms_broken_time.tm_min  = SwapDecimalNibble(buffer[sms_pid_start + 6]);
	sms_broken_time.tm_sec  = SwapDecimalNibble(buffer[sms_pid_start + 7]);
	const char gmt_offset   = SwapDecimalNibble(buffer[sms_pid_start + 8]);
	// GMT offset is expressed in 15 minutes increments.
	(*output_sms_time) = mktime(&sms_broken_time) - gmt_offset * 15 * 60;

	const int sms_start = sms_pid_start + 2 + 7;
	if (sms_start + 1 > buffer_length) return -1;  // Invalid input buffer.

	const int output_sms_text_length = buffer[sms_start];
	if (sms_text_size < output_sms_text_length) return -1;  // Cannot hold decoded buffer.

	const int decoded_sms_text_size = DecodePDUMessage(buffer + sms_start + 1, buffer_length - (sms_start + 1),
							   output_sms_text, output_sms_text_length);

	if (decoded_sms_text_size != output_sms_text_length) return -1;  // Decoder length is not as expected.

	// Add a C string end.
	if (output_sms_text_length < sms_text_size)
		output_sms_text[output_sms_text_length] = 0;
	else
		output_sms_text[sms_text_size-1] = 0;

	return output_sms_text_length;
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

unsigned int utf8_decode(char *s, unsigned int *pi) {
	unsigned int c;
	int i = *pi;
	/* one digit utf-8 */
	if ((s[i] & 128)== 0 ) {
		c = (unsigned int) s[i];
		i += 1;
	} else if ((s[i] & 224)== 192 ) { /* 110xxxxx & 111xxxxx == 110xxxxx */
		c = (( (unsigned int) s[i] & 31 ) << 6) +
			( (unsigned int) s[i+1] & 63 );
		i += 2;
	} else if ((s[i] & 240)== 224 ) { /* 1110xxxx & 1111xxxx == 1110xxxx */
		c = ( ( (unsigned int) s[i] & 15 ) << 12 ) + 
			( ( (unsigned int) s[i+1] & 63 ) << 6 ) +
		 	( (unsigned int) s[i+2] & 63 );
		i += 3;
	} else if ((s[i] & 248)== 240 ) { /* 11110xxx & 11111xxx == 11110xxx */
		c =  ( ( (unsigned int) s[i] & 7 ) << 18 ) +
			( ( (unsigned int) s[i+1] & 63 ) << 12 ) +
			( ( (unsigned int) s[i+2] & 63 ) << 6 ) +
			( (unsigned int) s[i+3] & 63 );
		i+= 4;
	} else if ((s[i] & 252)== 248 ) { /* 111110xx & 111111xx == 111110xx */
		c = ( ( (unsigned int) s[i] & 3 ) << 24 ) +
			( ( (unsigned int) s[i+1] & 63 ) << 18 ) +
			( ( (unsigned int) s[i+2] & 63 ) << 12 ) +
			( ( (unsigned int) s[i+3] & 63 ) << 6 ) +
			( (unsigned int) s[i+4] & 63 );
		i += 5;
	} else if ((s[i] & 254)== 252 ) { /* 1111110x & 1111111x == 1111110x */
		c = ( ( (unsigned int) s[i] & 1 ) << 30 ) + 
			( ( (unsigned int) s[i+1] & 63 ) << 24 ) +
			( ( (unsigned int) s[i+2] & 63 ) << 18 ) +
			( ( (unsigned int) s[i+3] & 63 ) << 12 ) +
			( ( (unsigned int) s[i+4] & 63 ) << 6 ) +
			( (unsigned int) s[i+5] & 63 );
		i += 6;
	} else {
		c = '?';
		i++;
	}
	*pi = i;
	return c;
}

static void
utf8_build( unsigned int value, unsigned char out[6], int in_pos, int out_pos )
{
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

int utf8_encode(unsigned int value, unsigned char out[6]) {
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

unsigned int textconvert(char *message, char *out) {
	unsigned int lng = strlen(message);
	unsigned int i, j, current, increment, real;
	
	for(i = 0, real = 0; i < lng; real++) {
		increment = 0;
		current = utf8_decode(message + i, &increment);
		
		// printf("Increment: %d, convert: %03d ", increment, current);
		
		for(j = 0; j < 128; j++) {
			if(gsm7bits_to_unicode[j] == current) {
				out[real] = j;
				break;
			}
		}
		
		if(j == 128) {
			printf("[-] warning: unknown charactere %d\n", current);
			out[real] = '?';
		}
		
		i += increment;

		if(real > 120)
			return real;
	}
	
	return real;
}

int pdu_message(char *number, char *message) {
	char buffer[1024];
	unsigned char pdu[2048], hexa[4096];
	unsigned int i, length, real, write, skip;
	
	real = textconvert(message, buffer);
	length = pdu_encode(CENTER_NUMBER, number, buffer, real, pdu, sizeof(buffer));
	
	for(i = 0, write = 0; i < length; i++)
		write += sprintf((char *) hexa + write, "%02X", pdu[i]);
	
	skip = ((int) *pdu) + 1;
	
	sprintf(buffer, "AT+CMGS=%d\r", length - skip);
	writefdraw(buffer);
	
	writefdraw((char *) hexa);
	at_commit();
	
	return 0;
}

char *pdu_receive(char *data, char **message, char **phone) {
	time_t hrs;
	char _phone[32], _text[512], readable[1024];
	unsigned char pdu[128];
	char temp[6];
	int length, i;
	int charlen, j, k;
	
	memset(temp, 0, sizeof(temp));
	length = strlen(data);
	
	for(i = 0; i < length; i += 2) {
		strncpy(temp, data + i, 2);
		pdu[i / 2] = (char) strtol(temp, NULL, 16);
	}
	
	if((length = pdu_decode(pdu, length / 2, &hrs, _phone, sizeof(_phone), _text, sizeof(_text))) < 0)
		return NULL;
	
	k = 0; // readable index
	
	// converting gsm7 to utf
	for(i = 0; i < length; i++) {
		charlen = utf8_encode(gsm7bits_to_unicode[_text[i]], (unsigned char *) temp);
		
		for(j = 0; j < charlen; j++)
			readable[k++] = temp[j];
	}
	
	// end of readable string
	readable[k] = '\0';
	
	*message = strdup(readable);
	*phone   = strdup(_phone);
	
	return *message;
}
