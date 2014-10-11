/*
 * util.c - Utils for ruby-pg
 * $Id$
 *
 */

#include "pg.h"
#include "util.h"

/*
 * These functions assume little endian machine as they're only used on windows.
 *
 */
uint16_t pg_be16toh(uint16_t x)
{
	return ((x & 0x00ff) << 8) | ((x & 0xff00) >> 8);
}

uint32_t pg_be32toh(uint32_t x)
{
	return (x >> 24) |
		((x << 8) & 0x00ff0000L) |
		((x >> 8) & 0x0000ff00L) |
		(x << 24);
}

uint64_t pg_be64toh(uint64_t x)
{
	return (x >> 56) |
		((x << 40) & 0x00ff000000000000LL) |
		((x << 24) & 0x0000ff0000000000LL) |
		((x << 8) & 0x000000ff00000000LL) |
		((x >> 8) & 0x00000000ff000000LL) |
		((x >> 24) & 0x0000000000ff0000LL) |
		((x >> 40) & 0x000000000000ff00LL) |
		(x << 56);
}

static const char base64_encode_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Encode _len_ bytes at _in_ as base64 and write output to _out_.
 *
 * This encoder runs backwards, so that it is possible to encode a string
 * in-place (with _out_ == _in_).
 */
void
base64_encode( char *out, char *in, int len)
{
	char *in_ptr = in + len;
	char *out_ptr = out + BASE64_ENCODED_SIZE(len);
	int part_len = len % 3;

	if( part_len > 0 ){
		long byte2 = part_len > 2 ? *--in_ptr : 0;
		long byte1 = part_len > 1 ? *--in_ptr : 0;
		long byte0 = *--in_ptr;
		long triple = (byte0 << 16) + (byte1 << 8) + byte2;

		*--out_ptr = part_len > 2 ? base64_encode_table[(triple >> 0 * 6) & 0x3F] : '=';
		*--out_ptr = part_len > 1 ? base64_encode_table[(triple >> 1 * 6) & 0x3F] : '=';
		*--out_ptr = base64_encode_table[(triple >> 2 * 6) & 0x3F];
		*--out_ptr = base64_encode_table[(triple >> 3 * 6) & 0x3F];
	}

	while( out_ptr > out ){
		long byte2 = *--in_ptr;
		long byte1 = *--in_ptr;
		long byte0 = *--in_ptr;
		long triple = (byte0 << 16) + (byte1 << 8) + byte2;

		*--out_ptr = base64_encode_table[(triple >> 0 * 6) & 0x3F];
		*--out_ptr = base64_encode_table[(triple >> 1 * 6) & 0x3F];
		*--out_ptr = base64_encode_table[(triple >> 2 * 6) & 0x3F];
		*--out_ptr = base64_encode_table[(triple >> 3 * 6) & 0x3F];
	}
}

/*
 * 0.upto(255).map{|a| "\\x#{ (base64_encode_table.index([a].pack("C")) || 0xff).to_s(16) }" }.join
 */
static const unsigned char base64_decode_table[] =
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x3e\xff\xff\xff\x3f"
	"\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\xff\xff\xff\xff\xff\xff"
	"\xff\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
	"\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\xff\xff\xff\xff\xff"
	"\xff\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28"
	"\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
	"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

/* Decode _len_ bytes of base64 characters at _in_ and write output to _out_.
 *
 * It is possible to decode a string in-place (with _out_ == _in_).
 */
int
base64_decode( char *out, char *in, unsigned int len)
{
	unsigned char a, b, c, d;
	unsigned char *in_ptr = (unsigned char *)in;
	unsigned char *out_ptr = (unsigned char *)out;
	unsigned char *iend_ptr = (unsigned char *)in + len;

	for(;;){
		if( in_ptr+3 < iend_ptr &&
				(a=base64_decode_table[in_ptr[0]]) != 0xff &&
				(b=base64_decode_table[in_ptr[1]]) != 0xff &&
				(c=base64_decode_table[in_ptr[2]]) != 0xff &&
				(d=base64_decode_table[in_ptr[3]]) != 0xff )
		{
			in_ptr += 4;
			*out_ptr++ = (a << 2) | (b >> 4);
			*out_ptr++ = (b << 4) | (c >> 2);
			*out_ptr++ = (c << 6) | d;
		} else if (in_ptr < iend_ptr){
			a = b = c = d = 0xff;
			while ((a = base64_decode_table[*in_ptr++]) == 0xff && in_ptr < iend_ptr) {}
			if (in_ptr < iend_ptr){
				while ((b = base64_decode_table[*in_ptr++]) == 0xff && in_ptr < iend_ptr) {}
				if (in_ptr < iend_ptr){
					while ((c = base64_decode_table[*in_ptr++]) == 0xff && in_ptr < iend_ptr) {}
					if (in_ptr < iend_ptr){
						while ((d = base64_decode_table[*in_ptr++]) == 0xff && in_ptr < iend_ptr) {}
					}
				}
			}
			if (a != 0xff && b != 0xff) {
				*out_ptr++ = (a << 2) | (b >> 4);
				if (c != 0xff) {
					*out_ptr++ = (b << 4) | (c >> 2);
					if (d != 0xff)
						*out_ptr++ = (c << 6) | d;
				}
			}
		} else {
			break;
		}
	}


	return (char*)out_ptr - out;
}
