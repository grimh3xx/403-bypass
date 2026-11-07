# 403-bypass — Makefile
CC      ?= cc
# -Wno-format-truncation: todo snprintf aqui e limitado de proposito; a
# truncacao segura e aceitavel e nao indica bug.
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-format-truncation \
           -D_POSIX_C_SOURCE=200809L
LDFLAGS ?=
LDLIBS  := -lcurl -lpthread

# Permite descobrir flags do curl via pkg-config quando disponivel.
PKG := $(shell pkg-config --exists libcurl 2>/dev/null && echo yes)
ifeq ($(PKG),yes)
CFLAGS += $(shell pkg-config --cflags libcurl)
LDLIBS := $(shell pkg-config --libs libcurl) -lpthread
endif

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
BIN := 403-bypass
PREFIX  ?= /usr/local
MANDIR  ?= $(PREFIX)/share/man/man1
PYTHON  ?= python3

.PHONY: all clean install uninstall test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(BIN)
	$(PYTHON) tests/test_cli.py ./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -d $(DESTDIR)$(MANDIR)
	install -m 0644 docs/403-bypass.1 $(DESTDIR)$(MANDIR)/403-bypass.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN)
	rm -f $(DESTDIR)$(MANDIR)/403-bypass.1

clean:
	rm -f $(OBJ) $(BIN)
