# See LICENSE for copyright details

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/share/man
BIN	= hirc
OBJ	= main.o handle.o hist.o nick.o \
	  chan.o serv.o ui.o commands.o \
	  config.o strlcpy.o
MAN	= hirc.1
COMMIT	= $(shell git log HEAD...HEAD~1 --pretty=format:%h)

# Comment to disable TLS
LDTLS	= -ltls
CTLS	= -DTLS

CFLAGS	= -g -O0 $(CTLS)
LDFLAGS = -lncursesw $(LDTLS)

all: $(BIN) $(MAN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

$(MAN): $(BIN) $(MAN).header $(MAN).footer
	./$(BIN) -d | \
		cat $(MAN).header - $(MAN).footer | \
		sed "s/COMMIT/$(COMMIT)/" > $(MAN)

install: all
	mkdir -p $(BINDIR) $(MANDIR)/man1
	install -m0755 $(BIN) $(BINDIR)/$(BIN)
	sed 's/COMMIT/$(COMMIT)/' \
		< $(MAN) \
		> $(MANDIR)/man1/$(MAN)

uninstall:
	-rm -f $(BINDIR)/$(BIN)
	-rm -f $(MANDIR)/man1/$(MAN)

clean:
	-rm -f $(OBJ) $(MAN) $(BIN)

.c.o:
	$(CC) $(CFLAGS) -c $<

$(OBJ):
