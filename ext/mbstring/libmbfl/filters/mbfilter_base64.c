 /*
 * "streamable kanji code filter and converter"
 * Copyright (c) 1998-2002 HappySize, Inc. All rights reserved.
 *
 * LICENSE NOTICES
 *
 * This file is part of "streamable kanji code filter and converter",
 * which is distributed under the terms of GNU Lesser General Public
 * License (version 2) as published by the Free Software Foundation.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with "streamable kanji code filter and converter";
 * if not, write to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA  02111-1307  USA
 *
 * The author of this file:
 *
 */
/*
 * The source code included in this files was separated from mbfilter.c
 * by Moriyoshi Koizumi <moriyoshi@php.net> on 4 Dec 2002. The file
 * mbfilter.c is included in this package .
 *
 */

#include "mbfilter.h"
#include "mbfilter_base64.h"

static void mbfl_filt_ident_base64(unsigned char c, mbfl_identify_filter *filter);

const mbfl_encoding mbfl_encoding_base64 = {
	mbfl_no_encoding_base64,
	"BASE64",
	"BASE64",
	NULL,
	NULL,
	MBFL_ENCTYPE_GL_UNSAFE,
	NULL,
	NULL
};

const struct mbfl_identify_vtbl vtbl_identify_base64 = {
	mbfl_no_encoding_base64,
	mbfl_filt_ident_common_ctor,
	mbfl_filt_ident_base64
};

const struct mbfl_convert_vtbl vtbl_8bit_b64 = {
	mbfl_no_encoding_8bit,
	mbfl_no_encoding_base64,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_base64enc,
	mbfl_filt_conv_base64enc_flush
};

const struct mbfl_convert_vtbl vtbl_b64_8bit = {
	mbfl_no_encoding_base64,
	mbfl_no_encoding_8bit,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_base64dec,
	mbfl_filt_conv_base64dec_flush
};

/*
 * any => BASE64
 */
static const unsigned char mbfl_base64_table[] = {
 /* 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', */
   0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,
 /* 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', */
   0x4e,0x4f,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,
 /* 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', */
   0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,
 /* 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', */
   0x6e,0x6f,0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,
 /* '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/', '\0' */
   0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x2b,0x2f,0x00
};

void mbfl_filt_conv_base64enc(int c, mbfl_convert_filter *filter)
{
	int n = (filter->status & 0xff);
	/* 1st (low) byte of `n` is the number of bytes already cached (0, 1, or 2)
	 *
	 * 2nd byte of `n` is the number of characters already emitted on the current
	 * line, _if_ this filter is not being used for MIME header encoding
	 *
	 * 4th byte of `n` is 1 if this filter is being used for MIME header encoding */
	if (n == 0) {
		filter->status++;
		filter->cache = (c & 0xff) << 16;
	} else if (n == 1) {
		filter->status++;
		filter->cache |= (c & 0xff) << 8;
	} else {
		filter->status &= ~0xff;
		if ((filter->status & MBFL_BASE64_STS_MIME_HEADER) == 0) {
			if ((filter->status & 0xff00) > (72 << 8)) {
				(*filter->output_function)(0x0d, filter->data);		/* CR */
				(*filter->output_function)(0x0a, filter->data);		/* LF */
				filter->status = 0;
			}
			filter->status += 0x400;
		}
		n = filter->cache | (c & 0xff);
		(*filter->output_function)(mbfl_base64_table[(n >> 18) & 0x3f], filter->data);
		(*filter->output_function)(mbfl_base64_table[(n >> 12) & 0x3f], filter->data);
		(*filter->output_function)(mbfl_base64_table[(n >> 6) & 0x3f], filter->data);
		(*filter->output_function)(mbfl_base64_table[n & 0x3f], filter->data);
	}
}

void mbfl_filt_conv_base64enc_flush(mbfl_convert_filter *filter)
{
	int status = filter->status & 0xff;
	int cache = filter->cache;
	int len = (filter->status & 0xff00) >> 8;
	filter->status &= ~0xffff;
	filter->cache = 0;
	/* flush fragments */
	if (status >= 1) {
		if ((filter->status & MBFL_BASE64_STS_MIME_HEADER) == 0) {
			if (len > 72){
				(*filter->output_function)('\r', filter->data);
				(*filter->output_function)('\n', filter->data);
			}
		}
		(*filter->output_function)(mbfl_base64_table[(cache >> 18) & 0x3f], filter->data);
		(*filter->output_function)(mbfl_base64_table[(cache >> 12) & 0x3f], filter->data);
		if (status == 1) {
			(*filter->output_function)('=', filter->data);
		} else {
			(*filter->output_function)(mbfl_base64_table[(cache >> 6) & 0x3f], filter->data);
		}
		(*filter->output_function)('=', filter->data);
	}
}

/*
 * BASE64 => any
 */
static unsigned int decode_base64_char(unsigned char c)
{
	if (c >= 'A' && c <= 'Z') {
		return c - 65;
	} else if (c >= 'a' && c <= 'z') {
		return c - 71;
	} else if (c >= '0' && c <= '9') {
		return c + 4;
	} else if (c == '+') {
		return 62;
	} else if (c == '/') {
		return 63;
	}
	return -1;
}

void mbfl_filt_conv_base64dec(int c, mbfl_convert_filter *filter)
{
	if (c == '\r' || c == '\n' || c == ' ' || c == '\t' || c == '=') {
		return;
	}

	unsigned int n = decode_base64_char(c);

	switch (filter->status) {
	case 0:
		filter->status = 1;
		filter->cache = n << 18;
		break;
	case 1:
		filter->status = 2;
		filter->cache |= n << 12;
		break;
	case 2:
		filter->status = 3;
		filter->cache |= n << 6;
		break;
	default:
		filter->status = 0;
		n |= filter->cache;
		(*filter->output_function)((n >> 16) & 0xff, filter->data);
		(*filter->output_function)((n >> 8) & 0xff, filter->data);
		(*filter->output_function)(n & 0xff, filter->data);
		break;
	}
}

void mbfl_filt_conv_base64dec_flush(mbfl_convert_filter *filter)
{
	int status = filter->status;
	int cache = filter->cache;
	filter->status = filter->cache = 0;
	/* flush fragments */
	if (status >= 2) {
		(*filter->output_function)((cache >> 16) & 0xff, filter->data);
		if (status >= 3) {
			(*filter->output_function)((cache >> 8) & 0xff, filter->data);
		}
	}
}

static void mbfl_filt_ident_base64(unsigned char c, mbfl_identify_filter *filter)
{
	if (decode_base64_char(c) != -1 || c == '=') {
		filter->status = (filter->status + 1) % 4;
	} else {
		filter->flag = 1; /* Illegal character */
	}
}
