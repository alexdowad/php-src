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
 * by moriyoshi koizumi <moriyoshi@php.net> on 4 dec 2002.
 *
 */

#include "mbfilter.h"
#include "mbfilter_utf16.h"

static const char *mbfl_encoding_utf16_aliases[] = {"utf16", NULL};

const mbfl_encoding mbfl_encoding_utf16 = {
	mbfl_no_encoding_utf16,
	"UTF-16",
	"UTF-16",
	mbfl_encoding_utf16_aliases,
	NULL,
	MBFL_ENCTYPE_MWC2BE,
	&vtbl_utf16_wchar,
	&vtbl_wchar_utf16
};

const mbfl_encoding mbfl_encoding_utf16be = {
	mbfl_no_encoding_utf16be,
	"UTF-16BE",
	"UTF-16BE",
	NULL,
	NULL,
	MBFL_ENCTYPE_MWC2BE,
	&vtbl_utf16be_wchar,
	&vtbl_wchar_utf16be
};

const mbfl_encoding mbfl_encoding_utf16le = {
	mbfl_no_encoding_utf16le,
	"UTF-16LE",
	"UTF-16LE",
	NULL,
	NULL,
	MBFL_ENCTYPE_MWC2LE,
	&vtbl_utf16le_wchar,
	&vtbl_wchar_utf16le
};

const struct mbfl_convert_vtbl vtbl_utf16_wchar = {
	mbfl_no_encoding_utf16,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_utf16_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_utf16 = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_utf16,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_utf16be,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_utf16be_wchar = {
	mbfl_no_encoding_utf16be,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_utf16be_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_utf16be = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_utf16be,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_utf16be,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_utf16le_wchar = {
	mbfl_no_encoding_utf16le,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_utf16le_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_utf16le = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_utf16le,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_utf16le,
	mbfl_filt_conv_common_flush
};

#define CK(statement)	do { if ((statement) < 0) return (-1); } while (0)

/*
 * UTF-16 => wchar
 */
int mbfl_filt_conv_utf16_wchar(int c, mbfl_convert_filter *filter)
{
	int n, endian;

	endian = filter->status & 0xff00;
	switch (filter->status & 0x0f) {
	case 0:
		if (endian) {
			n = c & 0xff;
		} else {
			n = (c & 0xff) << 8;
		}
		filter->cache |= n;
		filter->status++;
		break;
	default:
		if (endian) {
			n = (c & 0xff) << 8;
		} else {
			n = c & 0xff;
		}
		n |= filter->cache & 0xffff;
		filter->status &= ~0x0f;
		if (n >= 0xd800 && n < 0xdc00) {
			filter->cache = ((n & 0x3ff) << 16) + 0x400000;
		} else if (n >= 0xdc00 && n < 0xe000) {
			n &= 0x3ff;
			n |= (filter->cache & 0xfff0000) >> 6;
			filter->cache = 0;
			if (n >= MBFL_WCSPLANE_SUPMIN && n < MBFL_WCSPLANE_SUPMAX) {
				CK((*filter->output_function)(n, filter->data));
			} else {		/* illegal character */
				n &= MBFL_WCSGROUP_MASK;
				n |= MBFL_WCSGROUP_THROUGH;
				CK((*filter->output_function)(n, filter->data));
			}
		} else {
			int is_first = filter->status & 0x10;
			filter->cache = 0;
			filter->status |= 0x10;
			if (!is_first) {
				if (n == 0xfffe) {
					if (endian) {
						filter->status &= ~0x100;		/* big-endian */
					} else {
						filter->status |= 0x100;		/* little-endian */
					}
					break;
				} else if (n == 0xfeff) {
					break;
				}
			}
			CK((*filter->output_function)(n, filter->data));
		}
		break;
	}

	return c;
}

/*
 * UTF-16BE => wchar
 */
int mbfl_filt_conv_utf16be_wchar(int c, mbfl_convert_filter *filter)
{
	int n;

	switch (filter->status) {
	case 0:
		filter->status = 1;
		n = (c & 0xff) << 8;
		filter->cache |= n;
		break;
	default:
		filter->status = 0;
		n = (filter->cache & 0xff00) | (c & 0xff);
		if (n >= 0xd800 && n < 0xdc00) {
			filter->cache = ((n & 0x3ff) << 16) + 0x400000;
		} else if (n >= 0xdc00 && n < 0xe000) {
			n &= 0x3ff;
			n |= (filter->cache & 0xfff0000) >> 6;
			filter->cache = 0;
			if (n >= MBFL_WCSPLANE_SUPMIN && n < MBFL_WCSPLANE_SUPMAX) {
				CK((*filter->output_function)(n, filter->data));
			} else {		/* illegal character */
				n &= MBFL_WCSGROUP_MASK;
				n |= MBFL_WCSGROUP_THROUGH;
				CK((*filter->output_function)(n, filter->data));
			}
		} else {
			filter->cache = 0;
			CK((*filter->output_function)(n, filter->data));
		}
		break;
	}

	return c;
}

/*
 * wchar => UTF-16BE
 */
int mbfl_filt_conv_wchar_utf16be(int c, mbfl_convert_filter *filter)
{
	int n;

	if (c >= 0 && c < MBFL_WCSPLANE_UCS2MAX) {
		CK((*filter->output_function)((c >> 8) & 0xff, filter->data));
		CK((*filter->output_function)(c & 0xff, filter->data));
	} else if (c >= MBFL_WCSPLANE_SUPMIN && c < MBFL_WCSPLANE_SUPMAX) {
		n = ((c >> 10) - 0x40) | 0xd800;
		CK((*filter->output_function)((n >> 8) & 0xff, filter->data));
		CK((*filter->output_function)(n & 0xff, filter->data));
		n = (c & 0x3ff) | 0xdc00;
		CK((*filter->output_function)((n >> 8) & 0xff, filter->data));
		CK((*filter->output_function)(n & 0xff, filter->data));
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}

	return c;
}

/*
 * UTF-16LE => wchar
 */
int mbfl_filt_conv_utf16le_wchar_byte2(int c, mbfl_convert_filter *filter);
int mbfl_filt_conv_utf16le_wchar_byte3(int c, mbfl_convert_filter *filter);
int mbfl_filt_conv_utf16le_wchar_byte4(int c, mbfl_convert_filter *filter);

int mbfl_filt_conv_utf16le_wchar(int c, mbfl_convert_filter *filter)
{
	filter->cache = c & 0xff;
	filter->status = 1;
	filter->filter_function = mbfl_filt_conv_utf16le_wchar_byte2;
	return c;
}

int mbfl_filt_conv_utf16le_wchar_byte2(int c, mbfl_convert_filter *filter)
{
	if ((c & 0xfc) == 0xd8) {
		/* Looks like we have a surrogate pair here */
		filter->cache += ((c & 0x3) << 8);
		filter->filter_function = mbfl_filt_conv_utf16le_wchar_byte3;
	} else if ((c & 0xfc) == 0xdc) {
		/* This is wrong; the second part of the surrogate pair has come first
		 *
		 * Imitating legacy behavior of mbfilter for now; but I am not at all convinced
		 * that passing character through is the right thing to do. It might be better
		 * just to return an error status. */
		int n = (filter->cache + ((c & 0xff) << 8)) | MBFL_WCSGROUP_THROUGH;
		filter->filter_function = mbfl_filt_conv_utf16le_wchar;
		filter->status = 0;
		CK((*filter->output_function)(n, filter->data));
	} else {
		filter->filter_function = mbfl_filt_conv_utf16le_wchar;
		filter->status = 0;
		CK((*filter->output_function)(filter->cache + ((c & 0xff) << 8), filter->data));
	}
	return c;
}

int mbfl_filt_conv_utf16le_wchar_byte3(int c, mbfl_convert_filter *filter)
{
	filter->cache = (filter->cache << 10) + (c & 0xff);
	filter->filter_function = mbfl_filt_conv_utf16le_wchar_byte4;
	return c;
}

int mbfl_filt_conv_utf16le_wchar_byte4(int c, mbfl_convert_filter *filter)
{
	filter->filter_function = mbfl_filt_conv_utf16le_wchar;
	filter->status = 0;
	int n = filter->cache + ((c & 0x3) << 8) + 0x10000;
	if (n >= MBFL_WCSPLANE_SUPMIN && n < MBFL_WCSPLANE_SUPMAX) {
		CK((*filter->output_function)(n, filter->data));
	} else { /* illegal character */
		CK((*filter->output_function)((n & MBFL_WCSGROUP_MASK) | MBFL_WCSGROUP_THROUGH, filter->data));
	}
	return c;
}

/*
 * wchar => UTF-16LE
 */
int mbfl_filt_conv_wchar_utf16le(int c, mbfl_convert_filter *filter)
{
	int n;

	if (c >= 0 && c < MBFL_WCSPLANE_UCS2MAX) {
		CK((*filter->output_function)(c & 0xff, filter->data));
		CK((*filter->output_function)((c >> 8) & 0xff, filter->data));
	} else if (c >= MBFL_WCSPLANE_SUPMIN && c < MBFL_WCSPLANE_SUPMAX) {
		n = ((c >> 10) - 0x40) | 0xd800;
		CK((*filter->output_function)(n & 0xff, filter->data));
		CK((*filter->output_function)((n >> 8) & 0xff, filter->data));
		n = (c & 0x3ff) | 0xdc00;
		CK((*filter->output_function)(n & 0xff, filter->data));
		CK((*filter->output_function)((n >> 8) & 0xff, filter->data));
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}

	return c;
}
