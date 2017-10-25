/*
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

/*
* h264bitstream - a library for reading and writing H.264 video
* Copyright (C) 2005-2007 Auroras Entertainment, LLC
* Written by Alex Izvorski <aizvorski@gmail.com>
*
* h264lyze - an h264_analyze variant for parsing H.264 video
* Written by Jose A. Lopez <joselopez.engineering@gmail.com>
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "h264_stream.h"

#define H264LYZE_BUFSIZE 32*1024*1024
#define H264LYZE_MIN_ARGC 2

static char h264lyze_options[] =
"\t (null): high level h264 parse\n"
"\t --full: high & low level h264 parse\n"
"\t --type <t>: NALU type t parse\n"
"\t --unit <n>: NALU n parse\n"
"\t --compact: plot compatible output\n"
"\t --stats: high level h264 stats\n";

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
  const char *infile;
  uint8_t option;
  uint8_t nalu_type;
  uint64_t nalu_num
} h264lyze_t;

// h264lyze user option enum
enum h264lyze_user_opt {
  H264LYZE_OPT_HIGH = 0,
  H264LYZE_OPT_FULL,
  H264LYZE_OPT_TYPE,
  H264LYZE_OPT_UNIT,
  H264LYZE_OPT_COMPACT,
  H264LYZE_OPT_STATS
};

// Gets NALU type from h264 object
int h264lyze_nal_unit_type_get(h264_stream_t* h, uint8_t* buf, int size)
{
    nal_t* nal = h->nal;
    int nal_size = size;
    int rbsp_size = size;
    uint8_t* rbsp_buf = (uint8_t*)calloc(1, rbsp_size);
    int rc = nal_to_rbsp(buf, &nal_size, rbsp_buf, &rbsp_size);

    if (rc < 0) {
      free(rbsp_buf); return -1;
    }

    bs_t* b = bs_new(rbsp_buf, rbsp_size);
    bs_read_u(b, 1);
    nal->nal_ref_idc = bs_read_u(b, 2);
    nal->nal_unit_type = bs_read_u(b, 5);
    if (bs_overrun(b)) {
      bs_free(b);
      free(rbsp_buf);
      return -1;
    }

    bs_free(b);
    free(rbsp_buf);
    return nal_size;
}

// Generates h264 output
int h264lyze_process(h264lyze_t *h264lyze)
{
  int status = 0;
  FILE* infile = NULL;
  uint8_t* buf = NULL;
  h264_stream_t* h = NULL;
  int64_t off = 0;
  size_t rsz = 0;
  size_t sz = 0;
  int nal_start = 0;
  int nal_end = 0;
  uint32_t nal_num = 0;

  buf = (uint8_t*)malloc(H264LYZE_BUFSIZE);
  if (buf == NULL) {
    printf("Error: Alloc for buffer failed\n");
    return EXIT_FAILURE;
  }

  h = h264_new();
  if (h == NULL) {
    printf("Error: Alloc for h264 object failed\n");
    return EXIT_FAILURE;
  }

  infile = fopen(h264lyze->infile, "rb");
  if (infile == NULL) {
    printf("Error: File open failed\n");
    return EXIT_FAILURE;
  }
  uint8_t* p = buf;

  while (1) {
      rsz = fread(buf + sz, 1, H264LYZE_BUFSIZE - sz, infile);
      if (rsz == 0) {
          if (ferror(infile)) {
            printf("Error: read failed: %s \n", strerror(errno));
            break;
          }
          break;
      }
      sz += rsz;

      while (find_nal_unit(p, sz, &nal_start, &nal_end) > 0)
      {
          nal_num++;
          p += nal_start;
          h264lyze_nal_unit_type_get (h, p, nal_end - nal_start);

          // Parse h264 bitstream based on user option
          switch(h264lyze->option) {

            case H264LYZE_OPT_FULL: {
              printf("\n%u NAL: addr 0X%04llX size %lld type %u\n",
                     nal_num,
                     (long long int)(off + (p - buf) + nal_start),
                     (long long int)(nal_end - nal_start), h->nal->nal_unit_type);
              read_debug_nal_unit (h, p, nal_end - nal_start);
              break;
            }

            case H264LYZE_OPT_TYPE: {
              if (h->nal->nal_unit_type == h264lyze->nalu_type) {
                printf("\n%u NAL: addr 0X%04llX size %lld TYPE %u\n",
                       nal_num,
                       (long long int)(off + (p - buf) + nal_start),
                       (long long int)(nal_end - nal_start),
                       h->nal->nal_unit_type);
                read_debug_nal_unit (h, p, nal_end - nal_start);
              }
              break;
            }

            case H264LYZE_OPT_UNIT: {
              if (nal_num == h264lyze->nalu_num) {
                printf("\n%u NAL: addr 0X%04llX size %lld type %u\n",
                       nal_num,
                       (long long int)(off + (p - buf) + nal_start),
                       (long long int)(nal_end - nal_start),
                       h->nal->nal_unit_type);
                read_debug_nal_unit (h, p, nal_end - nal_start);
              }
              break;
            }

            case H264LYZE_OPT_COMPACT: {
              printf("%u %lld %lld %u\n",
                     nal_num,
                     (long long int)(off + (p - buf) + nal_start),
                     (long long int)(nal_end - nal_start),
                     h->nal->nal_unit_type);
              break;
            }

            // H264LYZE_OPT_HIGH
            default:
               printf("%u NAL: addr 0X%04llX size %lld type %u\n",
                      nal_num,
                      (long long int)(off + (p - buf) + nal_start),
                      (long long int)(nal_end - nal_start),
                      h->nal->nal_unit_type);
              break;
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
  status = fclose(infile);
  return status;
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
  if (status != 0) {
    printf("Error: File locate %s failed\n", infile);
    h264lyze_usage();
    return EXIT_FAILURE;
  } else {
    // Populate h264 object
    h264lyze->infile = infile;
  }

  // Checking argv2 as option/mode, if option is valid populate h264 object
  if (argc > 2) {
    status = strcmp(argv[2],"--full");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_FULL;
      return status;
    }

    status = strcmp(argv[2],"--type");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_TYPE;
      if (argc > 3) {
        char *p;
        long conv = strtol(argv[3], &p, 10);
        h264lyze->nalu_type = (uint8_t)conv;
      } else {
        printf("Error: Invalid NALU type\n");
        h264lyze_usage();
        return EXIT_FAILURE;
      }
      return status;
    }

    status = strcmp(argv[2],"--unit");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_UNIT;
      if (argc > 3) {
        char *p;
        long conv = strtol(argv[3], &p, 10);
        h264lyze->nalu_num = (uint64_t)conv;
      } else {
        printf("Error: Invalid NALU number)\n");
        h264lyze_usage();
        return EXIT_FAILURE;
      }
      return status;
    }

    status = strcmp(argv[2],"--compact");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_COMPACT;
      return status;
    }

    status = strcmp(argv[2],"--stats");
    if (status == 0) {
      h264lyze->option = H264LYZE_OPT_STATS;
      printf("Option %s currently not supported\n", argv[2]);
      h264lyze_usage();
      return EXIT_FAILURE;
    } else {
      printf("Error: Invalid option %s\n", argv[2]);
      h264lyze_usage();
      return EXIT_FAILURE;
    }
  } else {
    h264lyze->option = H264LYZE_OPT_HIGH;
  }

  return status;
}

int main(int argc, char *argv[])
{
  int status = 0;
  h264lyze_t h264lyze;

  // Populate h264lyze with input
  status = h264lyze_input_check(argc, argv, &h264lyze);
  if (status) return status;

  // Parse h264 input bitstream
  status = h264lyze_process(&h264lyze);
  return status;
}
