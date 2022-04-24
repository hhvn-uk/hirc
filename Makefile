# Makefile from hirc
#
# Copyright (c) 2021-2022 hhvn <dev@hhvn.uk>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
MANDIR	= $(PREFIX)/share/man
BIN	= hirc
SRC	= src/main.c src/mem.c src/handle.c src/hist.c \
	  src/nick.c src/chan.c src/serv.c src/ui.c \
	  src/format.c src/complete.c src/commands.c \
	  src/config.c src/str.c src/params.c
OBJ	= $(SRC:.c=.o)
MAN	= doc/hirc.1
COMMIT	= $(shell git log HEAD...HEAD~1 --pretty=format:%h)
CFLAGS	= $(DEBUG)
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

# Data/ headers
src/config.o: src/data/config.h
src/commands.o: src/data/commands.h
src/format.o: src/data/formats.h
src/handle.o: src/data/handlers.h

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

$(MAN): $(BIN) $(MAN).header $(MAN).footer
	./$(BIN) -d | \
		cat $(MAN).header - $(MAN).footer | \
		sed "s/COMMIT/$(COMMIT)/" > $(MAN)

misc:
	cd misc/ && make \
		CFLAGS="$(CFLAGS)"

misc-install:
	cd misc/ && make install \
		CFLAGS="$(CFLAGS)"
		PREFIX="$(PREFIX)" \
		BINDIR="$(BINDIR)" \
		MANDIR="$(MANDIR)"

misc-uninstall:
	cd misc/ && make uninstall \
		PREFIX="$(PREFIX)" \
		BINDIR="$(BINDIR)" \
		MANDIR="$(MANDIR)"

misc-clean:
	cd misc/ && make clean

install: all
	mkdir -p $(BINDIR) $(MANDIR)/man1
	install -m0755 $(BIN) $(BINDIR)/$(BIN)
	sed 's/COMMIT/$(COMMIT)/' \
		< $(MAN) \
		> $(MANDIR)/man1/`basename $(MAN)`

uninstall:
	-rm -f $(BINDIR)/$(BIN)
	-rm -f $(MANDIR)/man1/$(MAN)

clean: misc-clean
	-rm -f $(OBJ) $(MAN) $(BIN)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all misc clean install uninstall misc misc-install misc-uninstall misc-clean
