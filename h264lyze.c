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
#include <unistd.h>
#include "h264_stream.h"

// TODO: Apply same convention to all defines
#define BUFSIZE 32*1024*1024
#define H264LYZE_MIN_ARGC 2

static char h264lyze_options[] =
"\t (null): top level parse of the file\n"
"\t --stats: top level h264 stats of the file\n"
"\t --full: full high-low level parse of the file + stats\n"
"\t --type <type> [n]: NAL type parse, all or single\n";

// Prints program usage
void h264lyze_usage()
{
  fprintf( stdout, "\nh264lyze:\n");
  fprintf( stdout, "An h264_analyze variant\n");
  fprintf( stdout, "Analyze H.264 bitstreams in Annex B format\n");
  fprintf( stdout, "\nUsage:\n");
  fprintf( stdout, "h264lyze <input-bitstream> [option]\n");
  fprintf( stdout, "\nOptions:\n%s\n",
  h264lyze_options);
}

// Internal h264lyze object type
typedef struct h264lyze {
  char *infile;
  uint8_t option
} h264lyze_t;

// h264lyze user option enum
enum h264lyze_user_opt {
  H264LYZE_OPT_TOP = 0,
  H264LYZE_OPT_STATS,
  H264LYZE_OPT_FULL,
  H264LYZE_OPT_TYPE
};

// Generates h264 output
int h264lyze_process(h264lyze_t *h264lyze)
{
  int status = 0;
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


  infile = fopen(h264lyze->infile, "rb");
  // TODO: Check fopen return code
  uint8_t* p = buf;
  int64_t off = 0;
  size_t rsz = 0;
  size_t sz = 0;
  int nal_start;
  int nal_end;
  uint32_t nal_num = 0;
  int64_t nal_size = 0;
  int64_t nal_off = 0;

  while (1)
  {
      rsz = fread(buf + sz, 1, BUFSIZE - sz, infile);
      if (rsz == 0) {
          if (ferror(infile)) {
            printf( "!! Error: read failed: %s \n", strerror(errno));
            break;
          }
          break;  // if (feof(infile))
      }
      sz += rsz;

      while (find_nal_unit(p, sz, &nal_start, &nal_end) > 0)
      {
          nal_num++;
          p += nal_start;
          // h264lyze_nal_unit_type_get
          h264lyze_read_debug_nal_unit (h, p, nal_end - nal_start);

          if ( h264lyze->option == H264LYZE_OPT_TOP || h264lyze->option == H264LYZE_OPT_FULL ) {
            // Num NAL Type Offset Size Accum
             printf(  ">> %u NAL: addr 0X%04llX size %lld type %u\n",
                      nal_num,
                      (long long int)(off + (p - buf) + nal_start),
                      (long long int)(nal_end - nal_start), h->nal->nal_unit_type);
          }

          if ( h264lyze->option == H264LYZE_OPT_FULL ) {
            read_debug_nal_unit (h, p, nal_end - nal_start);
          }

          p += (nal_end - nal_start);
          sz -= nal_end;
      }

      // if no NALs found in buffer, discard it
      if (p == buf) {
          printf(  "(No NAL (0x%04llX) (0x%04llX). Discard\n",
                   (long long int)off,
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
}

// Checks user argments and populates h264 object
int h264lyze_input_check(int argc, char *argv[], h264lyze_t *h264lyze)
{
  int status = 0;
  const char *infile = NULL;

  // Check if user supplies minimum arg (h264 file)
  if (argc < H264LYZE_MIN_ARGC) {
    h264lyze_usage();
    return EXIT_FAILURE;
  }

  // Checking argv1 as h264 file
  infile = argv[1];
  status = access(infile, F_OK);
  if ( status != 0 ) {
    printf("Error: File locate %s failed\n", infile);
    h264lyze_usage();
    return EXIT_FAILURE;
  } else {
    // Populate h264 object
    h264lyze->infile = infile;
  }

  // Checking argv2 as option/mode, if option is valid populate h264 object
  if (argc > 2) {
    status = strcmp(argv[2],"--stats");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_STATS;
      return status;
    }
    status = strcmp(argv[2],"--full");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_FULL;
      return status;
    }
    status = strcmp(argv[2],"--type");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_TYPE;
      return status;
    } else {
      printf("Error: Invalid option %s\n", argv[2]);
      h264lyze_usage();
      return EXIT_FAILURE;
    }
  } else {
    h264lyze->option = H264LYZE_OPT_TOP;
  }

  return status;
}

int main(int argc, char *argv[])
{
  int status = 0;
  h264lyze_t h264lyze;
  status = h264lyze_input_check(argc, argv, &h264lyze);
  if (status) return status;

  status = h264lyze_process(&h264lyze);

  return status;
}
