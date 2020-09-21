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
#include "mbfilter_euc_jp.h"

#include "unicode_table_cp932_ext.h"
#include "unicode_table_jis.h"

void mbfl_filt_ident_eucjp(int c, mbfl_identify_filter *filter);

const unsigned char mblen_table_eucjp[] = { /* 0xA1-0xFE */
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1
};

static const char *mbfl_encoding_euc_jp_aliases[] = {"EUC", "EUC_JP", "eucJP", "x-euc-jp", NULL};

const mbfl_encoding mbfl_encoding_euc_jp = {
	mbfl_no_encoding_euc_jp,
	"EUC-JP",
	"EUC-JP",
	mbfl_encoding_euc_jp_aliases,
	mblen_table_eucjp,
	MBFL_ENCTYPE_MBCS,
	&vtbl_eucjp_wchar,
	&vtbl_wchar_eucjp
};

const struct mbfl_identify_vtbl vtbl_identify_eucjp = {
	mbfl_no_encoding_euc_jp,
	mbfl_filt_ident_common_ctor,
	mbfl_filt_ident_eucjp
};

const struct mbfl_convert_vtbl vtbl_eucjp_wchar = {
	mbfl_no_encoding_euc_jp,
	mbfl_no_encoding_wchar,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_eucjp_wchar,
	mbfl_filt_conv_common_flush
};

const struct mbfl_convert_vtbl vtbl_wchar_eucjp = {
	mbfl_no_encoding_wchar,
	mbfl_no_encoding_euc_jp,
	mbfl_filt_conv_common_ctor,
	NULL,
	mbfl_filt_conv_wchar_eucjp,
	mbfl_filt_conv_common_flush
};

#define CK(statement)	do { if ((statement) < 0) return (-1); } while (0)

/*
 * EUC-JP => wchar
 */
int
mbfl_filt_conv_eucjp_wchar(int c, mbfl_convert_filter *filter)
{
	int c1, s, w;

	switch (filter->status) {
	case 0:
		if (c >= 0 && c < 0x80) {	/* latin */
			CK((*filter->output_function)(c, filter->data));
		} else if (c > 0xa0 && c < 0xff) {	/* X 0208 first char */
			filter->status = 1;
			filter->cache = c;
		} else if (c == 0x8e) {	/* kana first char */
			filter->status = 2;
		} else if (c == 0x8f) {	/* X 0212 first char */
			filter->status = 3;
		} else {
			w = c & MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	case 1:	/* got first half */
		filter->status = 0;
		c1 = filter->cache;
		if (c > 0xa0 && c < 0xff) {
			s = (c1 - 0xa1)*94 + c - 0xa1;
			if (s >= 0 && s < jisx0208_ucs_table_size) {
				w = jisx0208_ucs_table[s];
			} else {
				w = 0;
			}
			if (w <= 0) {
				w = ((c1 & 0x7f) << 8) | (c & 0x7f);
				w &= MBFL_WCSPLANE_MASK;
				w |= MBFL_WCSPLANE_JIS0208;
			}
			CK((*filter->output_function)(w, filter->data));
		} else {
			w = (c1 << 8) | c;
			w &= MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	case 2:	/* got 0x8e */
		filter->status = 0;
		if (c > 0xa0 && c < 0xe0) {
			w = 0xfec0 + c;
			CK((*filter->output_function)(w, filter->data));
		} else {
			w = 0x8e00 | c;
			w &= MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	case 3: /* got 0x8f, JIS X 0212 first byte */
		if (c > 0xa0 && c < 0xff) {
			filter->status++;
			filter->cache = c;
		} else { /* bad */
			filter->status = 0;
			(*filter->output_function)(c | MBFL_WCSGROUP_THROUGH, filter->data);
		}
		break;
	case 4: /* got 0x8f, JIS X 0212 second byte */
		filter->status = 0;
		c1 = filter->cache;
		if (c > 0xa0 && c < 0xff) {
			s = (c1 - 0xa1)*94 + c - 0xa1;
			if (s >= 0 && s < jisx0212_ucs_table_size) {
				w = jisx0212_ucs_table[s];
			} else {
				w = 0;
			}
			if (w <= 0) {
				w = ((c1 & 0x7f) << 8) | (c & 0x7f);
				w &= MBFL_WCSPLANE_MASK;
				w |= MBFL_WCSPLANE_JIS0212;
			}
			CK((*filter->output_function)(w, filter->data));
		} else {
			w = (c1 << 8) | c | 0x8f0000;
			w &= MBFL_WCSGROUP_MASK;
			w |= MBFL_WCSGROUP_THROUGH;
			CK((*filter->output_function)(w, filter->data));
		}
		break;

	default:
		filter->status = 0;
		break;
	}

	return c;
}

/*
 * wchar => EUC-JP
 */
int
mbfl_filt_conv_wchar_eucjp(int c, mbfl_convert_filter *filter)
{
	int c1, s;

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
		c1 = c & ~MBFL_WCSPLANE_MASK;
		if (c1 == MBFL_WCSPLANE_JIS0208) {
			s = c & MBFL_WCSPLANE_MASK;
		} else if (c1 == MBFL_WCSPLANE_JIS0212) {
			s = c & MBFL_WCSPLANE_MASK;
			s |= 0x8080;
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
		if (s < 0x80) {	/* latin */
			CK((*filter->output_function)(s, filter->data));
		} else if (s < 0x100) {	/* kana */
			CK((*filter->output_function)(0x8e, filter->data));
			CK((*filter->output_function)(s, filter->data));
		} else if (s < 0x8080)  {	/* X 0208 */
			CK((*filter->output_function)(((s >> 8) & 0xff) | 0x80, filter->data));
			CK((*filter->output_function)((s & 0xff) | 0x80, filter->data));
		} else {	/* X 0212 */
			CK((*filter->output_function)(0x8f, filter->data));
			CK((*filter->output_function)(((s >> 8) & 0xff) | 0x80, filter->data));
			CK((*filter->output_function)((s & 0xff) | 0x80, filter->data));
		}
	} else {
		CK(mbfl_filt_conv_illegal_output(c, filter));
	}

	return c;
}

/* Not all byte sequences in JIS X 0208 which would otherwise be valid are
 * actually mapped to a character */
static inline int in_unused_jisx0208_range(int c1, int c2)
{
	/* `c1`, `c2` are kuten codes */
	unsigned int s = (c1 - 0x21)*94 + c2 - 0x21;
	return s >= jisx0208_ucs_table_size || !jisx0208_ucs_table[s];
}

static inline int in_unused_jisx0212_range(int c1, int c2)
{
	unsigned int s = (c1 - 0x21)*94 + c2 - 0x21;
	return s >= jisx0212_ucs_table_size || !jisx0212_ucs_table[s];
}

void mbfl_filt_ident_eucjp(int c, mbfl_identify_filter *filter)
{
	unsigned char ku, ten;

	switch (filter->status & 0xF) {
	case 0: /* latin */
		if (c < 0x80) {	/* ok */
			;
		} else if (c > 0xa0 && c < 0xff) {
			/* JIS X 0208, first byte
			 * In EUC-JP, each such byte ranges from 0xA1-0xFE; however,
			 * the bytes of JIS X 0208 kuten codes range from 0x21-0x7E */
			filter->status = ((c - 0xA1 + 0x21) << 8) | 1;
		} else if (c == 0x8e) { /* JIS X 0201 */
			filter->status = 2;
		} else if (c == 0x8f) { /* JIS X 0212 */
			filter->status = 3;
		} else { /* bad */
			filter->flag = 1;
		}
		break;

	case 1: /* 2nd byte of JIS X 0208 */
		ku  = filter->status >> 8;
		ten = c - 0xA1 + 0x21;
		if (c < 0xa1 || c > 0xfe || in_unused_jisx0208_range(ku, ten)) { /* bad */
			filter->flag = 1;
		}
		filter->status = 0;
		break;

	case 2:	/* JIS X 0201 */
		if (c < 0xa1 || c > 0xdf) { /* bad */
			filter->flag = 1;
		}
		filter->status = 0;
		break;

	case 3: /* JIS X 0212 */
		if (c < 0xa1 || c > 0xfe) { /* bad */
			filter->flag = 1;
		} else {
			filter->status = ((c - 0xA1 + 0x21) << 8) | 4;
		}
		break;

	case 4: /* JIS X 0212, final byte */
		ku  = filter->status >> 8;
		ten = c - 0xA1 + 0x21;
		if (c < 0xa1 || c > 0xfe || in_unused_jisx0212_range(ku, ten)) { /* bad */
			filter->flag = 1;
		}
		filter->status = 0;
		break;

	default:
		filter->status = 0;
		break;
	}
}
