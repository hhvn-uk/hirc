# See LICENSE for copyright details

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/share/man
BIN	= hirc
SRC	= src/main.c src/handle.c src/hist.c src/nick.c \
	  src/chan.c src/serv.c src/ui.c src/commands.c \
	  src/config.c
OBJ	= $(SRC:.c=.o)
MAN	= hirc.1
COMMIT	= $(shell git log HEAD...HEAD~1 --pretty=format:%h)
CFLAGS	= -g -O0 $(CTLS)
LDFLAGS = -lncursesw

include config.mk

all: $(BIN) $(MAN) misc

# Some make implementation will
# use a target to create an include
# file if it doesn't yet exist,
# but this is not a standard feature.
config.mk:
	./configure

# All objects should be rebuilt if
# struct.h changes, as, for example,
# if an enum changes value that will
# only be recognized in source files
# that have been changed aswell.
$(OBJ): src/struct.h

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

$(MAN): $(BIN) $(MAN).header $(MAN).footer
	./$(BIN) -d | \
		cat $(MAN).header - $(MAN).footer | \
		sed "s/COMMIT/$(COMMIT)/" > $(MAN)

misc:
	cd misc/ && make

misc-install:
	cd misc/ && make install \
		CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
		PREFIX="$(PREFIX)" \
		BINDIR="$(BINDIR)" \
		MANDIR="$(MANDIR)"

misc-uninstall:
	cd misc/ && make uninstall \
		PREFIX="$(PREFIX)" \
		BINDIR="$(BINDIR)" \
		MANDIR="$(MANDIR)"

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

.PHONY: all misc clean install uninstall
