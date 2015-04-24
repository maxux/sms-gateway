CREATE TABLE messages (id integer primary key, number varchar(32), message varchar(2048), received datetime, read int);
CREATE TABLE pending (id integer primary key, number varchar(32), message varchar(1024), sent integer, type int);
