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
  char *filename;
  uint8_t option
} h264lyze_t;

// h264lyze user option enum
enum h264lyze_user_opt {
  H264_ANALYZE_TOP = 0,
  H264LYZE_OPT_STATS,
  H264LYZE_OPT_FULL,
  H264LYZE_OPT_TYPE
};

// Checks user argments and populates h264 object accordingly
int h264lyze_input_check(int argc, char *argv[], h264lyze_t *h264lyze)
{
  int status = 0;
  const char *filename = NULL;

  // Check if user supplies minimum arg (h264 file)
  if (argc < H264LYZE_MIN_ARGC) {
    h264lyze_usage();
    return EXIT_FAILURE;
  }

  // Checking argv1 as h264 file
  filename = argv[1];
  status = access(filename, F_OK);
  if ( status != 0 ) {
    printf("Error: Couldn't locate %s file\n", filename);
    h264lyze_usage();
    return EXIT_FAILURE;
  } else {
    // Populate h264 object
    h264lyze->filename = filename;
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
    h264lyze->option = H264_ANALYZE_TOP;
  }

  return status;
}

int main(int argc, char *argv[])
{
  int status = 0;
  h264lyze_t h264lyze;
  status = h264lyze_input_check(argc, argv, &h264lyze);
  if (status) return status;

return 0;
}
