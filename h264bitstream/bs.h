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

#include <stdint.h>

#ifndef _H264_BS_H
#define _H264_BS_H        1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint8_t* start;
	uint8_t* p;
	uint8_t* end;
	int bits_left;
} bs_t;

bs_t* bs_new(uint8_t* buf, int size);
void bs_free(bs_t* b);
void bs_init(bs_t* b, uint8_t* buf, int size);
static inline uint32_t bs_byte_aligned(bs_t* b);
static inline uint32_t bs_eof(bs_t* b);
static inline int bs_pos(bs_t* b);

static inline uint32_t bs_read_u1(bs_t* b);
static inline uint32_t bs_read_u(bs_t* b, int n);
uint32_t bs_read_f(bs_t* b, int n);
uint32_t bs_read_u8(bs_t* b);
uint32_t bs_read_ue(bs_t* b);
int32_t  bs_read_se(bs_t* b);

static inline void bs_write_u1(bs_t* b, uint32_t v);
static inline void bs_write_u(bs_t* b, int n, uint32_t v);
void bs_write_f(bs_t* b, int n, uint32_t v);
void bs_write_u8(bs_t* b, uint32_t v);
void bs_write_ue(bs_t* b, uint32_t v);
void bs_write_se(bs_t* b, int32_t v);

static inline uint32_t bs_byte_aligned(bs_t* b) { if (b->bits_left == 8) { return 1; } else { return 0; } }

static inline uint32_t bs_eof(bs_t* b) { if (b->p >= b->end) { return 1; } else { return 0; } }

static inline int bs_pos(bs_t* b) { return (b->p - b->start); }

static inline uint32_t bs_read_u1(bs_t* b)
{
    uint32_t r = 0;
    if (bs_eof(b)) { return 0; }
    
    b->bits_left--;
    r = ((*(b->p)) >> b->bits_left) & 0x01;

    if (b->bits_left == 0) { b->p ++; b->bits_left = 8; }

    return r;
}

static inline uint32_t bs_read_u(bs_t* b, int n)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        r |= ( bs_read_u1(b) << ( n - i - 1 ) );
    }
    return r;
}

static inline void bs_write_u1(bs_t* b, uint32_t v)
{
    if (bs_eof(b)) { return; }
    
    b->bits_left--;
    (*(b->p)) &= ~(0x01 << b->bits_left);
    (*(b->p)) |= ((v & 0x01) << b->bits_left);

    if (b->bits_left == 0) { b->p ++; b->bits_left = 8; }
}

static inline void bs_write_u(bs_t* b, int n, uint32_t v)
{
    int i;
    for (i = 0; i < n; i++)
    {
        bs_write_u1(b, (v >> ( n - i - 1 ))&0x01 );
    }
}



#ifdef __cplusplus
}
#endif

#endif
