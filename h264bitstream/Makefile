VERSION = 0.1.5


CC = gcc
CFLAGS = -g
# CFLAGS = -O3

AR = ar
ARFLAGS = qc


all: libh264bitstream.a
	$(CC) $(CFLAGS) -o h264_analyze h264_analyze.c -L. -lm -lh264bitstream


libh264bitstream.a: h264_stream.c h264_stream.h bs.c bs.h
	$(CC) $(CFLAGS) -c -o bs.o bs.c
	$(CC) $(CFLAGS) -c -o h264_stream.o h264_stream.c
	$(AR) $(ARFLAGS) libh264bitstream.a h264_stream.o bs.o
	ranlib libh264bitstream.a


clean:
	rm -f *.o libh264bitstream.a h264_analyze


dist: clean
	mkdir h264bitstream-$(VERSION)
	tar c --files-from=MANIFEST -f tmp.tar ; cd h264bitstream-$(VERSION) ; tar xf ../tmp.tar ; rm -f ../tmp.tar
	tar czf ../h264bitstream-$(VERSION).tar.gz h264bitstream-$(VERSION)
	rm -rf h264bitstream-$(VERSION)

dox: h264_stream.c h264_stream.h bs.c bs.h Doxyfile
	doxygen Doxyfile
