CC = gcc
CFLAGS = -I. -g
.SUFFIXES: .c .o

all: oss usrPs

oss: oss.o
	$(CC) $(CFLAGS) -o $@ oss.o

usrPs: userPs.o
	$(CC) $(CFLAGS) -o $@ userPs.o

.c.o:
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm -f *.o oss usrPs
