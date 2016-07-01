EXEC = sms-gateway

VERSION = 0.2
CFLAGS  = -W -Wall -O2 -pipe -ansi -std=gnu99 -pthread -g
LDFLAGS = -lsqlite3 -lm

CC = gcc
