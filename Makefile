CC = gcc
CXX = g++

CXXSTD = -std=gnu++2a
DEFINES =
DEBUG = -g
OPTS =
INCLUDES = $(INCLUDES-$@)
CPPFLAGS = $(INCLUDES) $(DEFINES)
CXXFLAGS = $(CXXSTD) $(DEBUG) $(OPTS) $(CXXFLAGS-$@)
LDFLAGS = $(DEBUG) $(OPTS) $(CXXFLAGS-$@) $(LDFLAGS-$@)
LIBS = $(LIBS-$@)


INCLUDES-main.o = $(shell pkg-config --cflags hidapi-libusb Magick++)
LIBS-streamdeck = $(shell pkg-config --libs hidapi-libusb Magick++)

all: streamdeck

streamdeck: main.o
	$(LINK.cc) -o $@ $^ $(LIBS)


clean:
	rm -f streamdeck main.o

.PHONY: all clean
