# misc/Makefile from hirc
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
BINS	= irccat
MANS	= irccat.1

include ../config.mk

all: $(BINS)
irccat: irccat.c

install:
	mkdir -p $(BINDIR) $(MANDIR)/man1
	install -m0755 $(BINS) $(BINDIR)/
	for f in $(MANS); do \
		sed 's/COMMIT/$(COMMIT)/' \
			< $$f \
			> $(MANDIR)/man1/$$f; \
	done

uninstall:
	for f in $(BINS); do \
		rm -f $(BINDIR)/$$f; \
	done
	for f in $(MANS); do \
		rm -f $(MANDIR)/man1/$$f; \
	done

clean:
	rm -f $(BINS)

.c:
	cc -o $(<:.c=) $<

.PHONY: install uninstall clean
