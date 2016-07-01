/* 
 * Author: Daniel Maxime (maxux.unix@gmail.com)
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "pdu.h"
#include "serial_at.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial.h"
#include "database.h"

int pending_add(char *number, char *message) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO pending (number, message, added) "
		"VALUES ('%q', '%q', datetime('now'))",
		number, message
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

int pdu_add(char *pdu) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf("INSERT INTO raw (arrived, pdu) VALUES (datetime('now'), '%s')", pdu);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

int pending_commit(int id, int status) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"UPDATE pending SET sent = %d, departure = datetime('now') "
		"WHERE id = %d",
		status, id
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot update data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

int segment_add(pdu_t *pdu) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO segments (number, message, received, partid, part, total) "
		"VALUES ('%s', '%q', datetime('now'), %d, %d, %d)",
		pdu->number, pdu->message,
		pdu->multipart.id, pdu->multipart.current, pdu->multipart.total
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

int message_add(char *number, char *message) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO messages (number, message, received, read) "
		"VALUES ('%q', '%q', datetime('now'), 0)",
		number, message
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

int failed_add(char *pdu) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO failed (pdu) VALUES ('%q')",
		pdu
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

void pending_check() {
	sqlite3_stmt *stmt;
	char *sqlquery;
	char *number = NULL, *message;
	char *rnumber;
	int id, type, status;
	
	sqlquery = "SELECT number, message, id, type "
	           "FROM pending WHERE sent = 0 LIMIT 1";
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			number  = (char *) sqlite3_column_text(stmt, 0);
			message = (char *) sqlite3_column_text(stmt, 1);
			id      = sqlite3_column_int(stmt, 2);
			type    = sqlite3_column_int(stmt, 3);
			status  = 1;
			
			printf("[+] pending: [type: %d] <%s>: %s\n", type, number, message);
			// at_cmgs(number, message);
			
			rnumber = (*number == '+') ? number + 1 : number;

			if(pdu_message(rnumber, message, type) < 0) {
				fprintf(stderr, "[-] message not sent, setting error flag\n");
				status = 2;
			}
		}
	
	} else fprintf(stderr, "[-] sqlite: cannot select pending\n");

	sqlite3_finalize(stmt);
	
	if(number)
		pending_commit(id, status);
}

int unread_check() {
	at_cmgl("1");
	return checkok();
}
