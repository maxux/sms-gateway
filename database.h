#ifndef DATABASE_H
	#define DATABASE_H

	#include <sqlite3.h>
	
	#define SQL_DATABASE_FILE    "/tmp/ramdisk/pending.sqlite3"
	
	extern sqlite3 *sqlite_db;
	
	sqlite3 *db_sqlite_init();
	int db_sqlite_close(sqlite3 *db);
	
	sqlite3_stmt *db_sqlite_select_query(sqlite3 *db, char *sql);
	int db_sqlite_simple_query(sqlite3 *db, char *sql);
	unsigned int db_sqlite_num_rows(sqlite3_stmt *stmt);
#endif
