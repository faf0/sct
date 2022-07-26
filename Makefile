CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I /usr/X11R6/include
LDFLAGS ?= -L /usr/X11R6/lib -s
PREFIX ?= /usr
BIN ?= $(PREFIX)/bin
MAN ?= $(PREFIX)/share/man/man1
INSTALL ?= install

PROG = xsct
SRCS = xsct.c

LIBS = -lX11 -lXrandr -lm

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

install: $(PROG) $(PROG).1
	$(INSTALL) -d $(DESTDIR)$(BIN)
	$(INSTALL) -m 0755 $(PROG) $(DESTDIR)$(BIN)
	$(INSTALL) -d $(DESTDIR)$(MAN)
	$(INSTALL) -m 0644 $(PROG).1 $(DESTDIR)$(MAN)

uninstall:
	rm -f $(BIN)/$(PROG)
	rm -f $(MAN)/$(PROG).1

clean:
	rm -f $(PROG)
