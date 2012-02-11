#ifndef _BS_H
#define _BS_H        1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	uint8_t* start;
	uint8_t* p;
	uint8_t* end;
	int bits_left;
} bit_shifter_t;

#define _OPTIMIZE_BS_ 1

#if ( _OPTIMIZE_BS_ > 0 )
#ifndef FAST_U8
#define FAST_U8
#endif
#endif


static bit_shifter_t* bits_new(uint8_t* buf, size_t size);
static void bits_free(bit_shifter_t* b);
static bit_shifter_t* bits_clone( bit_shifter_t* dest, const bit_shifter_t* src );
static bit_shifter_t*  bits_init(bit_shifter_t* b, uint8_t* buf, size_t size);
static uint32_t bits_byte_aligned(bit_shifter_t* b);
static uint32_t bits_eof(bit_shifter_t* b);
static int bits_pos(bit_shifter_t* b);

static uint32_t bits_peek_u1(bit_shifter_t* b);
static uint32_t bits_read_u1(bit_shifter_t* b);
static uint32_t bits_read_u(bit_shifter_t* b, int n);
static uint32_t bits_read_f(bit_shifter_t* b, int n);
static uint32_t bits_read_u8(bit_shifter_t* b);
static uint32_t bits_read_ue(bit_shifter_t* b);
static int32_t  bits_read_se(bit_shifter_t* b);

static void bits_write_u1(bit_shifter_t* b, uint32_t v);
static void bits_write_u(bit_shifter_t* b, int n, uint32_t v);
static void bits_write_f(bit_shifter_t* b, int n, uint32_t v);
static void bits_write_u8(bit_shifter_t* b, uint32_t v);
static void bits_write_ue(bit_shifter_t* b, uint32_t v);
static void bits_write_se(bit_shifter_t* b, int32_t v);

static int bits_read_bytes(bit_shifter_t* b, uint8_t* buf, int len);
static int bits_write_bytes(bit_shifter_t* b, uint8_t* buf, int len);
static int bits_skip_bytes(bit_shifter_t* b, int len);
static uint32_t bits_next_bits( bit_shifter_t* bs, int nbits );
// IMPLEMENTATION

static inline bit_shifter_t* bits_init(bit_shifter_t* b, uint8_t* buf, size_t size)
{
    b->start = buf;
    b->p = buf;
    b->end = buf + size;
    b->bits_left = 8;
    return b;
}

static inline bit_shifter_t* bits_clone( bit_shifter_t* dest, const bit_shifter_t* src )
{
    dest->start = src->p;
    dest->p = src->p;
    dest->end = src->end;
    dest->bits_left = src->bits_left;
    return dest;
}

static inline uint32_t bits_byte_aligned(bit_shifter_t* b) 
{ 
    return (b->bits_left == 8);
}

static inline bit_shifter_t* bits_new(uint8_t* buf, size_t size)
{
    bit_shifter_t* b = (bit_shifter_t*)malloc(sizeof(bit_shifter_t));
    bits_init(b, buf, size);
    return b;
}

static inline void bits_free(bit_shifter_t* b)
{
    free(b);
}

static inline uint32_t bits_eof(bit_shifter_t* b) { if (b->p >= b->end) { return 1; } else { return 0; } }

static inline int bits_pos(bit_shifter_t* b) { return (b->p - b->start); }

static inline int bits_bytes_left(bit_shifter_t* b) { return (b->end - b->p); }

static inline uint32_t bits_read_u1(bit_shifter_t* b)
{
    uint32_t r = 0;
    if (bits_eof(b)) { return 0; }
    
    b->bits_left--;
    r = ((*(b->p)) >> b->bits_left) & 0x01;

    if (b->bits_left == 0) { b->p ++; b->bits_left = 8; }

    return r;
}

static inline void bits_skip_u1(bit_shifter_t* b)
{    
    b->bits_left--;
    if (b->bits_left == 0) { b->p ++; b->bits_left = 8; }
}

static inline uint32_t bits_peek_u1(bit_shifter_t* b)
{
    uint32_t r = 0;
    if (bits_eof(b)) { return 0; }
    
    r = ((*(b->p)) >> ( b->bits_left - 1 )) & 0x01;

    return r;
}


static inline uint32_t bits_read_u(bit_shifter_t* b, int n)
{
    uint32_t r = 0;
    int i;
    for (i = 0; i < n; i++)
    {
        r |= ( bits_read_u1(b) << ( n - i - 1 ) );
    }
    return r;
}

static inline void bits_skip_u(bit_shifter_t* b, int n)
{
    int i;
    for ( i = 0; i < n; i++ ) bits_skip_u1( b );
}

static inline uint32_t bits_read_f(bit_shifter_t* b, int n) { return bits_read_u(b, n); }

static inline uint32_t bits_read_u8(bit_shifter_t* b)
{
#ifdef FAST_U8
    if (b->bits_left == 8 && ! bits_eof(b)) // can do fast read
    {
        uint32_t r = b->p[0];
        b->p++;
        return r;
    }
#endif
    return bits_read_u(b, 8);
}

static inline uint32_t bits_read_ue(bit_shifter_t* b)
{
    int32_t r = 0;
    int i = 0;

    while( (bits_read_u1(b) == 0) && (i < 32) && (!bits_eof(b)) )
    {
        i++;
    }
    r = bits_read_u(b, i);
    r += (1 << i) - 1;
    return r;
}

static inline int32_t bits_read_se(bit_shifter_t* b) 
{
    int32_t r = bits_read_ue(b);
    if (r & 0x01)
    {
        r = (r+1)/2;
    }
    else
    {
        r = -(r/2);
    }
    return r;
}


static inline void bits_write_u1(bit_shifter_t* b, uint32_t v)
{
    if (bits_eof(b)) { return; }
    
    b->bits_left--;
    (*(b->p)) &= ~(0x01 << b->bits_left); // FIXME this is slow, but we must clear bit first - is it better to memset(0) the whole buffer during bits_init() instead?  if we don't do either, we introduce pretty nasty bugs
    (*(b->p)) |= ((v & 0x01) << b->bits_left);

    if (b->bits_left == 0) { b->p ++; b->bits_left = 8; }
}

static inline void bits_write_u(bit_shifter_t* b, int n, uint32_t v)
{
    int i;
    for (i = 0; i < n; i++)
    {
        bits_write_u1(b, (v >> ( n - i - 1 ))&0x01 );
    }
}

static inline void bits_write_f(bit_shifter_t* b, int n, uint32_t v) { bits_write_u(b, n, v); }

static inline void bits_write_u8(bit_shifter_t* b, uint32_t v)
{
#ifdef FAST_U8
    if (b->bits_left == 8 && ! bits_eof(b)) // can do fast write
    {
        b->p[0] = v;
        b->p++;
        return;
    }
#endif
    bits_write_u(b, 8, v);
}

static inline void bits_write_ue(bit_shifter_t* b, uint32_t v)
{
    static const int len_table[256] =
    {
        1,
        1,
        2,2,
        3,3,3,3,
        4,4,4,4,4,4,4,4,
        5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
        8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    };

    int len;

    if (v == 0)
    {
        bits_write_u1(b, 1);
    }
    else
    {
        v++;

        if (v >= 0x01000000)
        {
            len = 24 + len_table[ v >> 24 ];
        }
        else if(v >= 0x00010000)
        {
            len = 16 + len_table[ v >> 16 ];
        }
        else if(v >= 0x00000100)
        {
            len =  8 + len_table[ v >>  8 ];
        }
        else 
        {
            len = len_table[ v ];
        }

        bits_write_u(b, 2*len-1, v);
    }
}

static inline void bits_write_se(bit_shifter_t* b, int32_t v)
{
    if (v <= 0)
    {
        bits_write_ue(b, -v*2);
    }
    else
    {
        bits_write_ue(b, v*2 - 1);
    }
}

static inline int bits_read_bytes(bit_shifter_t* b, uint8_t* buf, int len)
{
    int actual_len;
    actual_len = len;
    if (b->end - b->p < actual_len) { actual_len = b->end - b->p; }
    if (actual_len < 0) { actual_len = 0; }
    memcpy(buf, b->p, actual_len);
    b->p += actual_len;
    return actual_len;
}

static inline int bits_write_bytes(bit_shifter_t* b, uint8_t* buf, int len)
{
    int actual_len;
    actual_len = len;
    if (b->end - b->p < actual_len) { actual_len = b->end - b->p; }
    if (actual_len < 0) { actual_len = 0; }
    memcpy(b->p, buf, actual_len);
    b->p += actual_len;
    return actual_len;
}

static inline int bits_skip_bytes(bit_shifter_t* b, int len)
{
    int actual_len;
    actual_len = len;
    if (b->end - b->p < actual_len) { actual_len = b->end - b->p; }
    if (actual_len < 0) { actual_len = 0; }
    b->p += actual_len;
    return actual_len;
}

static inline uint32_t bits_next_bits( bit_shifter_t* bs, int nbits )
{
   bit_shifter_t b;
   bits_clone(&b,bs);
   return bits_read_u( &b, nbits );
}

static inline uint64_t bits_next_bytes( const bit_shifter_t* const bs, int nbytes )
{
   int i = 0;
   uint64_t val = 0;

   if ( (nbytes > 8) || (nbytes < 1) ) return 0;
   if (bs->p + nbytes > bs->end) return 0;

   for ( i = 0; i < nbytes; i++ ) val = ( val << 8 ) | bs->p[i];  
   return val;
}

#define bits_print_state(b) fprintf( stderr,  "%s:%d@%s: b->p=0x%02hhX, b->left = %d\n", __FILE__, __LINE__, __FUNCTION__, *b->p, b->bits_left )

#ifdef __cplusplus
}
#endif

#endif
