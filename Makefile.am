AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

AM_CFLAGS = -I. -Wall -std=c99 $(EXTRA_CFLAGS)
AM_LDFLAGS = -lm

bin_PROGRAMS = h264_analyze svc_split

lib_LTLIBRARIES = libh264bitstream.la

libh264bitstream_la_LDFLAGS = -no-undefined
libh264bitstream_la_SOURCES = src/h264_stream.c src/h264_sei.c src/h264_nal.c

h264_analyze_SOURCES = src/h264_analyze.c
h264_analyze_LDADD = libh264bitstream.la

svc_split_SOURCES = src/svc_split.c
svc_split_LDADD = libh264bitstream.la

include_HEADERS = src/h264_stream.h src/h264_sei.h src/h264_avcc.h
pkginclude_HEADERS = src/h264_stream.h src/h264_sei.h src/h264_avcc.h src/bs.h

clean-local:
	rm -rf *.pc
	rm -f libh264bitstream-uninstalled.sh libh264bitstream.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libh264bitstream.pc
