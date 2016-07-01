CREATE TABLE raw (id integer primary key, arrived datetime, pdu text);
CREATE TABLE messages (id integer primary key, number varchar(32), message varchar(2048), received datetime, read int);
CREATE TABLE pending (id integer primary key, number varchar(32), message varchar(1024), sent integer default 0, type integer default 0, added datetime default CURRENT_TIMESTAMP, departure datetime default NULL);
CREATE TABLE failed (id integer primary key, pdu varchar(2048), added datetime default current_timestamp);
CREATE TABLE segments (id integer primary key, number varchar(32), message varchar(2048), received datetime, partid integer, part integer, total integer);
