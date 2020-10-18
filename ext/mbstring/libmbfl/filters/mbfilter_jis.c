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
 * The source code included in this files was separated from mbfilter_ja.c
 * by moriyoshi koizumi <moriyoshi@php.net> on 4 dec 2002.
 *
 */

#include "mbfilter.h"
#include "mbfilter_jis.h"

#include "unicode_table_cp932_ext.h"
#include "unicode_table_jis.h"

static void mbfl_filt_ident_jis7(unsigned char c, mbfl_identify_filter *filter);
static void mbfl_filt_ident_2022jp(unsigned char c, mbfl_identify_filter *filter);

const mbfl_encoding mbfl_encoding_jis = {
	mbfl_no_encoding_jis,
	"JIS",
	"ISO-2022-JP",
	NULL,
	NULL,
	MBFL_ENCTYPE_GL_UNSAFE,
	&vtbl_jis_wchar,
	&vtbl_wchar_jis
};

const mbfl_encoding mbfl_encoding_2022jp = {
	mbfl_no_encoding_2022jp,
	"ISO-2022-JP",
	"ISO-2022-JP",
	NULL,
	NULL,
	MBFL_ENCTYPE_GL_UNSAFE,
	&vtbl_2022jp_wchar,
	&vtbl_wchar_2022jp
};

const struct mbfl_identify_vtbl vtbl_identify_jis = {
	mbfl_no_encoding_jis,
	mbfl_filt_ident_common_ctor,
	mbfl_filt_ident_jis7
};

const struct mbfl_identify_vtbl vtbl_identify_2022jp = {
	mbfl_no_encoding_2022jp,
	mbfl_filt_ident_common_ctor,
	mbfl_filt_ident_2022jp
};

const struct mbfl_convert_vtbl vtbl_jis_wchar = {
	mbfl_no_encoding_jis,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_jis_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_jis = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_jis,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_jis,
	mbfl_filt_conv_any_jis_flush
};

const struct mbfl_convert_vtbl vtbl_2022jp_wchar = {
	mbfl_no_encoding_2022jp,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_jis_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_2022jp = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_2022jp,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_2022jp,
	mbfl_filt_conv_any_jis_flush
};

/*
 * JIS => wchar
 */
void mbfl_filt_conv_jis_wchar(int c, mbfl_convert_filter *filter)
{
	int c1, s, w;

retry:
	switch (filter->status & 0xf) {
/*	case 0x00:	 ASCII */
/*	case 0x10:	 X 0201 latin */
/*	case 0x20:	 X 0201 kana */
/*	case 0x80:	 X 0208 */
/*	case 0x90:	 X 0212 */
	case 0:
		if (c == 0x1b) {
			filter->status += 2;
		} else if (c == 0x0e) {		/* "kana in" */
			filter->status = 0x20;
		} else if (c == 0x0f) {		/* "kana out" */
			filter->status = 0;
		} else if (filter->status == 0x10 && c == 0x5c) {	/* YEN SIGN */
			(*filter->output_function)(0xa5, filter->data);
		} else if (filter->status == 0x10 && c == 0x7e) {	/* OVER LINE */
			(*filter->output_function)(0x203e, filter->data);
		} else if (filter->status == 0x20 && c > 0x20 && c < 0x60) {		/* kana */
			(*filter->output_function)(0xff40 + c, filter->data);
		} else if ((filter->status == 0x80 || filter->status == 0x90) && c > 0x20 && c < 0x7f) {		/* kanji first char */
			filter->cache = c;
			filter->status += 1;
		} else if (c >= 0 && c < 0x80) {		/* latin, CTLs */
			(*filter->output_function)(c, filter->data);
		} else if (c > 0xa0 && c < 0xe0) {	/* GR kana */
			(*filter->output_function)(0xfec0 + c, filter->data);
		} else {
			w = c & MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			(*filter->output_function)(w, filter->data);
		}
		break;

/*	case 0x81:	 X 0208 second char */
/*	case 0x91:	 X 0212 second char */
	case 1:
		filter->status &= ~0xf;
		c1 = filter->cache;
		if (c > 0x20 && c < 0x7f) {
			s = (c1 - 0x21)*94 + c - 0x21;
			if (filter->status == 0x80) {
				if (s >= 0 && s < jisx0208_ucs_table_size) {
					w = jisx0208_ucs_table[s];
				} else {
					w = 0;
				}
				if (w <= 0) {
					w = (c1 << 8) | c;
					w &= MBFL_WCSPLANE_MASK;
					w |= MBFL_WCSPLANE_JIS0208;
				}
			} else {
				if (s >= 0 && s < jisx0212_ucs_table_size) {
					w = jisx0212_ucs_table[s];
				} else {
					w = 0;
				}
				if (w <= 0) {
					w = (c1 << 8) | c;
					w &= MBFL_WCSPLANE_MASK;
					w |= MBFL_WCSPLANE_JIS0212;
				}
			}
			(*filter->output_function)(w, filter->data);
		} else {
			w = (c1 << 8) | c;
			w &= MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			(*filter->output_function)(w, filter->data);
		}
		break;

	/* ESC */
/*	case 0x02:	*/
/*	case 0x12:	*/
/*	case 0x22:	*/
/*	case 0x82:	*/
/*	case 0x92:	*/
	case 2:
		if (c == 0x24) {		/* '$' */
			filter->status++;
		} else if (c == 0x28) {		/* '(' */
			filter->status += 3;
		} else {
			filter->status &= ~0xf;
			(*filter->output_function)(0x1b, filter->data);
			goto retry;
		}
		break;

	/* ESC $ */
/*	case 0x03:	*/
/*	case 0x13:	*/
/*	case 0x23:	*/
/*	case 0x83:	*/
/*	case 0x93:	*/
	case 3:
		if (c == 0x40 || c == 0x42) {	/* '@' or 'B' */
			filter->status = 0x80;
		} else if (c == 0x28) {			/* '(' */
			filter->status++;
		} else {
			filter->status &= ~0xf;
			(*filter->output_function)(0x1b, filter->data);
			(*filter->output_function)(0x24, filter->data);
			goto retry;
		}
		break;

	/* ESC $ ( */
/*	case 0x04:	*/
/*	case 0x14:	*/
/*	case 0x24:	*/
/*	case 0x84:	*/
/*	case 0x94:	*/
	case 4:
		if (c == 0x40 || c == 0x42) {	/* '@' or 'B' */
			filter->status = 0x80;
		} else if (c == 0x44) {			/* 'D' */
			filter->status = 0x90;
		} else {
			filter->status &= ~0xf;
			(*filter->output_function)(0x1b, filter->data);
			(*filter->output_function)(0x24, filter->data);
			(*filter->output_function)(0x28, filter->data);
			goto retry;
		}
		break;

	/* ESC ( */
/*	case 0x05:	*/
/*	case 0x15:	*/
/*	case 0x25:	*/
/*	case 0x85:	*/
/*	case 0x95:	*/
	case 5:
		if (c == 0x42 || c == 0x48) {		/* 'B' or 'H' */
			filter->status = 0;
		} else if (c == 0x4a) {		/* 'J' */
			filter->status = 0x10;
		} else if (c == 0x49) {		/* 'I' */
			filter->status = 0x20;
		} else {
			filter->status &= ~0xf;
			(*filter->output_function)(0x1b, filter->data);
			(*filter->output_function)(0x28, filter->data);
			goto retry;
		}
		break;

	default:
		filter->status = 0;
		break;
	}
}

/*
 * wchar => JIS
 */
void mbfl_filt_conv_wchar_jis(int c, mbfl_convert_filter *filter)
{
	int s = 0;

	if (c >= ucs_a1_jis_table_min && c < ucs_a1_jis_table_max) {
		s = ucs_a1_jis_table[c - ucs_a1_jis_table_min];
	} else if (c >= ucs_a2_jis_table_min && c < ucs_a2_jis_table_max) {
		s = ucs_a2_jis_table[c - ucs_a2_jis_table_min];
	} else if (c >= ucs_i_jis_table_min && c < ucs_i_jis_table_max) {
		s = ucs_i_jis_table[c - ucs_i_jis_table_min];
	} else if (c >= ucs_r_jis_table_min && c < ucs_r_jis_table_max) {
		s = ucs_r_jis_table[c - ucs_r_jis_table_min];
	}
	if (s <= 0) {
		if (c == 0xa5) {		/* YEN SIGN */
			s = 0x1005c;
		} else if (c == 0x203e) {	/* OVER LINE */
			s = 0x1007e;
		} else if (c == 0xff3c) {	/* FULLWIDTH REVERSE SOLIDUS */
			s = 0x2140;
		} else if (c == 0xff5e) {	/* FULLWIDTH TILDE */
			s = 0x2141;
		} else if (c == 0x2225) {	/* PARALLEL TO */
			s = 0x2142;
		} else if (c == 0xff0d) {	/* FULLWIDTH HYPHEN-MINUS */
			s = 0x215d;
		} else if (c == 0xffe0) {	/* FULLWIDTH CENT SIGN */
			s = 0x2171;
		} else if (c == 0xffe1) {	/* FULLWIDTH POUND SIGN */
			s = 0x2172;
		} else if (c == 0xffe2) {	/* FULLWIDTH NOT SIGN */
			s = 0x224c;
		}
		if (c == 0) {
			s = 0;
		} else if (s <= 0) {
			s = -1;
		}
	}
	if (s >= 0) {
		if (s < 0x80) { /* ASCII */
			if (filter->status != 0) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x42, filter->data);		/* 'B' */
			}
			filter->status = 0;
			(*filter->output_function)(s, filter->data);
		} else if (s < 0x100) { /* kana */
			if (filter->status != 0x100) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x49, filter->data);		/* 'I' */
			}
			filter->status = 0x100;
			(*filter->output_function)(s & 0x7f, filter->data);
		} else if (s < 0x8080) { /* X 0208 */
			if (filter->status != 0x200) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x24, filter->data);		/* '$' */
				(*filter->output_function)(0x42, filter->data);		/* 'B' */
			}
			filter->status = 0x200;
			(*filter->output_function)((s >> 8) & 0x7f, filter->data);
			(*filter->output_function)(s & 0x7f, filter->data);
		} else if (s < 0x10000) { /* X 0212 */
			if (filter->status != 0x300) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x24, filter->data);		/* '$' */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x44, filter->data);		/* 'D' */
			}
			filter->status = 0x300;
			(*filter->output_function)((s >> 8) & 0x7f, filter->data);
			(*filter->output_function)(s & 0x7f, filter->data);
		} else { /* X 0201 latin */
			if (filter->status != 0x400) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x4a, filter->data);		/* 'J' */
			}
			filter->status = 0x400;
			(*filter->output_function)(s & 0x7f, filter->data);
		}
	} else {
		mbfl_filt_conv_illegal_output(c, filter);
	}
}

/*
 * wchar => ISO-2022-JP
 */
void mbfl_filt_conv_wchar_2022jp(int c, mbfl_convert_filter *filter)
{
	int s;

	s = 0;
	if (c >= ucs_a1_jis_table_min && c < ucs_a1_jis_table_max) {
		s = ucs_a1_jis_table[c - ucs_a1_jis_table_min];
	} else if (c >= ucs_a2_jis_table_min && c < ucs_a2_jis_table_max) {
		s = ucs_a2_jis_table[c - ucs_a2_jis_table_min];
	} else if (c >= ucs_i_jis_table_min && c < ucs_i_jis_table_max) {
		s = ucs_i_jis_table[c - ucs_i_jis_table_min];
	} else if (c >= ucs_r_jis_table_min && c < ucs_r_jis_table_max) {
		s = ucs_r_jis_table[c - ucs_r_jis_table_min];
	}
	if (s <= 0) {
		if (c == 0xa5) {			/* YEN SIGN */
			s = 0x1005c;
		} else if (c == 0x203e) {	/* OVER LINE */
			s = 0x1007e;
		} else if (c == 0xff3c) {	/* FULLWIDTH REVERSE SOLIDUS */
			s = 0x2140;
		} else if (c == 0xff5e) {	/* FULLWIDTH TILDE */
			s = 0x2141;
		} else if (c == 0x2225) {	/* PARALLEL TO */
			s = 0x2142;
		} else if (c == 0xff0d) {	/* FULLWIDTH HYPHEN-MINUS */
			s = 0x215d;
		} else if (c == 0xffe0) {	/* FULLWIDTH CENT SIGN */
			s = 0x2171;
		} else if (c == 0xffe1) {	/* FULLWIDTH POUND SIGN */
			s = 0x2172;
		} else if (c == 0xffe2) {	/* FULLWIDTH NOT SIGN */
			s = 0x224c;
		}
		if (c == 0) {
			s = 0;
		} else if (s <= 0) {
			s = -1;
		}
	} else if ((s >= 0x80 && s < 0x2121) || (s > 0x8080)) {
		s = -1;
	}
	if (s >= 0) {
		if (s < 0x80) { /* ASCII */
			if ((filter->status & 0xff00) != 0) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x42, filter->data);		/* 'B' */
			}
			filter->status = 0;
			(*filter->output_function)(s, filter->data);
		} else if (s < 0x10000) { /* X 0208 */
			if ((filter->status & 0xff00) != 0x200) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x24, filter->data);		/* '$' */
				(*filter->output_function)(0x42, filter->data);		/* 'B' */
			}
			filter->status = 0x200;
			(*filter->output_function)((s >> 8) & 0x7f, filter->data);
			(*filter->output_function)(s & 0x7f, filter->data);
		} else { /* X 0201 latin */
			if ((filter->status & 0xff00) != 0x400) {
				(*filter->output_function)(0x1b, filter->data);		/* ESC */
				(*filter->output_function)(0x28, filter->data);		/* '(' */
				(*filter->output_function)(0x4a, filter->data);		/* 'J' */
			}
			filter->status = 0x400;
			(*filter->output_function)(s & 0x7f, filter->data);
		}
	} else {
		mbfl_filt_conv_illegal_output(c, filter);
	}
}

void mbfl_filt_conv_any_jis_flush(mbfl_convert_filter *filter)
{
	/* back to latin */
	if (filter->status & 0xff00) {
		(*filter->output_function)(0x1b, filter->data);		/* ESC */
		(*filter->output_function)(0x28, filter->data);		/* '(' */
		(*filter->output_function)(0x42, filter->data);		/* 'B' */
	}
	filter->status &= 0xff;

	if (filter->flush_function) {
		(*filter->flush_function)(filter->data);
	}
}

static void mbfl_filt_ident_jis7_0208(unsigned char c, mbfl_identify_filter *filter);
static void mbfl_filt_ident_jis7_0212(unsigned char c, mbfl_identify_filter *filter);
static void mbfl_filt_ident_2022jp_0208(unsigned char c, mbfl_identify_filter *filter);

/* ISO 2022-JP has different modes, which can be selected by a sequence
 * starting with ESC (0x1B). In each mode, characters can be selected from a
 * different character set. */
static int handle_basic_esc_sequence(int c, mbfl_identify_filter *filter)
{
	/* If we are on the 2nd byte of a 2-byte character, `filter->status` will
	 * be `first_byte << 8` */

	switch (filter->status) {
	case 0: /* Starting new character */
		if (c == 0x1B) { /* ESC */
			filter->status = 1;
			return 1;
		}
		break;

	case 1: /* Already saw ESC */
		if (c == '$') {
			filter->status = 2;
		} else if (c == '(') {
			filter->status = 3;
		} else {
			filter->flag = 1;
			filter->status = 0;
		}
		return 1;

	case 2: /* Already saw ESC $ */
		if (c == '(') {
			filter->status = 4;
			return 1;
		}
	}

	return 0;
}

static int handle_esc_sequence_jis7(int c, mbfl_identify_filter *filter)
{
	if (handle_basic_esc_sequence(c, filter)) {
		return 1;
	}

	switch (filter->status) {
	case 2: /* Already saw ESC $ */
		if (c == 'B' || c == '@') {
			/* Switch to JIS X 0208 */
			filter->filter_function = mbfl_filt_ident_jis7_0208;
		} else {
			filter->flag = 1;
		}
		filter->status = 0;
		return 1;

	case 3: /* Already saw ESC ( */
		if (c == 'B' || c == 'H' || c == 'J' || c == 'I') {
			/* B => switch to ASCII
			 * J => switch to JIS X 0201 Roman
			 * I => switch to JIS X 0201 Kana
			 * We don't care about the difference, because the valid byte
			 * sequences are the same in any case! */
			filter->filter_function = mbfl_filt_ident_jis7;
		} else {
			filter->flag = 1;
		}
		filter->status = 0;
		return 1;

	case 4: /* Already saw ESC $ ( */
		if (c == 'D') {
			/* Switch to JIS X 0212
			 * This is an extension from ISO-2022-JP-1 (RFC 2237) */
			filter->filter_function = mbfl_filt_ident_jis7_0212;
		} else {
			filter->flag = 1;
		}
		filter->status = 0;
		return 1;
	}

	return 0;
}

static void mbfl_filt_ident_jis7(unsigned char c, mbfl_identify_filter *filter)
{
	/* We convert single bytes from 0xA1-0xDF to JIS X 0201 kana, even if
	 * no escape to shift to JIS X 0201 has been seen */
	if (!handle_esc_sequence_jis7(c, filter) && ((c >= 0x80 && c <= 0xA0) || c >= 0xE0)) {
		filter->flag = 1;
	}
}

/* Not all byte sequences in JIS X 0208 which would otherwise be valid are
 * actually mapped to a character */
static inline int in_unused_jisx0208_range(int c1, int c2)
{
	unsigned int s = (c1 - 0x21)*94 + c2 - 0x21;
	return s >= jisx0208_ucs_table_size || !jisx0208_ucs_table[s];
}

static void handle_jisx_0208(int c, mbfl_identify_filter *filter)
{
	if (filter->status == 0) {
		if (c >= 0x21 && c <= 0x7E) {
			filter->status = c << 8;
		} else if (c > 0x7F) {
			filter->flag = 1;
		}
	} else if (c < 0x21 || c > 0x7E || in_unused_jisx0208_range(filter->status >> 8, c)) {
		filter->flag = 1;
	} else {
		filter->status = 0;
	}
}

static void mbfl_filt_ident_jis7_0208(unsigned char c, mbfl_identify_filter *filter)
{
	if (!handle_esc_sequence_jis7(c, filter)) {
		handle_jisx_0208(c, filter);
	}
}

static inline int in_unused_jisx0212_range(int c1, int c2)
{
	unsigned int s = (c1 - 0x21)*94 + c2 - 0x21;
	return s >= jisx0212_ucs_table_size || !jisx0212_ucs_table[s];
}

static void mbfl_filt_ident_jis7_0212(unsigned char c, mbfl_identify_filter *filter)
{
	if (handle_esc_sequence_jis7(c, filter)) {
		return;
	} else if (filter->status == 0) {
		if (c >= 0x21 && c <= 0x7E) {
			filter->status = c << 8;
		} else if (c > 0x7F) {
			filter->flag = 1;
		}
	} else if (c < 0x21 || c > 0x7E || in_unused_jisx0212_range(filter->status >> 8, c)) {
		filter->flag = 1;
	} else {
		filter->status = 0;
	}
}

static int handle_esc_sequence_2022jp(int c, mbfl_identify_filter *filter)
{
	if (handle_basic_esc_sequence(c, filter)) {
		return 1;
	}

	switch (filter->status) {
	case 2: /* Already saw ESC $ */
		if (c == 'B' || c == '@') {
			/* Switch to JIS X 0208 */
			filter->filter_function = mbfl_filt_ident_2022jp_0208;
		} else {
			filter->flag = 1;
		}
		filter->status = 0;
		return 1;

	case 3: /* Already saw ESC ( */
		if (c == 'B' || c == 'J') {
			/* B => switch to ASCII
			 * J => switch to JIS X 0201 Roman
			 * We don't care about the difference, because the valid byte
			 * sequences are the same in any case! */
			filter->filter_function = mbfl_filt_ident_2022jp;
		} else {
			filter->flag = 1;
		}
		filter->status = 0;
		return 1;

	case 4: /* Already saw ESC $ ( */
		filter->flag = 1;
		filter->status = 0;
		return 1;
	}

	return 0;
}

static void mbfl_filt_ident_2022jp(unsigned char c, mbfl_identify_filter *filter)
{
	if (!handle_esc_sequence_2022jp(c, filter) && c > 0x7F) {
		filter->flag = 1;
	}
}

static void mbfl_filt_ident_2022jp_0208(unsigned char c, mbfl_identify_filter *filter)
{
	if (!handle_esc_sequence_2022jp(c, filter)) {
		handle_jisx_0208(c, filter);
	}
}
