CFLAGS=-O2 -Wall -Wpedantic
LIBS = -lm
ifeq ($(OS),Windows_NT)
    LIBS += -lws2_32
    CFLAGS += -D WINDOWS
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        CFLAGS += -D LINUX
    endif
    ifeq ($(UNAME_S),Darwin)
        CFLAGS += -D OSX
    endif
endif
LD=gcc
CC=gcc
example.exe: example.o mini-rocket.o
	$(LD) $(LDFLAGS) -o $@ $<  mini-rocket.o $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean: 
	rm -f example mini-rocket.o
