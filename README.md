A library to read and write H.264 video bitstreams, in particular to examine or modify headers.

[![CircleCI](https://circleci.com/gh/aizvorski/h264bitstream.svg?style=shield)](https://circleci.com/gh/aizvorski/h264bitstream)

## Compile and Install

1. Install pre-requisites (Debian, Ubuntu)
  
    `sudo apt-get install build-essential libtool autoconf ffmpeg`

1. Auto-reconfigure the project

    `autoreconf -i`

1. Run the generated configure script and build the project

    ```sh
    ./configure --prefix=/usr/local
    make
    ```

1. Optionally, install the binaries in `/usr/local/`

    `make install`

This will produce `/usr/local/bin/h264_analyze` and `/usr/local/lib/libh264bitstream`.

## Example Code

Read one data unit (NAL, or network abstraction layer unit) out of an H264 bitstream and print it out:

```
int nal_start, nal_end;
uint8_t* buf;
int len;
// read some H264 data into buf
h264_stream_t* h = h264_new();
find_nal_unit(buf, len, &nal_start, &nal_end);
read_nal_unit(h, &buf[nal_start], nal_end - nal_start);
debug_nal(h,h->nal);
```

## Goals

The main design goal is provide a complete, fully standards-compliant open-source library for reading and writing H264 streams.

Reading and writing headers (sequence and picture parameter sets, slice headers, etc) is a higher priority goal than reading and writing encoded picture data, and has been implemented first.

Secondary goals are portability and a simple, clean API.  The library uses a subset of the language features of the C99 standard, without any compiler-specific extensions or dependence on a particular operating system or hardware.

_This library is not an encoder or decoder_, although you could use it to make writing an encoder or decoder much easier.  It understands the _syntax_ of H264, but not the semantics (meaning) of the data it reads or writes.


## Implementation Status

Everything in the H264 standard is implemented except for: 
- parsing of diffeent SEI messages
- SPS extension
- slice data
- slice data partitioning

Most of the unimplemented data will be correctly skipped when reading and ignored (not written) while writing; the code to read/write it is present as a stub, but they require somewhat more complex data structures to store the data, and those are not implemented yet.

Reading and writing slice data is complex and may not be fully implemented soon (target: 1.0.0 version), although there is some work-in-progress code for it.

Slice data partitioning will not be implemented in the forseeable future.  It would require extensive changes to the library, and as it is only available in Extended Profile, its utility is limited.


## Documentation

The library follows the H264 standard verbatim in most cases, including the naming of most variables and some functions.

A copy of the standard can be obtained from ITU (at http://www.itu.int/rec/T-REC-H.264/e ).

The public API documentation can be generated using Doxygen (run "make dox", the results are in the dox/ subdirectory).

The public API consists of:

```
    h264_new
    h264_free
    find_nal_unit
    read_nal_unit
    write_nal_unit
    rbsp_to_nal
    nal_to_rbsp
    debug_nal
```

plus direct access to the fields of h264_stream_t and the data structures nested in that.

Using other functions contained in the library to directly read or write specific types of NALs or parts thereof is not part of the public API, although it is not hard to do if you prepare the required bs_t argument.  Please also note that using rbsp functions requires you also to perform handle RBSP to NAL (and vice versa) translation by calling rbsp_to_nal and nal_to_rbsp.


## Programming Notes

All data types are stored in simple int fields in various structures, sometimes nested structures (which are guaranteed to be non-null if the containing structure is non-null, and may therefore be safely dereferenced).  These fields can be used and assigned to directly; due to the very large number of fields, there are no accessor functions.  Boolean values are 0 for false and 1 for true.  Unsigned and signed integers can be assigned and read directly.

The currently active picture parameter set, sequence parameter set, slice header and nal are stored as fields in the h264_stream_t structure h which represents the stream being read.

For example, to write a simple SPS, use code like this:

```
    h264_stream_t* h = h264_new();

    h->nal->nal_ref_idc = 0x03;
    h->nal->nal_unit_type = NAL_UNIT_TYPE_SPS;

    h->sps->profile_idc = 0x42;
    h->sps->level_idc = 0x33;
    h->sps->log2_max_frame_num_minus4 = 0x05;
    h->sps->log2_max_pic_order_cnt_lsb_minus4 = 0x06;
    h->sps->num_ref_frames = 0x01;
    h->sps->pic_width_in_mbs_minus1 = 0x2c;
    h->sps->pic_height_in_map_units_minus1 = 0x1d;
    h->sps->frame_mbs_only_flag = 0x01;
    
    int len = write_nal_unit(h, buf, size);
```

All operations rely on an underlying set of bitstream functions which operate on `bs_t*` structures.  Those are inherently buffer-overflow-safe and endiannes-independent, but provide only limited error handling at this time.  Reads beyond the end of a buffer succeed and return an infinite sequence of zero bits; writes beyong the end of a buffer succeed and are ignored.  To be sure that the buffer passed was large enough, check that the return of the read_nal_unit or write_nal_unit is _less_ than the size of the buffer you passed in; if it is equal it is possible you're missing the end of the data.

You should always call find_nal_unit before calling read_nal_unit as shown in the quick start example.   Successive reads without a find may fail, either due to bugs in the handling of the various types of rbsp padding, or because the stream is not compliant.


## Limitations

The library does not try to maintain a table of different SPS and PPS, nor does it check that pic_parameter_set_id and seq_parameter_set_id in the current nal match those of the last read SPS/PPS.  It stores the most recently seen SPS and PPS, and assumes those are applicable.  If you are likely to encounter streams with multiple PPS or SPS, as a workaround you can store copies of each SPS/PPS which is read, check the corresponding id in other elements after reading, and if they do not match substitute the correct parameter set and re-read.  This limitation will be removed in a later version.


## Known Bugs

* if attempting to write SLICE_TYPE_*_ALT slices, resulting slice header could be incorrect in versions <= 0.1.6

* if no nal was found in entire processing buffer, h264_analyze could exit prematurely in versions <= 0.1.6

* writing complete nals did not work correctly in versions <= 0.1.5

* rbsp_trailing_bits could be (very rarely) handled incorrectly in versions <= 0.1.5

* attempting to write decoded reference picture marking or reference picture list reordering data results in unpredictable behaviour


## Copyright

Copyright (C) 2013-2017 Alex Izvorski and contributors
Copyright (C) 2008-2012 Alex Izvorski and Alex Giladi
Copyright (C) 2005-2007 Auroras Entertainment, LLC

Written by Alex Izvorski <aizvorski@gmail.com>

This library is distributed under the GNU Lesser General Public License.

Please see the file LICENSE for more details.

Homepage: https://github.com/aizvorski/h264bitstream/
