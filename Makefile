CFLAGS=-O2 -g -Wall -Wextra -std=c99
LDFLAGS=-lpthread

all: solver server

server.6:  server.go;  6g -o $@ $<
server:    server.6;   6l -o $@ $<
