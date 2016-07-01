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
#include <ctype.h>
#include <sqlite3.h>
#include "database.h"

sqlite3 *sqlite_db;

sqlite3 *db_sqlite_init() {
	sqlite3 *db;
	
	printf("[+] sqlite: loading <%s>\n", SQL_DATABASE_FILE);
	
	sqlite3_config(SQLITE_CONFIG_SERIALIZED);
	
	if(sqlite3_open(SQL_DATABASE_FILE, &db) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: cannot open sqlite databse: <%s>\n", sqlite3_errmsg(db));
		return NULL;
	}
	
	sqlite3_busy_timeout(db, 30000);
	
	printf("[+] sqlite: database loaded\n");
	
	return db;
}

sqlite3_stmt *db_sqlite_select_query(sqlite3 *db, char *sql) {
	sqlite3_stmt *stmt;
	
	/* Debug SQL */
	printf("[+] sqlite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return NULL;
	}
	
	return stmt;
}

int db_sqlite_simple_query(sqlite3 *db, char *sql) {
	/* Debug SQL */
	printf("[+] sqlite: <%s>\n", sql);
	
	/* Query */
	if(sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: query <%s> failed: %s\n", sql, sqlite3_errmsg(db));
		return 0;
	}
	
	return 1;
}

unsigned int db_sqlite_num_rows(sqlite3_stmt *stmt) {
	unsigned int nbrows = 0;
	
	while(sqlite3_step(stmt) != SQLITE_DONE)
		nbrows++;
	
	sqlite3_finalize(stmt);
	
	return nbrows;
}

int db_sqlite_close(sqlite3 *db) {
	int err;
	
	if((err = sqlite3_close(db)) != SQLITE_OK) {
		fprintf(stderr, "[-] sqlite: cannot close database: %s\n", sqlite3_errmsg(db));
		
	} else printf("[+] sqlite: database closed.\n");
	
	return err;
}

