/* 
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2006 Auroras Entertainment, LLC
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

#ifndef _BS_H
#define _BS_H        1

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

void     bs_init(bs_t* b, uint8_t* buf, int size);
uint32_t bs_byte_aligned(bs_t* b);
uint32_t bs_eof(bs_t* b);
int bs_pos(bs_t* b);

uint32_t bs_read_u1(bs_t* b);
uint32_t bs_read_u(bs_t* b, int n);
uint32_t bs_read_f(bs_t* b, int n);
uint32_t bs_read_u8(bs_t* b);
uint32_t bs_read_ue(bs_t* b);
int32_t  bs_read_se(bs_t* b);

void bs_write_u1(bs_t* b, uint32_t v);
void bs_write_u(bs_t* b, int n, uint32_t v);
void bs_write_f(bs_t* b, int n, uint32_t v);
void bs_write_u8(bs_t* b, uint32_t v);
void bs_write_ue(bs_t* b, uint32_t v);
void bs_write_se(bs_t* b, int32_t v);

#ifdef __cplusplus
}
#endif

#endif
