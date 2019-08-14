PROG = xsct
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c99 -O2 -I /usr/X11R6/include
LDFLAGS = -L /usr/X11R6/lib -lX11 -lXrandr -lm -s
SRCS = xsct.c
PREFIX = /usr
BIN = $(PREFIX)/bin
MAN = $(PREFIX)/share/man/man1
INSTALL = install

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

install: $(PROG) $(PROG).1
	$(INSTALL) -d $(BIN)
	$(INSTALL) -m 0755 $(PROG) $(BIN)
	$(INSTALL) -d $(MAN)
	$(INSTALL) -m 0644 $(PROG).1 $(MAN)

uninstall:
	rm -f $(BIN)/$(PROG)
	rm -f $(MAN)/$(PROG).1

clean:
	rm -f $(PROG)
