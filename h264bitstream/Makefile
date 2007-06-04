VERSION = 0.1.7

INCLUDES = 
LIBS = 

CC = gcc
CFLAGS += -std=c99 -pedantic -Wall -W -Wshadow -Wwrite-strings -Wno-unused -g $(INCLUDES)

LD = gcc
LDFLAGS += $(LIBS)

AR = ar
ARFLAGS = rsc


all: libh264bitstream.a h264_analyze


h264_analyze: h264_analyze.c libh264bitstream.a
	$(CC) $(CFLAGS) -c -o h264_analyze.o h264_analyze.c
	$(LD) $(LDFLAGS) -o h264_analyze h264_analyze.o -L. -lm -lh264bitstream


libh264bitstream.a: h264_stream.c h264_stream.h h264_sei.c h264_sei.h bs.c bs.h
	$(CC) $(CFLAGS) -c -o bs.o bs.c
	$(CC) $(CFLAGS) -c -o h264_stream.o h264_stream.c
	$(CC) $(CFLAGS) -c -o h264_sei.o h264_sei.c
	$(AR) $(ARFLAGS) libh264bitstream.a h264_stream.o h264_sei.o bs.o


clean:
	rm -f *.o libh264bitstream.a h264_analyze


dist: clean
	mkdir h264bitstream-$(VERSION)
	tar c --files-from=MANIFEST -f tmp.tar ; cd h264bitstream-$(VERSION) ; tar xf ../tmp.tar ; rm -f ../tmp.tar
	tar czf ../h264bitstream-$(VERSION).tar.gz h264bitstream-$(VERSION)
	rm -rf h264bitstream-$(VERSION)


dox: h264_stream.c h264_stream.h bs.c bs.h Doxyfile
	doxygen Doxyfile
