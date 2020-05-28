VERSION = 1.0
ABI = 1

CC = gcc
CXX = g++
GAWK = gawk
INSTALL = install
LN_S = ln -s

CXXSTD = -std=gnu++2a
DEFINES =
DEBUG = -g
OPTS =
INCLUDES = $(INCLUDES-$@)
CPPFLAGS = $(INCLUDES) $(DEFINES)
CXXFLAGS = $(CXXSTD) $(DEBUG) $(OPTS) $(CXXFLAGS-$@)
LDFLAGS = $(DEBUG) $(OPTS) $(CXXFLAGS-$@) $(LDFLAGS-$@)
LIBS = $(LIBS-$@)

prefix = /usr
includedir = $(prefix)/include/streamdeckpp-$(VERSION)
libdir = $(prefix)/$(shell $(CXX) -print-file-name=libc.so | $(GAWK) -v FS='/' '{ print($$(NF-1)) }')
pcdir= $(libdir)/pkgconfig

IFACEPKGS = hidapi-libusb
DEPPKGS = Magick++
ALLPKGS = $(IFACEPKGS) $(DEPPKGS)

INCLUDES-main.o = $(shell pkg-config --cflags $(IFACEPKGS))
INCLUDES-streamdeckpp.o = $(shell pkg-config --cflags $(ALLPKGS))
INCLUDES-streamdeckpp.os = $(shell pkg-config --cflags $(ALLPKGS))
LIBS-streamdeck = $(shell pkg-config --libs $(ALLPKGS))
LIBS-libstreamdeckpp.so = $(shell pkg-config --libs $(ALLPKGS))

all: streamdeck libstreamdeckpp.so libstreamdeckpp.a

.cc.os:
	$(COMPILE.cc) -fpic -o $@ $<

streamdeck: main.o libstreamdeckpp.a
	$(LINK.cc) -o $@ $^ $(LIBS)

main.o streamdeckpp.o: streamdeckpp.hh

libstreamdeckpp.a: streamdeckpp.o
	$(AR) $(ARFLAGS) $@ $?

libstreamdeckpp.so: streamdeckpp.os
	$(LINK.cc) -shared -o $@ $(LIBS-$@) $(LIBS)

streamdeckpp.pc: Makefile
	cat > $@-tmp <<EOF
	prefix=$(prefix)
	includedir=$(includedir)
	libdir=$(libdir)

	Name: StreamDeck++
	Decription: C++ Library to interace with StreamDeck devices
	Version: $(VERSION)
	URL: https://github.com/drepper/streamdeckpp
	Requires: $(IFACEPKGS)
	Requires.private: $(DEPPKGS)
	Cflags: -I$(includedir)
	Libs: -L$(libdir) -lstreamdeckpp
	EOF
	mv -f $@-tmp $@

install: libstreamdeckpp.a streamdeckpp.pc
	$(INSTALL) -D -c -m 755 libstreamdeckpp.so $(DESTDIR)$(libdir)/libstreamdeckpp-$(VERSION).so
	$(LN_S) -f libstreamdeckpp-$(VERSION).so $(DESTDIR)$(libdir)/libstreamdeckpp.so.$(ABI)
	$(LN_S) -f libstreamdeckpp.so.$(ABI) $(DESTDIR)$(libdir)/libstreamdeckpp.so
	$(INSTALL) -D -c -m 644 libstreamdeckpp.a $(DESTDIR)$(libdir)/libstreamdeckpp.a
	$(INSTALL) -D -c -m 644 streamdeckpp.pc $(DESTDIR)$(pcdir)/streamdeckpp.pc
	$(INSTALL) -D -c -m 644 streamdeckpp.hh $(DESTDIR)$(includedir)/streamdeckpp.hh

clean:
	rm -f streamdeck main.o streamdeckpp.os libstreamdeckpp.so streamdeckpp.o libstreamdeckpp.a \
	      streamdeckpp.pc

.PHONY: all install clean
.SUFFIXES: .os
.ONESHELL:
