VERSION = 1.1
ABI = 1

CXX = g++ $(CXXSTD)
GAWK = gawk
INSTALL = install
CAT = cat
LN_FS = ln -fs
SED = sed
TAR = tar
MV_F = mv -f
RM_F = rm -f
RPMBUILD = rpmbuild

CXXSTD = -std=gnu++2a
DEFINES =
DEBUG = -g
OPTS =
WARN = -Wall
INCLUDES = $(INCLUDES-$@)
CPPFLAGS = $(INCLUDES) $(DEFINES)
CXXFLAGS = $(DEBUG) $(OPTS) $(CXXFLAGS-$@) $(WARN)
LDFLAGS = $(LDFLAGS-$@)
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

main.o streamdeckpp.o streamdeckpp.os: streamdeckpp.hh

libstreamdeckpp.a: streamdeckpp.o
	$(AR) $(ARFLAGS) $@ $?

libstreamdeckpp.so: streamdeckpp.os
	$(LINK.cc) -shared -Wl,-h,libstreamdeckpp.so.$(ABI) -o $@ $^ $(LIBS)

streamdeckpp.pc: Makefile
	$(CAT) > $@-tmp <<EOF
	prefix=$(prefix)
	includedir=$(includedir)
	libdir=$(libdir)

	Name: StreamDeck++
	Description: C++ Library to interace with StreamDeck devices
	Version: $(VERSION)
	URL: https://github.com/drepper/streamdeckpp
	Requires: $(IFACEPKGS)
	Requires.private: $(DEPPKGS)
	Cflags: -I$(includedir)
	Libs: -L$(libdir) -lstreamdeckpp
	EOF
	$(MV_F) $@-tmp $@

streamdeckpp.spec: streamdeckpp.spec.in Makefile
	$(SED) 's/@VERSION@/$(VERSION)/;s/@ABI@/$(ABI)/' $< > $@-tmp
	$(MV_F) $@-tmp $@

install: libstreamdeckpp.a streamdeckpp.pc
	$(INSTALL) -D -c -m 755 libstreamdeckpp.so $(DESTDIR)$(libdir)/libstreamdeckpp-$(VERSION).so
	$(LN_FS) libstreamdeckpp-$(VERSION).so $(DESTDIR)$(libdir)/libstreamdeckpp.so.$(ABI)
	$(LN_FS) libstreamdeckpp.so.$(ABI) $(DESTDIR)$(libdir)/libstreamdeckpp.so
	$(INSTALL) -D -c -m 644 libstreamdeckpp.a $(DESTDIR)$(libdir)/libstreamdeckpp.a
	$(INSTALL) -D -c -m 644 streamdeckpp.pc $(DESTDIR)$(pcdir)/streamdeckpp.pc
	$(INSTALL) -D -c -m 644 streamdeckpp.hh $(DESTDIR)$(includedir)/streamdeckpp.hh

dist: streamdeckpp.spec
	$(LN_FS) . streamdeckpp-$(VERSION)
	$(TAR) achf streamdeckpp-$(VERSION).tar.xz streamdeckpp-$(VERSION)/{Makefile,streamdeckpp.hh,streamdeckpp.cc,main.cc,README.md,streamdeckpp.spec,streamdeckpp.spec.in}
	$(RM_F) streamdeckpp-$(VERSION)

srpm: dist
	$(RPMBUILD) -ts streamdeckpp-$(VERSION).tar.xz
rpm: dist
	$(RPMBUILD) -tb streamdeckpp-$(VERSION).tar.xz

clean:
	$(RM_F) streamdeck main.o streamdeckpp.os libstreamdeckpp.so streamdeckpp.o libstreamdeckpp.a \
	        streamdeckpp.pc streamdeckpp.spec

.PHONY: all install dist srpm rpm clean
.SUFFIXES: .os
.ONESHELL:
