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

.c:
	cc -o $(<:.c=) $<

.PHONY: install uninstall
