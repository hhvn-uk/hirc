# See LICENSE for copyright details

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
BIN	= hirc
OBJ	= main.o handle.o hist.o nick.o \
	  chan.o serv.o ui.o commands.o \
	  config.o

# Comment to disable TLS
LDTLS	= -ltls
CTLS	= -DTLS

CFLAGS	= -g -O0 $(CTLS)
LDFLAGS = -lncursesw $(LDTLS)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

clean:
	-rm -f $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJ):
