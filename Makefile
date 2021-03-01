# Makefile for Project 2 blather of CSci 4061
# Default target is blather

CLFLAGS = -lpthread
CFLAGS	= -Wall -g 
CC	= gcc $(CFLAGS)

# default

blather: bl-client bl-server bl-showlog
	@echo blather ready

bl-client: bl-client.o util.o simpio.o blather.h
	$(CC) -o $@ $^ $(CLFLAGS)

bl-server: bl-server.o util.o server.o blather.h
	$(CC) -o $@ $^ $(CLFLAGS)

bl-showlog: bl-showlog.o util.o blather.h
	$(CC) -o $@ $^

bl-showlog.o: bl-showlog.c blather.h
	$(CC) -c $<

bl-client.o: bl-client.c blather.h
	$(CC) -c $<

bl-server.o: bl-server.c blather.h
	$(CC) -c $<

server.o: server.c blather.h
	$(CC) -c $<

simpio.o: simpio.c blather.h
	$(CC) -c $<

util.o: util.c blather.h
	$(CC) -c $<

clean:
	@echo Cleaning up object files and test outputs
	rm -f *.o *.fifo *.log

clean_all: clean
	@echo Removing objects and programs
	rm -f bl-client bl-server bl-showlog
