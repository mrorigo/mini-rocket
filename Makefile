LD=gcc
CC=gcc
CFLAGS=-O0 -ggdb -Wall -Wpedantic
LIBS=-lm -lws2_32

example.exe: example.o mini-rocket.o
	$(LD) $(LDFLAGS) -o $@ $<  mini-rocket.o $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean: 
	rm -f example.exe mini-rocket.o
