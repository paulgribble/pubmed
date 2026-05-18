CFLAGS  ?= -std=c99 -Wall -Wextra -O2
PKGS     = libxml-2.0 libcurl
CFLAGS  += $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS))

.PHONY: all clean
all: pubmed

pubmed: pubmed.c
	$(CC) $(CFLAGS) $< $(LDLIBS) -o $@

clean:
	rm -f pubmed
