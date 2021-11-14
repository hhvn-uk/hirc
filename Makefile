# See LICENSE for copyright details

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
BIN	= hirc
OBJ	= main.o handle.o hist.o nick.o \
	  chan.o serv.o ui.o commands.o \
	  config.o
MAN	= hirc.1

# Comment to disable TLS
LDTLS	= -ltls
CTLS	= -DTLS

CFLAGS	= -g -O0 $(CTLS)
LDFLAGS = -lncursesw $(LDTLS)

all: $(BIN) $(MAN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

$(MAN): $(BIN) $(MAN).header $(MAN).footer
	./$(BIN) -d | cat $(MAN).header - $(MAN).footer > $(MAN)

clean:
	-rm -f $(OBJ) $(MAN) $(BIN)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJ):
