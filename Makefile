VERSION=1.10.1
CFLAGS+=-g -DVERSION='"$(VERSION)"' -Wall -Wextra -Werror -Wno-unused-parameter

# '-static' not supported on macOS due to lack of 'crt0.o'
# https://lists.sr.ht/~sircmpwn/public-inbox/%3C3A12328E-5EC6-4E34-A676-64A3039A2AF7%40herrbischoff.com%3E
UNAME := $(shell uname)
ifneq ($(UNAME), Darwin)
	LDFLAGS+=-static
endif

INCLUDE+=-Iinclude
PREFIX?=/usr/local
_INSTDIR=$(DESTDIR)$(PREFIX)
BINDIR?=$(_INSTDIR)/bin
MANDIR?=$(_INSTDIR)/share/man
PCDIR?=$(_INSTDIR)/lib/pkgconfig
OUTDIR=.build
HOST_SCDOC=./scdoc
.DEFAULT_GOAL=all

OBJECTS=\
	$(OUTDIR)/main.o \
	$(OUTDIR)/string.o \
	$(OUTDIR)/utf8_chsize.o \
	$(OUTDIR)/utf8_decode.o \
	$(OUTDIR)/utf8_encode.o \
	$(OUTDIR)/utf8_fgetch.o \
	$(OUTDIR)/utf8_fputch.o \
	$(OUTDIR)/utf8_size.o \
	$(OUTDIR)/util.o

$(OUTDIR)/%.o: src/%.c
	@mkdir -p $(OUTDIR)
	$(CC) -std=c99 -pedantic -c -o $@ $(CFLAGS) $(INCLUDE) $<

scdoc: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

scdoc.1: scdoc.1.scd $(HOST_SCDOC)
	$(HOST_SCDOC) < $< > $@

scdoc.5: scdoc.5.scd $(HOST_SCDOC)
	$(HOST_SCDOC) < $< > $@

scdoc.pc: scdoc.pc.in
	sed -e 's:@prefix@:$(PREFIX):g' -e 's:@version@:$(VERSION):g' < $< > $@

all: scdoc scdoc.1 scdoc.5 scdoc.pc

clean:
	rm -rf $(OUTDIR) scdoc scdoc.1 scdoc.5 scdoc.pc

install: all
	mkdir -p $(BINDIR) $(MANDIR)/man1 $(MANDIR)/man5 $(PCDIR)
	install -m755 scdoc $(BINDIR)/scdoc
	install -m644 scdoc.1 $(MANDIR)/man1/scdoc.1
	install -m644 scdoc.5 $(MANDIR)/man5/scdoc.5
	install -m644 scdoc.pc $(PCDIR)/scdoc.pc

check: scdoc scdoc.1 scdoc.5
	@find test -perm -111 -exec '{}' \;

.PHONY: all clean install check
