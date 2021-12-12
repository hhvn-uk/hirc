# See LICENSE for copyright details

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/share/man
BIN	= hirc
OBJ	= src/main.o src/handle.o src/hist.o src/nick.o \
	  src/chan.o src/serv.o src/ui.o src/commands.o \
	  src/config.o src/strlcpy.o
MAN	= hirc.1
COMMIT	= $(shell git log HEAD...HEAD~1 --pretty=format:%h)
CFLAGS	= -g -O0 $(CTLS)
LDFLAGS = -lncursesw

include config.mk

all: $(BIN) $(MAN)

# Some make implementation will
# use a target to create an include
# file if it doesn't yet exist,
# but this is not a standard feature.
config.mk:
	./configure

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
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean install uninstall
