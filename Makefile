CC=gcc
CFLAGS=-Wall -std=c99 -pthread

all: http

http: 

clean:
	rm -f http

run: http
		./http
