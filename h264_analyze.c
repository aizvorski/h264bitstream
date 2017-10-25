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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "h264_stream.h"

#define BUFSIZE 32*1024*1024

#if (defined(__GNUC__))
#define HAVE_GETOPT_LONG

#include <getopt.h>

static struct option long_options[] =
{
    { "probe",   no_argument, NULL, 'p'},
    { "output",  required_argument, NULL, 'o'},
    { "help",    no_argument,       NULL, 'h'},
    { "verbose", required_argument, NULL, 'v'},
};
#endif

static char options[] =
"\t-o output_file, defaults to test.264\n"
"\t-v verbose_level, print more info\n"
"\t-p print codec for HTML5 video tag's codecs parameter, per RFC6381\n"
"\t-h print this message and exit\n";

void usage()
{
    fprintf( stderr, "h264_analyze, version 0.2.0\n");
    fprintf( stderr, "Analyze H.264 bitstreams in Annex B format\n");
    fprintf( stderr, "Usage: \n");
    fprintf( stderr, "h264_analyze [options] <input bitstream>\noptions:\n%s\n",
             options);
}

int main(int argc, char *argv[])
{
    FILE* infile = NULL;
    uint8_t* buf = NULL;
    h264_stream_t* h = NULL;
    int opt_verbose = 1;
    int opt_probe = 0;

    buf = (uint8_t*)malloc( BUFSIZE );
    if (buf == NULL) {
      printf("!! Error: Alloc for buffer failed\n");
      return EXIT_FAILURE;
    }

    h = h264_new();
    if (h == NULL) {
      printf("!! Error: Alloc for h264 object failed\n");
      return EXIT_FAILURE;
    }

    if (argc < 2) {
      usage();
      return EXIT_FAILURE;
    }

#ifdef HAVE_GETOPT_LONG
    int long_options_index = 0;
    extern char* optarg;
    extern int optind;
    int c = 0;

    while ( ( c = getopt_long( argc, argv, "o:phv:", long_options,
                               &long_options_index) ) != -1 )
    {
        switch ( c )
        {
            case 'o':
                if (h264_dbgfile == NULL) {
                  h264_dbgfile = fopen( optarg, "wt");
                }
                break;

            case 'p':
                opt_probe = 1;
                opt_verbose = 0;
                break;

            case 'v':
                opt_verbose = atoi( optarg );
                break;

            case 'h':

            default:
                usage( );
                return 1;
        }
    }

    infile = fopen(argv[optind], "rb");
#else
    infile = fopen(argv[1], "rb");
#endif

    if (infile == NULL) {
      fprintf( stderr,
               "!! Error: Could not open file: %s \n",
               strerror(errno));
               exit(EXIT_FAILURE);
    }

    if (h264_dbgfile == NULL) {
      h264_dbgfile = stdout;
    }

    uint8_t* p = buf;
    int64_t off = 0;
    size_t rsz = 0;
    size_t sz = 0;
    int nal_start;
    int nal_end;

    while (1)
    {
        rsz = fread(buf + sz, 1, BUFSIZE - sz, infile);
        if (rsz == 0) {
            if (ferror(infile)) {
              fprintf( stderr, "!! Error: read failed: %s \n", strerror(errno));
              break;
            }
            break;  // if (feof(infile))
        }
        sz += rsz;

        while (find_nal_unit(p, sz, &nal_start, &nal_end) > 0)
        {
            if ( opt_verbose > 0 ) {
               fprintf( h264_dbgfile,
                        "!! Found NAL at offset %lld (0x%04llX), size %lld (0x%04llX) \n",
                        (long long int)(off + (p - buf) + nal_start),
                        (long long int)(off + (p - buf) + nal_start),
                        (long long int)(nal_end - nal_start),
                        (long long int)(nal_end - nal_start) );
            }

            p += nal_start;
            read_debug_nal_unit(h, p, nal_end - nal_start);

            if ( opt_probe && h->nal->nal_unit_type == NAL_UNIT_TYPE_SPS ) {
                // print codec parameter, per RFC 6381.
                int constraint_byte = h->sps->constraint_set0_flag << 7;
                constraint_byte = h->sps->constraint_set1_flag << 6;
                constraint_byte = h->sps->constraint_set2_flag << 5;
                constraint_byte = h->sps->constraint_set3_flag << 4;
                constraint_byte = h->sps->constraint_set4_flag << 3;
                constraint_byte = h->sps->constraint_set4_flag << 3;
                fprintf( h264_dbgfile,
                         "codec: avc1.%02X%02X%02X\n",h->sps->profile_idc,
                         constraint_byte,
                         h->sps->level_idc );

                // TODO: add more, move to h264_stream (?)
                break; // we've seen enough, bailing out.
            }

// NOTE: undefined reference to `debug_nal'
#if 0
            if ( opt_verbose > 0 ) {
              fprintf( h264_dbgfile, "XX ");
              debug_bytes(p-4,
                          nal_end - nal_start + 4 >= 16 ? 16: nal_end - nal_start + 4);
              debug_nal(h, h->nal);
            }
#endif
            p += (nal_end - nal_start);
            sz -= nal_end;
        }

        // if no NALs found in buffer, discard it
        if (p == buf) {
            fprintf( stderr,
                     "!! Did not find any NALs between offset %lld (0x%04llX), size %lld (0x%04llX), discarding \n",
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
    fclose(h264_dbgfile);
    fclose(infile);

    return 0;
}
