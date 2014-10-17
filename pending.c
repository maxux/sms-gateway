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
#include "serial_at.h"
#include "serial_status.h"
#include "serial_parser.h"
#include "serial.h"
#include "database.h"
#include "pdu.h"

int pending_add(char *number, char *message) {
	char *sqlquery;
	int value;
	
	sqlquery = sqlite3_mprintf(
		"INSERT INTO pending (number, message, sent) "
		"VALUES ('%q', '%q', 0)",
		number, message
	);
	
	if(!(value = db_sqlite_simple_query(sqlite_db, sqlquery)))
		fprintf(stderr, "[-] cannot insert data\n");
	
	sqlite3_free(sqlquery);
	
	return value;
}

void pending_check() {
	sqlite3_stmt *stmt;
	char *sqlquery, sqlquery2[1024];
	char *number = NULL, *message;
	char *rnumber;
	int id, type;
	
	sqlquery = "SELECT number, message, id, type "
	           "FROM pending WHERE sent = 0";
	
	if((stmt = db_sqlite_select_query(sqlite_db, sqlquery))) {
		if(sqlite3_step(stmt) == SQLITE_ROW) {
			number  = (char *) sqlite3_column_text(stmt, 0);
			message = (char *) sqlite3_column_text(stmt, 1);
			id      = sqlite3_column_int(stmt, 2);
			type    = sqlite3_column_int(stmt, 3);
			
			printf("[+] pending: [type: %d] <%s>: %s\n", type, number, message);
			// at_cmgs(number, message);
			
			rnumber = (*number == '+') ? number + 1 : number;
			
			type = pdu_message(rnumber, message, type);
		}
	
	} else fprintf(stderr, "[-] sqlite: cannot select pending\n");

	sqlite3_finalize(stmt);
	
	if(number) {
		sprintf(sqlquery2, "UPDATE pending SET sent = %d WHERE id = %d", type, id);
		db_sqlite_simple_query(sqlite_db, sqlquery2);	
	}
}
