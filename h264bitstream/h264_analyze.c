/* 
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2007 Auroras Entertainment, LLC
 * 
 * Written by Alex Izvorski <aizvorski@gmail.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "h264_stream.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define BUFSIZE 32*1024*1024



int main(int argc, char *argv[])
{
    uint8_t* buf = (uint8_t*)malloc(BUFSIZE);
    h264_stream_t* h = h264_new();

    if (argc < 2)
    {
        printf("h264_analyze, version 0.1.7\n");
        printf("Usage: \n");
        printf("  h264_analyze stream.h264\n");
        printf("where stream.h264 is a raw H264 stream, as produced by JM or x264\n");
    }

    FILE* infile = fopen(argv[1], "rb");
    if (infile == NULL) { printf("!! Error: could not open file: %s \n", strerror(errno)); exit(0); }

    size_t rsz = 0;
    size_t sz = 0;
    int64_t off = 0;
    uint8_t* p = buf;

    int nal_start, nal_end;

    while (1)
    {
        rsz = fread(buf + sz, 1, BUFSIZE - sz, infile);
        if (rsz == 0)
        {
            if (ferror(infile)) { printf("!! Error: read failed: %s \n", strerror(errno)); break; }
            break;  // if (feof(infile)) 
        }

        sz += rsz;

        while (find_nal_unit(p, sz, &nal_start, &nal_end) > 0)
        {
            printf("!! Found NAL at offset %lld (0x%04llX), size %lld (0x%04llX) \n", 
                   (long long int)(off + (p - buf) + nal_start), 
                   (long long int)(off + (p - buf) + nal_start), 
                   (long long int)(nal_end - nal_start), 
                   (long long int)(nal_end - nal_start) );

            p += nal_start;
            read_nal_unit(h, p, nal_end - nal_start);

            printf("XX ");
            debug_bytes(p-4, nal_end - nal_start + 4 >= 16 ? 16: nal_end - nal_start + 4);

            debug_nal(h, h->nal);
            p += (nal_end - nal_start);
            sz -= nal_end;
        }

        // if no NALs found in buffer, discard it
        if (p == buf) 
        {
            printf("!! Did not find any NALs between offset %lld (0x%04llX), size %lld (0x%04llX), discarding \n", 
                   (long long int)off, 
                   (long long int)off, 
                   (long long int)off + sz, 
                   (long long int)off + sz);

            p = buf + sz;
            sz = 0;
        }

        memmove(buf, p, sz);
        off += p - buf;
        p = buf;
    }

    h264_free(h);
    free(buf);

    fclose(infile);

    return 0;
}
