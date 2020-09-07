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
 * by Moriyoshi Koizumi <moriyoshi@php.net> on 20 Dec 2002. The file
 * mbfilter.c is included in this package .
 *
 */

#include <stddef.h>

#include "mbfl_ident.h"
#include "mbfilter_pass.h"
#include "mbfilter_8bit.h"
#include "mbfilter_wchar.h"
#include "../mbstring_singlebyte.h"

#include "filters/mbfilter_euc_cn.h"
#include "filters/mbfilter_hz.h"
#include "filters/mbfilter_euc_tw.h"
#include "filters/mbfilter_big5.h"
#include "filters/mbfilter_uhc.h"
#include "filters/mbfilter_euc_kr.h"
#include "filters/mbfilter_iso2022_kr.h"
#include "filters/mbfilter_sjis.h"
#include "filters/mbfilter_sjis_open.h"
#include "filters/mbfilter_sjis_mobile.h"
#include "filters/mbfilter_sjis_2004.h"
#include "filters/mbfilter_sjis_mac.h"
#include "filters/mbfilter_jis.h"
#include "filters/mbfilter_iso2022_jp_ms.h"
#include "filters/mbfilter_iso2022jp_2004.h"
#include "filters/mbfilter_iso2022jp_mobile.h"
#include "filters/mbfilter_euc_jp.h"
#include "filters/mbfilter_euc_jp_win.h"
#include "filters/mbfilter_euc_jp_2004.h"
#include "filters/mbfilter_utf8_mobile.h"
#include "filters/mbfilter_cp932.h"
#include "filters/mbfilter_cp936.h"
#include "filters/mbfilter_cp51932.h"
#include "filters/mbfilter_cp5022x.h"
#include "filters/mbfilter_gb18030.h"
#include "filters/mbfilter_base64.h"
#include "filters/mbfilter_qprint.h"
#include "filters/mbfilter_uuencode.h"
#include "filters/mbfilter_7bit.h"
#include "filters/mbfilter_utf7.h"
#include "filters/mbfilter_utf7imap.h"
#include "filters/mbfilter_utf8.h"
#include "filters/mbfilter_utf16.h"
#include "filters/mbfilter_utf32.h"
#include "filters/mbfilter_byte2.h"
#include "filters/mbfilter_byte4.h"
#include "filters/mbfilter_ucs4.h"
#include "filters/mbfilter_ucs2.h"
#include "filters/mbfilter_htmlent.h"

static const struct mbfl_identify_vtbl vtbl_identify_false = {
	mbfl_no_encoding_pass,
	mbfl_filt_ident_false_ctor,
	mbfl_filt_ident_false
};

static const struct mbfl_identify_vtbl *mbfl_identify_filter_list[] = {
	&vtbl_identify_utf8,
	&vtbl_identify_utf7,
	&vtbl_identify_ascii,
	&vtbl_identify_eucjp,
	&vtbl_identify_sjis,
	&vtbl_identify_sjis_open,
	&vtbl_identify_eucjpwin,
	&vtbl_identify_eucjp2004,
	&vtbl_identify_cp932,
	&vtbl_identify_jis,
	&vtbl_identify_2022jp,
	&vtbl_identify_2022jpms,
	&vtbl_identify_2022jp_2004,
	&vtbl_identify_2022jp_kddi,
	&vtbl_identify_cp51932,
	&vtbl_identify_sjis_docomo,
	&vtbl_identify_sjis_kddi,
	&vtbl_identify_sjis_sb,
	&vtbl_identify_utf8_docomo,
	&vtbl_identify_utf8_kddi_a,
	&vtbl_identify_utf8_kddi_b,
	&vtbl_identify_utf8_sb,
	&vtbl_identify_euccn,
	&vtbl_identify_cp936,
	&vtbl_identify_hz,
	&vtbl_identify_euctw,
	&vtbl_identify_big5,
	&vtbl_identify_cp950,
	&vtbl_identify_euckr,
	&vtbl_identify_uhc,
	&vtbl_identify_2022kr,
	&vtbl_identify_cp1251,
	&vtbl_identify_cp866,
	&vtbl_identify_koi8r,
	&vtbl_identify_koi8u,
	&vtbl_identify_cp1252,
	&vtbl_identify_cp1254,
	&vtbl_identify_8859_1,
	&vtbl_identify_8859_2,
	&vtbl_identify_8859_3,
	&vtbl_identify_8859_4,
	&vtbl_identify_8859_5,
	&vtbl_identify_8859_6,
	&vtbl_identify_8859_7,
	&vtbl_identify_8859_8,
	&vtbl_identify_8859_9,
	&vtbl_identify_8859_10,
	&vtbl_identify_8859_13,
	&vtbl_identify_8859_14,
	&vtbl_identify_8859_15,
	&vtbl_identify_8859_16,
	&vtbl_identify_armscii8,
	&vtbl_identify_cp850,
	&vtbl_identify_jis_ms,
	&vtbl_identify_cp50220,
	&vtbl_identify_cp50221,
	&vtbl_identify_cp50222,
	&vtbl_identify_gb18030,
	&vtbl_identify_7bit,
	&vtbl_identify_utf16,
	&vtbl_identify_utf16le,
	&vtbl_identify_utf16be,
	&vtbl_identify_8bit,
	&vtbl_identify_ucs2,
	&vtbl_identify_ucs2be,
	&vtbl_identify_ucs2le,
	&vtbl_identify_utf32,
	&vtbl_identify_utf32be,
	&vtbl_identify_utf32le,
	&vtbl_identify_sjis2004,
	&vtbl_identify_sjis_mac,
	&vtbl_identify_ucs4,
	&vtbl_identify_ucs4be,
	&vtbl_identify_ucs4le,
	&vtbl_identify_utf7imap,
	&vtbl_identify_false,
	NULL
};

const struct mbfl_identify_vtbl* mbfl_identify_filter_get_vtbl(enum mbfl_no_encoding encoding)
{
	const struct mbfl_identify_vtbl** vtbl;

	for (vtbl = mbfl_identify_filter_list; *vtbl; vtbl++) {
		if ((*vtbl)->encoding == encoding) {
			return *vtbl;
		}
	}

	return NULL;
}

mbfl_identify_filter *mbfl_identify_filter_new(enum mbfl_no_encoding encoding)
{
	mbfl_identify_filter *filter = emalloc(sizeof(mbfl_identify_filter));
	mbfl_identify_filter_init(filter, encoding);
	return filter;
}

mbfl_identify_filter *mbfl_identify_filter_new2(const mbfl_encoding *encoding)
{
	mbfl_identify_filter *filter = emalloc(sizeof(mbfl_identify_filter));
	mbfl_identify_filter_init2(filter, encoding);
	return filter;
}

void mbfl_identify_filter_init(mbfl_identify_filter *filter, enum mbfl_no_encoding encoding)
{
	const mbfl_encoding *enc = mbfl_no2encoding(encoding);
	mbfl_identify_filter_init2(filter, enc ? enc : &mbfl_encoding_pass);
}

void mbfl_identify_filter_init2(mbfl_identify_filter *filter, const mbfl_encoding *encoding)
{
	filter->encoding = encoding;
	filter->status = filter->flag = filter->score = 0;

	/* setup the function table */
	const struct mbfl_identify_vtbl *vtbl = mbfl_identify_filter_get_vtbl(filter->encoding->no_encoding);
	if (vtbl == NULL) {
		vtbl = &vtbl_identify_false;
	}
	filter->filter_ctor = vtbl->filter_ctor;
	filter->filter_function = vtbl->filter_function;

	(*filter->filter_ctor)(filter);
}

void mbfl_identify_filter_delete(mbfl_identify_filter *filter)
{
	efree(filter);
}

void mbfl_filt_ident_common_ctor(mbfl_identify_filter *filter)
{
	filter->status = filter->flag = 0;
}

/* A (useless) filter which says that _every_ string is invalid in a certain encoding.
 * Obviously, that cannot be true. Remove after all encodings have proper identify filters */
int mbfl_filt_ident_false(int c, mbfl_identify_filter *filter)
{
	filter->flag = 1;	/* bad */
	return c;
}

void mbfl_filt_ident_false_ctor(mbfl_identify_filter *filter)
{
	filter->status = 0;
	filter->flag = 1;
}

/* For encodings in which _every_ possible input string is valid */
int mbfl_filt_ident_true(int c, mbfl_identify_filter *filter)
{
	return c;
}
