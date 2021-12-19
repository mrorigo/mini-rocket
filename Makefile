LD=gcc
CC=gcc

LIBS=-lm -lws2_32

mini-rocket.exe: mini-rocket.o
	$(LD) $(LDFLAGS) -o $@ $<  $(LIBS)

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

clean: 
	rm -f mini-rocket.exe mini-rocket.o
