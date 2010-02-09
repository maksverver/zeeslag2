CFLAGS=-O2 -g -Wall -Wextra -std=c99
LDFLAGS=-lpthread

all: solver server

server.6:  server.go;  6g -o $@ $<
server:    server.6;   6l -o $@ $<

clean:
	rm -f server.6

distclean: clean
	rm -f solver server

test:
	./run-tests.sh < tests/test-competition-2.in | diff - tests/test-competition-2.out
	./run-tests.sh < tests/hard-cases.in | diff - tests/hard-cases.out

.PHONY: all clean distclean test
