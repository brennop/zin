CC=gcc
CFLAGS=-Wall -std=c99 -pthread

all: http

http: http.o queue.o
	$(CC) $(CFLAGS) -o http http.o queue.o

clean:
	rm -f http *.o

run: http
		./http
