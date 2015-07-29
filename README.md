h264bitstream
=============

A complete set of functions to read and write H.264 video bitstreams, in particular to examine or modify headers.

## Compiling and installing

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
