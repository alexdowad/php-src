/*
 * charset=UTF-8
 */

/*
 * "streamable kanji code filter and converter"
 *
 * Copyright (c) 1998,1999,2000,2001 HappySize, Inc. All rights reserved.
 *
 * This software is released under the GNU Lesser General Public License.
 * (Version 2.1, February 1999)
 * Please read the following detail of the licence (in japanese).
 *
 * ◆使用許諾条件◆
 *
 * このソフトウェアは株式会社ハッピーサイズによって開発されました。株式会社ハッ
 * ピーサイズは、著作権法および万国著作権条約の定めにより、このソフトウェアに関
 * するすべての権利を留保する権利を持ち、ここに行使します。株式会社ハッピーサイ
 * ズは以下に明記した条件に従って、このソフトウェアを使用する排他的ではない権利
 * をお客様に許諾します。何人たりとも、以下の条件に反してこのソフトウェアを使用
 * することはできません。
 *
 * このソフトウェアを「GNU Lesser General Public License (Version 2.1, February
 * 1999)」に示された条件で使用することを、全ての方に許諾します。「GNU Lesser
 * General Public License」を満たさない使用には、株式会社ハッピーサイズから書面
 * による許諾を得る必要があります。
 *
 * 「GNU Lesser General Public License」の全文は以下のウェブページから取得でき
 * ます。「GNU Lesser General Public License」とは、これまでLibrary General
 * Public Licenseと呼ばれていたものです。
 *     http://www.gnu.org/ --- GNUウェブサイト
 *     http://www.gnu.org/copyleft/lesser.html --- ライセンス文面
 * このライセンスの内容がわからない方、守れない方には使用を許諾しません。
 *
 * しかしながら、当社とGNUプロジェクトとの特定の関係を示唆または主張するもので
 * はありません。
 *
 * ◆保証内容◆
 *
 * このソフトウェアは、期待された動作・機能・性能を持つことを目標として設計され
 * 開発されていますが、これを保証するものではありません。このソフトウェアは「こ
 * のまま」の状態で提供されており、たとえばこのソフトウェアの有用性ないし特定の
 * 目的に合致することといった、何らかの保証内容が、明示されたり暗黙に示されてい
 * る場合であっても、その保証は無効です。このソフトウェアを使用した結果ないし使
 * 用しなかった結果によって、直接あるいは間接に受けた身体的な傷害、財産上の損害
 * 、データの損失あるいはその他の全ての損害については、その損害の可能性が使用者
 * 、当社あるいは第三者によって警告されていた場合であっても、当社はその損害の賠
 * 償および補填を行いません。この規定は他の全ての、書面上または書面に無い保証・
 * 契約・規定に優先します。
 *
 * ◆著作権者の連絡先および使用条件についての問い合わせ先◆
 *
 * 〒102-0073
 * 東京都千代田区九段北1-13-5日本地所第一ビル4F
 * 株式会社ハッピーサイズ
 * Phone: 03-3512-3655, Fax: 03-3512-3656
 * Email: sales@happysize.co.jp
 * Web: http://happysize.com/
 *
 * ◆著者◆
 *
 * 金本　茂 <sgk@happysize.co.jp>
 *
 * ◆履歴◆
 *
 * 1998/11/10 sgk implementation in C++
 * 1999/4/25  sgk Cで書きなおし。
 * 1999/4/26  sgk 入力フィルタを実装。漢字コードを推定しながらフィルタを追加。
 * 1999/6/??      Unicodeサポート。
 * 1999/6/22  sgk ライセンスをLGPLに変更。
 *
 */

/*
 * Unicode support
 *
 * Portions copyright (c) 1999,2000,2001 by the PHP3 internationalization team.
 * All rights reserved.
 *
 */

#include <stddef.h>
#include <string.h>

#include "mbfilter.h"
#include "mbfilter_8bit.h"
#include "mbfilter_wchar.h"
#include "filters/mbfilter_ascii.h"
#include "filters/mbfilter_base64.h"
#include "filters/mbfilter_qprint.h"
#include "filters/mbfilter_tl_jisx0201_jisx0208.h"
#include "filters/mbfilter_utf8.h"
#include "filters/mbfilter_utf16.h"

#include "mbfl_filter_output.h"
#include "eaw_table.h"
#include "filters/unicode_prop.h"

static inline void mbfl_filter_feed_char(int c, mbfl_convert_filter *filter)
{
	(filter->filter_function)(c, filter);
}

static inline unsigned char* mbfl_filter_feed_data(mbfl_string *string, mbfl_convert_filter *filter)
{
	return mbfl_convert_filter_feed_string(filter, string->val, string->len);
}

/* Used below for counting characters in a string */
static int filter_count_output(int c, void *data)
{
	(*(size_t *)data)++;
	return c;
}

/* Byte offset into multi-byte encoded string -> number of complete characters up to that position
 * Negative offsets count back from the end of the string, but cannot count back further than the start of the string
 * Positive offsets which are greater than the length of the string are clamped to the end of the string */
static long byte_offset_to_char_offset(const mbfl_encoding *encoding, unsigned char *start, size_t length, ssize_t byte_offset)
{
	if (byte_offset == 0) {
		return 0;
	} else if (byte_offset < 0) {
		byte_offset += length;
		if (byte_offset < 0) {
			/* negative offsets greater than length of string are clamped to zero */
			return 0;
		}
	} else if (byte_offset > length) {
		byte_offset = length;
	}

	/* If encoding uses a constant number of bytes per character, we can trivially convert
	 * byte offset to character offset */
	if (encoding->flag & MBFL_ENCTYPE_SBCS) {
		return byte_offset;
	} else if (encoding->flag & (MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE)) {
		return byte_offset / 2;
	} else if (encoding->flag & (MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE)) {
		return byte_offset / 4;
	} else if (encoding->mblen_table != NULL) {
		/* If not, can we determine the number of bytes for each character just by
		 * looking at the first byte? If so, we can run through the string, using
		 * a table lookup on the first byte to see how many bytes to skip ahead */
		const unsigned char *mbtab = encoding->mblen_table;
		unsigned char *p = start, *e = p + byte_offset;
		size_t len = 0;
		while (p < e) {
			p += mbtab[*p];
			len++;
		}
		if (p > e) {
			len--;
		}
		return len;
	} else {
		/* Otherwise, do it the slow way: convert the string to wchar, and count those  */
		size_t len = 0;
		mbfl_convert_filter *filter = mbfl_convert_filter_new(encoding, &mbfl_encoding_wchar, filter_count_output, 0, &len);
		mbfl_convert_filter_feed_string(filter, start, byte_offset);
		mbfl_convert_filter_delete(filter);
		return len;
	}
}

/* Character offset into multi-byte encoded string -> number of bytes up to that position
 * If requested offset is greater than character length of string, returns byte length of string
 * Negative offsets count back from the end of the string, but cannot count past the beginning */
MBFLAPI size_t char_offset_to_byte_offset(const mbfl_encoding *encoding, unsigned char *start, size_t length, ssize_t char_offset)
{
	if (char_offset == 0) {
		return 0;
	} else if (char_offset < 0) {
		/* Need to go backwards from end of string */
		char_offset = -char_offset;
		if (char_offset >= length) {
			/* Each character takes _at least_ one byte, so this will definitely go
			 * to the beginning of the string */
			return 0;
		}
		if (encoding->flag & MBFL_ENCTYPE_SBCS) {
			return MAX((long)length - char_offset, 0);
		} else if (encoding->flag & (MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE)) {
			return MAX((long)length - (char_offset << 1), 0);
		} else if (encoding->flag & (MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE)) {
			return MAX((long)length - (char_offset << 2), 0);
		} else if (encoding == &mbfl_encoding_utf8) {
			/* We can easily count backwards from the end of a UTF-8 string
			 * As we back up, decrement `char_offset` when we find a byte which is the first byte in a character
			 * It's easy to tell, since any byte which starts with bits 10 is a continuation byte and is not
			 * the first byte in a character */
			unsigned char *p = start + length;
			while (char_offset > 0 && --p > start) {
				unsigned char c = *p;
				if (c < 0x80) {
					char_offset--;
				} else if ((c & 0xC0) != 0x80) {
					char_offset--;
				}
			}
			return p - start;
		} else if (encoding == &mbfl_encoding_utf16le) {
			/* We can also easily count backwards from the end of a UTF-16 string */
			length &= -2; /* If there is a 'junk byte' at the end of the string, leave it out */
			unsigned char *p = start + length;
			while (char_offset > 0 && (p -= 2) > start) {
				unsigned char c = *p;
				if (c < 0xD8 || c >= 0xE0) {
					char_offset--;
				} else if ((c & 0xFC) == 0xD8) {
					char_offset--;
				}
			}
			return p - start;
		} else {
			/* If we don't have any efficient way to iterate backwards from the end
			 * of the string, the only sane thing to do is to iterate through
			 * the string _twice_. First, go through one time and find its length
			 * in characters. Then, go through a second time and stop when we reach
			 * the requested number of characters before the end.
			 *
			 * (If `char_offset` is a small number, we could also keep track of the
			 * last `char_offset` character positions passed by when iterating through
			 * the string. This would avoid the need to iterate over the string twice.
			 * But it's not good for the general case, since `char_offset` can be
			 * arbitrarily large.) */
			long char_len = byte_offset_to_char_offset(encoding, start, length, length);
			if (char_offset >= char_len) {
				return 0;
			}
			return char_offset_to_byte_offset(encoding, start, length, char_len - char_offset);
		}
	} else if (char_offset >= length) {
		return length;
	}

	/* `char_offset` is positive; we are searching from the beginning of the string.
	 * The easiest case is when each character takes a constant number of bytes */
	if (encoding->flag & MBFL_ENCTYPE_SBCS) {
		return MIN(char_offset, length);
	} else if (encoding->flag & (MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE)) {
		return MIN(char_offset * 2, length);
	} else if (encoding->flag & (MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE)) {
		return MIN(char_offset * 4, length);
	} else if (encoding->mblen_table != NULL) {
		const unsigned char *mbtab = encoding->mblen_table;
		unsigned char *p = start, *e = p + length;
		while (p < e && char_offset--) {
			p += mbtab[*p];
		}
		return MIN(p, e) - start;
	} else {
		/* Use a filter function to iterate through characters in string */
		size_t len = 0;
		mbfl_convert_filter *filter = mbfl_convert_filter_new(encoding, &mbfl_encoding_wchar, filter_count_output, 0, &len);
		unsigned char *p = start, *e = p + length;
		while (len < char_offset && p < e) {
			mbfl_filter_feed_char(*p++, filter);
		}
		mbfl_convert_filter_delete(filter);
		return p - start;
	}
}

/* If we can't convert between encodings `from` -> `to`, output parameters `filter1` and `filter2` will receive NULL */
static void create_encoding_conversion_filters(const mbfl_encoding *from, const mbfl_encoding *to, mbfl_memory_device *device,
	mbfl_convert_filter **filter1, mbfl_convert_filter **filter2)
{
	/* If we have a function which directly converts encoding `from` -> `to`, use it */
	if ((*filter1 = mbfl_convert_filter_new(from, to, mbfl_memory_device_output, NULL, device))) {
		/* `filter1` can handle the conversion, `filter2` will not be needed */
		*filter2 = NULL;
	} else {
		/* If not, can we do it by going through two stages of conversion?
		 * (`from` -> wchar -> `to`) */
		if ((*filter2 = mbfl_convert_filter_new(&mbfl_encoding_wchar, to, mbfl_memory_device_output, NULL, device))) {
			*filter1 = mbfl_convert_filter_new(from, &mbfl_encoding_wchar, (filter_output_func)((*filter2)->filter_function),
				((*filter2)->filter_flush), *filter2);
			if (*filter1 == NULL) {
				mbfl_convert_filter_delete(*filter2);
				*filter2 = NULL;
			}
		}
	}
}

static unsigned int* convert_string_to_wchar(mbfl_string *string, size_t *len)
{
	mbfl_wchar_device wdev;
	mbfl_wchar_device_init(&wdev, mbfl_strlen(string));
	mbfl_convert_filter *encoder = mbfl_convert_filter_new(string->encoding, &mbfl_encoding_wchar, mbfl_wchar_device_output, 0, &wdev);
	mbfl_filter_feed_data(string, encoder);
	mbfl_convert_filter_flush(encoder);
	mbfl_convert_filter_delete(encoder);
	*len = wdev.pos;
	return wdev.buffer; /* Caller is responsible for freeing this buffer */
}

/*
 *  buffering converter
 */
mbfl_buffer_converter* mbfl_buffer_converter_new(const mbfl_encoding *from, const mbfl_encoding *to, size_t buf_initsz)
{
	mbfl_buffer_converter *convd = emalloc(sizeof(mbfl_buffer_converter));
	convd->to = to;

	create_encoding_conversion_filters(from, to, &convd->device, &convd->filter1, &convd->filter2);
	if (convd->filter1 == NULL) {
		/* We are not able to convert between encodings `from` and `to` */
		efree(convd);
		return NULL;
	}

	mbfl_memory_device_init(&convd->device, buf_initsz, buf_initsz/4);
	return convd;
}

void mbfl_buffer_converter_delete(mbfl_buffer_converter *convd)
{
	mbfl_convert_filter_delete(convd->filter1);
	if (convd->filter2) {
		mbfl_convert_filter_delete(convd->filter2);
	}
	mbfl_memory_device_clear(&convd->device);
	efree((void*)convd);
}

void mbfl_buffer_converter_illegal_mode(mbfl_buffer_converter *convd, int mode)
{
	(convd->filter2 ? convd->filter2 : convd->filter1)->illegal_mode = mode;
}

void mbfl_buffer_converter_illegal_substchar(mbfl_buffer_converter *convd, int substchar)
{
	(convd->filter2 ? convd->filter2 : convd->filter1)->illegal_substchar = substchar;
}

size_t mbfl_buffer_converter_feed(mbfl_buffer_converter *convd, mbfl_string *string)
{
	/* Ensure that the buffer has enough space to hold the entire string; this is better
	 * for performance than repeatedly growing it in small increments */
	mbfl_memory_device_realloc(&convd->device, convd->device.pos + string->len, string->len / 4);
	return mbfl_filter_feed_data(string, convd->filter1) - string->val;
}

void mbfl_buffer_converter_flush(mbfl_buffer_converter *convd)
{
	mbfl_convert_filter_flush(convd->filter1);
}

mbfl_string* mbfl_buffer_converter_result(mbfl_buffer_converter *convd, mbfl_string *result)
{
	result->encoding = convd->to;
	mbfl_convert_filter_flush(convd->filter1);
	return mbfl_memory_device_result(&convd->device, result);
}

mbfl_string* mbfl_buffer_converter_feed_result(mbfl_buffer_converter *convd, mbfl_string *string, mbfl_string *result)
{
	mbfl_buffer_converter_feed(convd, string);
	return mbfl_buffer_converter_result(convd, result);
}

size_t mbfl_buffer_illegalchars(mbfl_buffer_converter *convd)
{
	size_t num_illegalchars = convd->filter1->num_illegalchar;
	if (convd->filter2 != NULL) {
		num_illegalchars += convd->filter2->num_illegalchar;
	}
	return num_illegalchars;
}

/*
 * encoding detector
 */
mbfl_encoding_detector* mbfl_encoding_detector_new(const mbfl_encoding **elist, int elistsz, int strict)
{
	ZEND_ASSERT(elistsz > 0);

	/* allocate */
	mbfl_encoding_detector *identd = emalloc(sizeof(mbfl_encoding_detector));
	identd->filter_list = ecalloc(elistsz, sizeof(mbfl_identify_filter *));

	/* create filters */
	for (int i = 0; i < elistsz; i++) {
		identd->filter_list[i] = mbfl_identify_filter_new2(elist[i]);
	}
	identd->filter_list_size = elistsz;
	identd->strict = strict;

	return identd;
}

void mbfl_encoding_detector_delete(mbfl_encoding_detector *identd)
{
	for (int i = identd->filter_list_size - 1; i >= 0; i--) {
		mbfl_identify_filter_delete(identd->filter_list[i]);
	}
	efree((void*)identd->filter_list);
	efree((void*)identd);
}

int mbfl_encoding_detector_feed(mbfl_encoding_detector *identd, mbfl_string *string)
{
	if (string->len > 0) {
		int num = identd->filter_list_size;
		unsigned char *p = string->val, *e = p + string->len;
		int bad = 0;

		/* We have an array of filter functions corresponding to different encodings
		 * Iterate over the string, pushing each byte into each and every filter function
		 *
		 * If the string is not encoding XYZ, then likely at some point we will hit a
		 * byte which is not valid in encoding XYZ, and the the 'flag' for that encoding
		 * will be set.
		 *
		 * Our hope is that at some point, all the candidate encodings will be flagged
		 * except one, which will presumably be the correct one. */
		while (p < e) {
			for (int i = 0; i < num; i++) {
				mbfl_identify_filter *filter = identd->filter_list[i];
				if (!filter->flag) {
					(filter->filter_function)(*p, filter);
					if (filter->flag) {
						bad++;
					}
				}
			}
			/* If all candidate encodings have failed, or there is only one remaining
			 * which has not failed, we can return.
			 *
			 * The exception is in strict mode; because when we discover that all
			 * other possible encodings have failed, we might still be in the middle
			 * of a multi-byte character for the 'correct' encoding, and stopping
			 * right here would cause `mbfl_encoding_detector_judge` (below) to
			 * wrongly reject it. */
			if ((num - 1) <= bad && !identd->strict) {
				return 1;
			}
			p++;
		}
	}

	return 0; /* No conclusion on which of the candidate encodings is correct */
}

const mbfl_encoding* mbfl_encoding_detector_judge(mbfl_encoding_detector *identd)
{
	for (int i = 0; i < identd->filter_list_size; i++) {
		mbfl_identify_filter *filter = identd->filter_list[i];
		/* `filter->status` is a state machine state variable for all the various
		 * filter functions which iterate over the characters in a multi-byte
		 * encoded string. When `status` is 0, it means a new character is starting.
		 *
		 * If we are in strict mode, then we don't accept a possible encoding as
		 * valid if it means the string would end in the middle of a multi-byte
		 * character. */
		if (!filter->flag && (!identd->strict || !filter->status)) {
			return filter->encoding;
		}
	}

	return NULL;
}

const mbfl_encoding* mbfl_identify_encoding(mbfl_string *string, const mbfl_encoding **elist, int elistsz, int strict)
{
	mbfl_encoding_detector *identd = mbfl_encoding_detector_new(elist, elistsz, strict);
	mbfl_encoding_detector_feed(identd, string);
	const mbfl_encoding *encoding = mbfl_encoding_detector_judge(identd);
	mbfl_encoding_detector_delete(identd);
	return encoding;
}

/*
 * encoding converter
 */
mbfl_string* mbfl_convert_encoding(mbfl_string *string, mbfl_string *result, const mbfl_encoding *toenc)
{
	mbfl_memory_device device;
	mbfl_convert_filter *filter1, *filter2;

	create_encoding_conversion_filters(string->encoding, toenc, &device, &filter1, &filter2);
	if (filter1 == NULL) {
		/* Can't convert from `string->encoding` to `toenc` */
		return NULL;
	}

	mbfl_memory_device_init(&device, string->len, (string->len >> 2) + 8);
	mbfl_filter_feed_data(string, filter1);
	mbfl_convert_filter_flush(filter1);
	mbfl_convert_filter_delete(filter1);
	if (filter2 != NULL) {
		mbfl_convert_filter_delete(filter2);
	}

	result->encoding = toenc;
	return mbfl_memory_device_result(&device, result);
}

/*
 *  strlen
 */
size_t mbfl_strlen(const mbfl_string *string)
{
	return byte_offset_to_char_offset(string->encoding, string->val, string->len, string->len);
}

/*
 *  strpos
 */
size_t mbfl_strpos(mbfl_string *haystack, mbfl_string *needle, ssize_t offset, int reverse)
{
	ZEND_ASSERT(haystack->encoding == needle->encoding);

	unsigned char *offset_pointer = haystack->val;
	if (offset != 0) {
		long str_len = mbfl_strlen(haystack); /* `long` because next comparison must be signed */
		if (offset > str_len || offset < -str_len) {
			return MBFL_ERROR_OFFSET;
		}
		offset_pointer += char_offset_to_byte_offset(haystack->encoding, haystack->val, haystack->len, offset);
	}

	if (haystack->len < needle->len) {
		return MBFL_ERROR_NOT_FOUND;
	}

	const char *found_pos;
	if (!reverse) {
		found_pos = zend_memnstr(
			(const char*)offset_pointer,
			(const char*)needle->val, needle->len,
			(const char*)haystack->val + haystack->len);
	} else if (offset >= 0) {
		found_pos = zend_memnrstr(
			(const char*)offset_pointer,
			(const char*)needle->val, needle->len,
			(const char*)haystack->val + haystack->len);
	} else {
		size_t needle_len = mbfl_strlen(needle);
		/* If we're searching in the reverse direction (from string end going backwards),
		 * and the offset was negative, then we will search the area _BEFORE_ the offset
		 * position (with a positive offset, we would search the area after the offset position)
		 *
		 * But if the needle is longer than 1 character, and it appears right at the end of
		 * the area to be searched, it may pass over the offset position, overlapping it
		 * Expand the area to be searched just enough so that we still find the needle in
		 * such cases */
		offset_pointer = offset_pointer + char_offset_to_byte_offset(haystack->encoding,
			offset_pointer, haystack->val + haystack->len - offset_pointer, needle_len);
		found_pos = zend_memnrstr(
			(const char*)haystack->val,
			(const char*)needle->val, needle->len,
			(const char*)offset_pointer);
	}

	if (found_pos) {
		return byte_offset_to_char_offset(haystack->encoding, haystack->val, haystack->len,
			((unsigned char*)found_pos) - haystack->val);
	}

	return MBFL_ERROR_NOT_FOUND;
}

/*
 *  substr_count
 */
size_t mbfl_substr_count(mbfl_string *haystack, mbfl_string *needle)
{
	ZEND_ASSERT(haystack->encoding == needle->encoding);

	if (mbfl_strlen(needle) == 0) {
		return MBFL_ERROR_EMPTY;
	}

	size_t result = 0;
	unsigned char *search_pos = haystack->val, *found_pos, *e = search_pos + haystack -> len;

	while (search_pos < e) {
		found_pos = (unsigned char*)zend_memnstr(
			(const char*)search_pos,
			(const char*)needle->val, needle->len,
			(const char*)haystack->val + haystack->len);
		if (!found_pos) {
			break;
		}
		result++;
		search_pos = found_pos + needle->len;
	}

	return result;
}

/*
 *  substr
 */
mbfl_string* mbfl_substr(mbfl_string *string, mbfl_string *result, size_t from, size_t length)
{
	const mbfl_encoding *encoding = string->encoding;

	mbfl_string_init_set(result, encoding);

	size_t start = char_offset_to_byte_offset(encoding, string->val, string->len, from);
	size_t end   = char_offset_to_byte_offset(encoding, string->val + start, string->len - start, length) + start;

	result->val = ecalloc(end - start, sizeof(unsigned char));
	result->len = end - start;
	memcpy(result->val, string->val + start, end - start);

	return result;
}

/*
 *  strcut
 */
static inline void strcut_const_width(mbfl_string *string, const mbfl_encoding *encoding, size_t from, const unsigned char **start, size_t *length)
{
	/* If the requested `from` and `length` would give us a range of bytes which starts
	 * or ends in the middle of a multi-byte character, push the start position and/or
	 * the end position forwards so we get only complete characters. This means the extracted
	 * range may include slightly more or less bytes than requested.
	 *
	 * It's easiest when each character is encoded with a constant number of bytes: */
	int bytes_per_char;
	if (encoding->flag & (MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE)) {
		bytes_per_char = 2;
	} else if (encoding->flag & (MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE)) {
		bytes_per_char = 4;
	} else {
		bytes_per_char = 1;
	}

	from &= -bytes_per_char; /* push start position forward by zeroing low bits */
	*length = MIN(*length, string->len - from) & -bytes_per_char;
	*start = string->val + from;
}

static inline void strcut_utf8(mbfl_string *string, size_t from, const unsigned char **start, size_t *length)
{
	/* It is easy to tell if we are in the middle of a multi-byte character
	 * with UTF-8; all continuation bytes are like 10xxxxxx (in binary) */
	unsigned char *search_pos = string->val + from;
	while (((*search_pos & 0xC0) == 0x80) && (search_pos > string->val)) {
		search_pos--;
	}
	*start = search_pos;

	size_t max_len = string->val + string->len - search_pos;
	if (*length >= max_len) {
		*length = max_len;
	} else {
	  unsigned char *end = search_pos + *length;
	  while (((*end & 0xC0) == 0x80) && (end > search_pos)) {
			end--;
	  }
	  *length = end - search_pos;
	}
}

static inline void strcut_utf16le(mbfl_string *string, size_t from, const unsigned char **start, size_t *length)
{
	/* UTF-16 uses either 2 or 4 bytes per character; it's easy to tell if we
	 * are in the middle of a 4-byte char, since all 4-byte chars are encoded
	 * as 'surrogate pairs', like 110110xx xxxxxxxx 110111xx xxxxxxxx in binary */
	from &= -2;
	unsigned char *search_pos = string->val + from;
	if (((*search_pos & 0xFC) == 0xDC) && (search_pos - 2 >= string->val)) {
		search_pos -= 2;
	}
	*start = search_pos;

	*length &= -2;
	size_t max_len = string->val + string->len - *start;
	if (*length >= max_len) {
		*length = max_len;
	} else {
	  unsigned char *end = search_pos + *length;
	  if (((*end & 0xFC) == 0xDC) && (end - 2 >= search_pos)) {
			end -= 2;
	  }
	  *length = end - search_pos;
	}
}

static inline void strcut_with_mblen_table(mbfl_string *string, const unsigned char *mblen_table, size_t from, const unsigned char **start, size_t *length)
{
	/* If we can determine the number of bytes each character is encoded in by
	 * looking at the first byte, we can iterate through the string using a
	 * '1st byte -> byte length' table, see if we land on the requested start
	 * and end positions, and adjust if not */
	const unsigned char *search_pos = string->val, *search_end = search_pos + from;
	int byte_len = 0;

	while (search_pos < search_end) {
		search_pos += (byte_len = mblen_table[*search_pos]);
	}

	if (search_pos > search_end) {
		search_pos -= byte_len;
	}

	*start = search_pos;

	if (*length >= string->val + string->len - *start) {
		/* If the requested length goes beyond the end of the string, there is no need
		 * to search for the requested end position and see if it is valid; just go to
		 * the end of the string */
		*length = string->val + string->len - *start;
	} else {
		search_end = search_pos + *length;

		while (search_pos < search_end) {
			search_pos += (byte_len = mblen_table[*search_pos]);
		}

		if (search_pos > search_end) {
			search_pos -= byte_len;
		}

		*length = search_pos - *start;
	}
}

mbfl_string* mbfl_strcut(mbfl_string *string, mbfl_string *result, size_t from, size_t length)
{
	const mbfl_encoding *encoding = string->encoding;
	const unsigned char *start = NULL;

	if (from >= string->len) {
		from = string->len;
	}

	if ((encoding->flag & (MBFL_ENCTYPE_SBCS | MBFL_ENCTYPE_WCS2BE | MBFL_ENCTYPE_WCS2LE | MBFL_ENCTYPE_WCS4BE | MBFL_ENCTYPE_WCS4LE))) {
		strcut_const_width(string, encoding, from, &start, &length);
	} else if (encoding == &mbfl_encoding_utf8) {
		strcut_utf8(string, from, &start, &length);
	} else if (encoding == &mbfl_encoding_utf16le) {
		strcut_utf16le(string, from, &start, &length);
	} else if (encoding->mblen_table != NULL) {
		strcut_with_mblen_table(string, encoding->mblen_table, from, &start, &length);
	} else {
		/* This is the slowest case; we have to use one of our filter functions
		 * (string decoding state machine) to iterate through the string, look for
		 * the requested start and end positions, and see if they land on a
		 * character boundary or not */
		mbfl_convert_filter *decoder = mbfl_convert_filter_new(string->encoding, &mbfl_encoding_wchar, NULL, NULL, NULL);

		/* It should be possible to convert any encoding to wchar, though we don't
		 * actually want to convert anything; a peculiarity of libmbfl is that to
		 * iterate over a string using mbfl_convert_filter, we have to convert it
		 * to something */
		ZEND_ASSERT(decoder);

		unsigned char *search_pos = string->val, *search_end = search_pos + from, *last_valid_pos = string->val;

		while (search_pos < search_end) {
			mbfl_filter_feed_char(*search_pos++, decoder);
			if (decoder->status == 0) {
				/* We have just passed by a complete character
				 * (For each encoding filter, 0 is the starting state of its state
				 * machine, used when it is just entering a new character) */
				last_valid_pos = search_pos;
			}
		}

		start = last_valid_pos;

		if (length >= string->val + string->len - start) {
			/* If the requested length goes beyond the end of the string, there is no need
			 * to search for the requested end position and see if it is valid; just go to
			 * the end of the string */
			length = string->val + string->len - start;
		} else {
			search_end += length;
			while (search_pos < search_end) {
				mbfl_filter_feed_char(*search_pos++, decoder);
				if (decoder->status == 0) { /* See comment above */
					last_valid_pos = search_pos;
				}
			}
			length = last_valid_pos - start;
		}

		mbfl_convert_filter_delete(decoder);
	}

	result->val = ecalloc(length, sizeof(unsigned char));
	memcpy(result->val, start, length);
	result->len = length;
	result->encoding = encoding;
	return result;
}

/*
 *  strwidth
 */

/* Some East Asian characters, when printed at a terminal (or the like), require double
 * the usual amount of horizontal space. We call these "fullwidth" characters. */
static size_t character_width(int c)
{
	/* The first fullwidth codepoint is 0x1100; the most commonly used characters,
	 * including all those in the ASCII character set, are below that */
	if (c < 0x1100) {
		return 1;
	}

	/* Do a binary search to see if we fall in any of the fullwidth ranges */
	int lo = 0, hi = sizeof(mbfl_eaw_table) / sizeof(mbfl_eaw_table[0]);
	while (lo < hi) {
		int probe = (lo + hi) / 2;
		if (c < mbfl_eaw_table[probe].begin) {
			hi = probe;
		} else if (c > mbfl_eaw_table[probe].end) {
			lo = probe + 1;
		} else {
			return 2;
		}
	}

	return 1;
}

static int filter_count_width(int c, void* data)
{
	(*(size_t *)data) += character_width(c);
	return c;
}

size_t mbfl_strwidth(mbfl_string *string)
{
	size_t width = 0;

	if (string->len > 0) {
		mbfl_convert_filter *filter = mbfl_convert_filter_new(string->encoding, &mbfl_encoding_wchar, filter_count_width, 0, &width);
		mbfl_filter_feed_data(string, filter);
		mbfl_convert_filter_flush(filter);
		mbfl_convert_filter_delete(filter);
	}

	return width;
}

/*
 *  strimwidth
 */
mbfl_string* mbfl_strimwidth(mbfl_string *string, mbfl_string *marker, mbfl_string *result, size_t max_width)
{
	/* Cut string down to total display width of `max_width` or less, where East Asian width is taken into account
	 * If it is necessary to trim string, add `marker` at end (unless the marker is NULL)
	 * Even after adding the trim marker, the total display width of `max_width` will not be exceeded, except in
	 * one pathological case... when BOTH the string width AND the width of the trim marker are more than `max_width` */

	mbfl_string_init_set(result, string->encoding);

	if (string->len == 0) {
		return result;
	}

	unsigned char *start = string->val, *e = start + string->len;

	/* Check how far we can get into remainder of string while still leaving
	 * enough space for the trim marker, in case it is necessary to add it */
	size_t marker_width = (marker ? mbfl_strwidth(marker) : 0);

	size_t width = 0;
	mbfl_convert_filter *filter = mbfl_convert_filter_new(string->encoding,
		&mbfl_encoding_wchar, filter_count_width, 0, &width);

	unsigned char *p = start, *enough_space_for_marker = start;
	while (p < e) {
		mbfl_filter_feed_char(*p++, filter);
		if (width + marker_width <= max_width) {
			/* If `filter->status` is zero, we have just passed over a complete
			 * character and are entering another one */
			if (filter->status == 0) {
				enough_space_for_marker = p;
			}
		} else {
			break;
		}
	}

	/* OK, we've found the last position where the trim marker could fit.
	 * Now let's see if the rest of the string will fit in `max_width` or not */
	while (width < max_width && p < e) {
		mbfl_filter_feed_char(*p++, filter);
	}
	mbfl_convert_filter_delete(filter);

	if (p == e && width <= max_width) {
		/* The whole input string fits within `max_width`, at least after skipping
		 * over `from` characters, so there is no need to add the trim marker */
		size_t length = string->val + string->len - start;
		result->val = ecalloc(length, sizeof(unsigned char));
		memcpy(result->val, start, length);
		result->len = length;
	} else {
		/* We need to trim the input string and add the trim marker */
		size_t trimmed_length = enough_space_for_marker - start;
		size_t marker_length  = (marker ? marker->len : 0);
		result->val = ecalloc(trimmed_length + marker_length, sizeof(unsigned char));
		memcpy(result->val, start, trimmed_length);
		memcpy(result->val + trimmed_length, marker->val, marker_length);
		result->len = trimmed_length + marker_length;
	}

	return result;
}

/* Perform various transformations on Japanese text
 *
 * Some Japanese characters have both halfwidth and fullwidth versions; 'hantozen' refers
 * to converting halfwidth to fullwidth.
 * However, this function can also convert in the opposite direction, or convert
 * hiragana to katakana and so on... all depending on the value of the `mode` argument */
mbfl_string* mbfl_ja_jp_hantozen(mbfl_string *string, mbfl_string *result, intptr_t mode)
{
	mbfl_memory_device device;

	mbfl_memory_device_init(&device, string->len, 0);
	mbfl_string_init_set(result, string->encoding);

	mbfl_convert_filter *decoder = mbfl_convert_filter_new(&mbfl_encoding_wchar,
		string->encoding, mbfl_memory_device_output, 0, &device);

	mbfl_convert_filter *tl_filter = mbfl_convert_filter_new2(&vtbl_tl_jisx0201_jisx0208,
		(filter_output_func)decoder->filter_function, decoder->filter_flush, decoder);
	tl_filter->opaque = (void*)mode;

	mbfl_convert_filter *encoder = mbfl_convert_filter_new(string->encoding, &mbfl_encoding_wchar,
		(filter_output_func)tl_filter->filter_function, tl_filter->filter_flush, tl_filter);

	mbfl_filter_feed_data(string, encoder);
	mbfl_convert_filter_flush(encoder);
	mbfl_memory_device_result(&device, result);

	mbfl_convert_filter_delete(tl_filter);
	mbfl_convert_filter_delete(decoder);
	mbfl_convert_filter_delete(encoder);

	return result;
}

/*
 *  MIME header encode
 */
struct mime_header_encoder_data {
	mbfl_memory_device outdev;
	mbfl_encoding *output_encoding;
	mbfl_encoding *transfer_encoding;
	size_t linehead;
	size_t firstindent;
	int encname_len;
	int linefeed_len;
	/* The longest preferred MIME name for an encoding is 16 characters: "Quoted-Printable"
	 * So `encname` will never be longer than 21 characters, since 5 more are added to the MIME name */
	char encname[24];
	char linefeed[16];
};

struct mime_header_encoder_data* mime_header_encoder_new(const mbfl_encoding *incode,
	const mbfl_encoding *outcode, const mbfl_encoding *transenc)
{
	ZEND_ASSERT(transenc == &mbfl_encoding_qprint || transenc == &mbfl_encoding_base64);

	/* check MIME charset name for content encoding */
	if (outcode->mime_name == NULL || outcode->mime_name[0] == '\0') {
		return NULL;
	}

	struct mime_header_encoder_data *pe = emalloc(sizeof(struct mime_header_encoder_data));
	mbfl_memory_device_init(&pe->outdev, 0, 0);
	pe->linehead = pe->firstindent = 0;
	pe->output_encoding = (mbfl_encoding*)outcode;
	pe->transfer_encoding = (mbfl_encoding*)transenc;

	/* make the encoding description string  exp. "=?ISO-2022-JP?B?" */
	pe->encname_len = snprintf(pe->encname, sizeof(pe->encname), "=?%s?%c?",
		outcode->mime_name, (transenc == &mbfl_encoding_qprint ? 'Q' : 'B'));

	pe->linefeed_len = snprintf(pe->linefeed, sizeof(pe->linefeed), "\r\n ");

	return pe;
}

void mime_header_encoder_delete(struct mime_header_encoder_data *pe)
{
	mbfl_memory_device_clear(&pe->outdev);
	efree((void*)pe);
}

static int qp_table[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x00 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x10 */
	0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x20 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, /* 0x30 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x40 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, /* 0x50 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, /* 0x60 */
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, /* 0x70 */
};

#define EMIT_SPACE() mbfl_memory_device_output(' ', &pe->outdev)

static void mime_header_break_line(struct mime_header_encoder_data *pe)
{
	mbfl_memory_device_strncat(&pe->outdev, pe->linefeed, pe->linefeed_len);
	pe->linehead = pe->outdev.pos;
	pe->firstindent = 0; /* We're no longer on the first line */
}

static void mime_header_break_marked_line(struct mime_header_encoder_data *pe)
{
	mbfl_memory_device_strncat(&pe->outdev, "?=", 2);
	mime_header_break_line(pe);
	mbfl_memory_device_strncat(&pe->outdev, pe->encname, pe->encname_len);
}

static void mime_header_flush_to_output(struct mime_header_encoder_data *pe, unsigned int *start, unsigned int *end)
{
	int line_length = pe->outdev.pos - pe->linehead + pe->firstindent + (end - start);

	if (line_length > 74) {
		mime_header_break_line(pe);
	} else if (pe->outdev.pos > 0) {
		EMIT_SPACE();
	}

	while (start < end) {
		mbfl_memory_device_output(*start++, &pe->outdev);
	}
}

static void mime_header_emit_encoded_line(mbfl_memory_device *converted, mbfl_convert_filter *convert_filter, mbfl_convert_filter *encode_filter,
	struct mime_header_encoder_data *pe)
{
	(convert_filter->filter_flush)(convert_filter);
	mbfl_convert_filter_devcat(encode_filter, converted);
	(encode_filter->filter_flush)(encode_filter);
	mime_header_break_marked_line(pe);
	mbfl_memory_device_reset(converted);
}

static void mime_header_base64_output(int c, mbfl_memory_device *converted, mbfl_convert_filter *convert_filter, mbfl_convert_filter *encode_filter,
	struct mime_header_encoder_data *pe)
{
	int line_length = pe->outdev.pos - pe->linehead + pe->firstindent;
	int max_bytes = (73 - line_length) / 4 * 3;

	int saved_pos = converted->pos, saved_status = convert_filter->status, saved_cache = convert_filter->cache;
	mbfl_filter_feed_char(c, convert_filter);

	/* If we're getting close to where we need to break to another line, check whether we
	 * will still be within the line length limit after flushing the conversion filter */
	if (converted->pos > max_bytes - 6) {
		int saved_pos2 = converted->pos, saved_status2 = convert_filter->status, saved_cache2 = convert_filter->cache;
		(convert_filter->filter_flush)(convert_filter);

		if (converted->pos > max_bytes) {
			/* Looks like `c` will not actually fit on this line
			 * Back up to where we were before consuming `c` */
			converted->pos = saved_pos;
			convert_filter->status = saved_status;
			convert_filter->cache = saved_cache;

			/* Emit a completed line and then consume `c` again */
			mime_header_emit_encoded_line(converted, convert_filter, encode_filter, pe);
			mbfl_filter_feed_char(c, convert_filter);
		} else {
			/* OK, `c` will fit. This is good to know. But we need to undo the flush
			 * so more characters can potentially fit on the same line */
			converted->pos = saved_pos2;
			convert_filter->status = saved_status2;
			convert_filter->cache = saved_cache2;
		}
	}
}

static void mime_header_qprint_output(int c, mbfl_memory_device *converted, mbfl_convert_filter *convert_filter, mbfl_convert_filter *encode_filter,
	struct mime_header_encoder_data *pe)
{
	int saved_pos = converted->pos, saved_status = convert_filter->status, saved_cache = convert_filter->cache;
	mbfl_filter_feed_char(c, convert_filter);

	int line_length = pe->outdev.pos - pe->linehead + pe->firstindent;
	unsigned char *p = converted->buffer, *e = p + converted->pos;
	while (p < e) {
		unsigned char byte = *p++;
		line_length += (byte >= 0x80 || byte == '=' || mbfl_charprop_table[byte]) ? 3 : 1;
	}

	/* `line_length` must not exceed 73 at the point where we decide to break, to make
	 * space for the terminating '?='
	 * Flushing the conversion filter should not emit more than 4 bytes. Those 4 bytes
	 * could potentially expand to 12 bytes when QPrint-encoded. */
	if (line_length > 73 - 12) {
		int saved_pos2 = converted->pos, saved_status2 = convert_filter->status, saved_cache2 = convert_filter->cache;
		(convert_filter->filter_flush)(convert_filter);

		e = converted->buffer + converted->pos;
		while (p < e)	{
			unsigned char byte = *p++;
			line_length += (byte >= 0x80 || byte == '=' || mbfl_charprop_table[byte]) ? 3 : 1;
		}

		if (line_length > 73) {
			/* Looks like `c` will not actually fit on this line
			 * Back up to where we were before consuming `c` */
			converted->pos = saved_pos;
			convert_filter->status = saved_status;
			convert_filter->cache = saved_cache;

			/* Emit a completed line and then consume `c` again */
			mime_header_emit_encoded_line(converted, convert_filter, encode_filter, pe);
			mbfl_filter_feed_char(c, convert_filter);
		} else {
			/* OK, `c` will fit. This is good to know. But we need to undo the flush
			 * so more characters can potentially fit on the same line */
			converted->pos = saved_pos2;
			convert_filter->status = saved_status2;
			convert_filter->cache = saved_cache2;
		}
	}
}

static void mime_header_encode_rest_of_input(unsigned int *p, unsigned int *e, struct mime_header_encoder_data *pe)
{
	mbfl_memory_device converted;
	mbfl_memory_device_init(&converted, 0, 0);

	/* Transfer encode filter */
	mbfl_convert_filter *encode_filter = mbfl_convert_filter_new(pe->output_encoding, pe->transfer_encoding, mbfl_memory_device_output, 0, &pe->outdev);

	/* Output code filter */
	mbfl_convert_filter *conv2_filter = mbfl_convert_filter_new(&mbfl_encoding_wchar, pe->output_encoding, mbfl_memory_device_output, 0, &converted);

	void (*output_function)(int, mbfl_memory_device*, mbfl_convert_filter*, mbfl_convert_filter*, struct mime_header_encoder_data*);
	if (pe->transfer_encoding == &mbfl_encoding_qprint) {
		encode_filter->status |= MBFL_QPRINT_STS_MIME_HEADER;
		output_function = mime_header_qprint_output;
	} else {
		encode_filter->status |= MBFL_BASE64_STS_MIME_HEADER;
		output_function = mime_header_base64_output;
	}

	int line_length = pe->outdev.pos - pe->linehead + pe->encname_len + pe->firstindent;
	if (line_length > 60)  {
		mime_header_break_line(pe);
	} else if (pe->outdev.pos > 0)  {
		EMIT_SPACE();
	}

	mbfl_memory_device_strncat(&pe->outdev, pe->encname, pe->encname_len);

	while (p < e) {
		output_function(*p++, &converted, conv2_filter, encode_filter, pe);
	}

	(conv2_filter->filter_flush)(conv2_filter);
	mbfl_convert_filter_devcat(encode_filter, &converted);
	(encode_filter->filter_flush)(encode_filter);

	mbfl_memory_device_strncat(&pe->outdev, "?=", 2);

	mbfl_memory_device_clear(&converted);
	mbfl_convert_filter_delete(conv2_filter);
	mbfl_convert_filter_delete(encode_filter);
}

mbfl_string* mbfl_mime_header_encode(mbfl_string *string, mbfl_string *result, const mbfl_encoding *outcode,
	const mbfl_encoding *transfer_encoding, const char *linefeed, int indent)
{
	mbfl_string_init_set(result, &mbfl_encoding_ascii);

	struct mime_header_encoder_data *pe = mime_header_encoder_new(string->encoding, outcode, transfer_encoding);
	if (pe == NULL) {
		return NULL;
	}

	if (linefeed != NULL) {
		pe->linefeed_len = snprintf(pe->linefeed, sizeof(pe->linefeed), "%.8s ", linefeed);
	}
	if (indent > 0 && indent < 74) {
		pe->firstindent = indent;
	}

	size_t len;
	unsigned int *wc_buffer = convert_string_to_wchar(string, &len), *p = wc_buffer, *e = p + len, *saved = p;

	while (p < e) {
		int c = *p++;

		/* Accept a run of spaces followed by a run of characters which do not require
		 * any special encoding. When we see another space after that, flush to output
		 * buffer, breaking line if necessary.
		 *
		 * If at any point we see a character which *does* need special encoding,
		 * switch to Base64-encoding or QPrint-encoding all the input */
		if (c <= 0x7f && qp_table[c]) {
			while (p < e) {
				int c = *p++;

				if (c == ' ' && (p - saved - 1) < 74) {
					mime_header_flush_to_output(pe, saved, p - 1);
					saved = p;
					break;
				} else if (c > 0x7f || !qp_table[c]) {
					mime_header_encode_rest_of_input(saved, e, pe);
					goto mime_header_encode_finished_input;
				}
			}
		} else if (c != ' ') {
			mime_header_encode_rest_of_input(saved, e, pe);
			goto mime_header_encode_finished_input;
		}
	}

	if (saved < e) {
		mime_header_flush_to_output(pe, saved, e);
	}

mime_header_encode_finished_input:
	efree(wc_buffer);
	mbfl_memory_device_result(&pe->outdev, result);
	mime_header_encoder_delete(pe);
	return result;
}

/*
 *  MIME header decode
 */
struct mime_header_decoder_data {
	mbfl_convert_filter *decode_filter;
	mbfl_convert_filter *conv1_filter;
	mbfl_convert_filter *conv2_filter;
	mbfl_memory_device outdev;
};

mbfl_string* mime_header_decoder_result(struct mime_header_decoder_data *pd, mbfl_string *result)
{
	(*pd->decode_filter->filter_flush)(pd->decode_filter);
	return mbfl_memory_device_result(&pd->outdev, result);
}

struct mime_header_decoder_data* mime_header_decoder_new(const mbfl_encoding *outcode)
{
	struct mime_header_decoder_data *pd = emalloc(sizeof(struct mime_header_decoder_data));

	mbfl_memory_device_init(&pd->outdev, 0, 0);
	pd->conv2_filter = mbfl_convert_filter_new(&mbfl_encoding_wchar, outcode, mbfl_memory_device_output, 0, &pd->outdev);
	pd->conv1_filter = mbfl_convert_filter_new(&mbfl_encoding_ascii, &mbfl_encoding_wchar, (filter_output_func)(pd->conv2_filter->filter_function),
		pd->conv2_filter->filter_flush, pd->conv2_filter);

	/* decode filter; 'from' encoding may change to base64 or qprint later */
	pd->decode_filter = mbfl_convert_filter_new(&mbfl_encoding_8bit, &mbfl_encoding_8bit, mbfl_filter_output_pipe,
		pd->conv1_filter->filter_flush, pd->conv1_filter);

	return pd;
}

void mime_header_decoder_delete(struct mime_header_decoder_data *pd)
{
	mbfl_convert_filter_delete(pd->conv2_filter);
	mbfl_convert_filter_delete(pd->conv1_filter);
	mbfl_convert_filter_delete(pd->decode_filter);
	mbfl_memory_device_clear(&pd->outdev);
	efree((void*)pd);
}

static unsigned char* mime_header_skip_whitespace(unsigned char *p, unsigned char *e)
{
	while (p < e) {
		unsigned char c = *p;
		if (c == 0x0d || c == 0x0a || c == 0x20 || c == 0x09) {
			p++;
		} else {
			return p;
		}
	}
	return p;
}

static const mbfl_encoding* mime_identify_transfer_encoding(unsigned char c)
{
	if (c == 'B' || c == 'b') {
		return &mbfl_encoding_base64;
	} else if (c == 'Q' || c == 'q') {
		return &mbfl_encoding_qprint;
	} else {
		return NULL;
	}
}

static const mbfl_encoding* mime_identify_content_encoding(unsigned char *p, size_t len)
{
	/* It would be nice if we had a variant of `mbfl_name2encoding` which took string length as a parameter
	 * Then we wouldn't have to `memcpy` the string elsewhere just for the sake of adding a terminating NUL
	 * We are not guaranteed that `p + len` is a valid pointer (it could run off the end of a string),
	 * so we can't swap a NUL for `*(p + len)` and swap it back after */
	unsigned char charset_name[64];
	memcpy(charset_name, p, len);
	charset_name[len] = '\0';
	return mbfl_name2encoding((const char *)charset_name);
}

static void mime_prepare_filters_for_encword_contents(struct mime_header_decoder_data *pd, const mbfl_encoding *content_encoding,
	const mbfl_encoding *transfer_encoding)
{
	mbfl_convert_filter_reset(pd->conv1_filter, content_encoding, &mbfl_encoding_wchar);
	pd->decode_filter->flush_function  = pd->conv1_filter->filter_flush;
	mbfl_convert_filter_reset(pd->decode_filter, transfer_encoding, &mbfl_encoding_8bit);
}

static void mime_reset_filter_after_encword(struct mime_header_decoder_data *pd)
{
	(*pd->decode_filter->filter_flush)(pd->decode_filter);
	mbfl_convert_filter_reset(pd->conv1_filter, &mbfl_encoding_ascii, &mbfl_encoding_wchar);
}

#define NOT_ENCODED_WORD() do { p--; \
	mbfl_convert_filter_feed_string(pd->conv1_filter, encword_start, (p - encword_start)); \
	goto mime_header_decode_feed; } while(0)

mbfl_string* mbfl_mime_header_decode(mbfl_string *string, mbfl_string *result, const mbfl_encoding *outcode)
{
	struct mime_header_decoder_data *pd = mime_header_decoder_new(outcode);
	unsigned char *p = string->val, *e = string->val + string->len, *encword_start;
	mbfl_string_init_set(result, outcode);

mime_header_decode_feed:
	while (p < e) {
		int c = *p++;

		if (c == 0x0d || c == 0x0a) { /* CR LF */
			p = mime_header_skip_whitespace(p, e);
			mbfl_filter_feed_char(' ', pd->conv1_filter); /* Emit space */
		} else if (c == '=' && p < e) {
			encword_start = p - 1;

			if (*p++ != '?')
				NOT_ENCODED_WORD();

			/* We have passed over '=?', which should be the start of a MIME 'encoded word'
			 * Content encoding name should follow. Run over it and see if it is an encoding we understand */
			while (p < e) {
				c = *p++;

				if (c == '?') { /* End of content encoding name */
					const mbfl_encoding *encoding = mime_identify_content_encoding(encword_start + 2, p - encword_start - 3);
					if (encoding == NULL || p >= e)
						NOT_ENCODED_WORD();

					const mbfl_encoding *transfer_encoding = mime_identify_transfer_encoding(*p++);
					if (transfer_encoding == NULL || p >= e)
						NOT_ENCODED_WORD();

					if (*p++ != '?')
						NOT_ENCODED_WORD();

					mime_prepare_filters_for_encword_contents(pd, encoding, transfer_encoding);

					while (p < e && (c = *p++) != '?') {
						mbfl_filter_feed_char(c, pd->decode_filter);
					}

					/* Skip over whatever is presumably '='; an encoded word is terminated by '?=', and after you see '?',
					 * the only thing which can legally follow is '=' */
					p++;

					mime_reset_filter_after_encword(pd);

					/* Normally we emit a space after skipping a block of whitespace, but we don't want to do so here */
					p = mime_header_skip_whitespace(p, e);
					break;
				} else if ((p - encword_start) > 100 || c == 0x0d || c == 0x0a) { /* too long charset string or CR or LF */
					NOT_ENCODED_WORD();
				}
			}
		} else {
			mbfl_filter_feed_char(c, pd->conv1_filter); /* Emit character to output */
		}
	}

	result = mime_header_decoder_result(pd, result);
	mime_header_decoder_delete(pd);
	return result;
}


/*
 *  convert HTML numeric entity
 */
ZEND_SET_ALIGNED(2, static char decimal_digits[]) = "00010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081828384858687888990919293949596979899";

mbfl_string* mbfl_html_numeric_entity_encode(mbfl_string *string, mbfl_string *result, int *convmap,
	int mapsize, int hex)
{
	mbfl_string_init_set(result, string->encoding);
	mbfl_memory_device device;
	mbfl_memory_device_init(&device, string->len, 0);
	mbfl_convert_filter *decoder = mbfl_convert_filter_new(&mbfl_encoding_wchar, string->encoding,
		mbfl_memory_device_output, 0, &device);

	size_t len;
	unsigned int *wc_buffer = convert_string_to_wchar(string, &len), *p = wc_buffer, *e = p + len;
	int *convmap_end = convmap + (mapsize * 4);
	ZEND_SET_ALIGNED(2, unsigned char buf[16]); /* We will do 16-bit writes into this buffer */

next_character:
	while (p < e) {
		unsigned int c = *p++;

		for (int *mapelm = convmap; mapelm < convmap_end; mapelm += 4) {
			int lo_code = mapelm[0];
			int hi_code = mapelm[1];
			int offset  = mapelm[2];
			int mask    = mapelm[3];

			if (c >= lo_code && c <= hi_code) {
				c = (c + offset) & mask;

				mbfl_filter_feed_char('&', decoder);
				mbfl_filter_feed_char('#', decoder);

				/* Convert codepoint to decimal/hex string */
				if (c == 0) {
					mbfl_filter_feed_char('0', decoder);
				} else {
					unsigned char *converted = buf + sizeof(buf);
					if (hex) {
						while (c > 0) {
							*(--converted) = "0123456789ABCDEF"[c & 0xF];
							c >>= 4;
						}
					} else {
						while (c >= 10) {
							converted -= 2;
							*((uint16_t*)converted) = ((uint16_t*)decimal_digits)[c % 100];
							c /= 100;
						}
						if (c > 0) {
							*(--converted) = "0123456789"[c % 10];
						}
					}
					mbfl_convert_filter_feed_string(decoder, converted, buf + sizeof(buf) - converted);
				}

				mbfl_filter_feed_char(';', decoder);
				goto next_character;
			}
		}
		/* None of the ranges matched; pass this character through unmodified */
		mbfl_filter_feed_char(c, decoder);
	}

	efree(wc_buffer);
	mbfl_convert_filter_flush(decoder);
	mbfl_memory_device_result(&device, result);
	mbfl_convert_filter_delete(decoder);
	return result;
}

static int html_numeric_entity_deconvert(int number, int *convmap, int mapsize)
{
	int *convmap_end = convmap + (mapsize * 4);

	for (int *mapelm = convmap; mapelm < convmap_end; mapelm += 4) {
		int lo_code = mapelm[0];
		int hi_code = mapelm[1];
		int offset  = mapelm[2];
		int codepoint = number - offset;
		if (codepoint >= lo_code && codepoint <= hi_code) {
			return codepoint;
		}
	}

	return -1;
}

mbfl_string* mbfl_html_numeric_entity_decode(mbfl_string *string, mbfl_string *result, int *convmap, int mapsize)
{
	mbfl_string_init_set(result, string->encoding);
	mbfl_memory_device device;
	mbfl_memory_device_init(&device, string->len, 0);
	mbfl_convert_filter *decoder = mbfl_convert_filter_new(&mbfl_encoding_wchar, string->encoding,
		mbfl_memory_device_output, 0, &device);

	size_t len;
	unsigned int *wc_buffer = convert_string_to_wchar(string, &len), *p = wc_buffer, *e = wc_buffer + len;

continue_decoding_html_entity:
	while (p < e) {
		unsigned int c = *p++;

		if (c == '&' && p < e) {
			/* OK, we have something which _might_ be an HTML numeric entity. Let's see what comes next... */
			unsigned int *entity = p - 1; /* So we can go back and output these chars if it's not a valid numeric entity */
			c = *p++;

			if (c == '#' && p < e) {
				c = *p++;

				if (c == 'x' && p < e) {
					/* Start decoding hexadecimal number */
					int hex = 0, digits = 0;
					c = *p++;

					/* If very next character is not a hex digit, then what we have found
					 * is not actually a hexadecimal numeric entity */
					if ((c < '0' || c > '9') && (c < 'A' || c > 'F') && (c < 'a' || c > 'f')) {
						goto not_valid_html_entity;
					} else {
						goto process_hex_digit;
					}

					while (p < e) {
						c = *p++;
process_hex_digit:
						if (digits++ >= 10) {
							goto not_valid_html_entity;
						}

						if (c >= '0' && c <= '9') {
							hex = (hex * 16) + (c - '0');
						} else if (c >= 'A' && c <= 'F') {
							hex = (hex * 16) + 10 + (c - 'A');
						} else if (c >= 'a' && c <= 'f') {
							hex = (hex * 16) + 10 + (c - 'a');
						} else {
							int original = html_numeric_entity_deconvert(hex, convmap, mapsize);
							if (original != -1) {
								mbfl_filter_feed_char(original, decoder);
								if (c != ';') {
									mbfl_filter_feed_char(c, decoder);
								}
								goto continue_decoding_html_entity;
							} else {
								goto not_valid_html_entity;
							}
						}
					}

					/* Hit the end of the string while decoding a hexadecimal numeric entity */
					goto not_valid_html_entity;
				} else if (c >= '0' && c <= '9') {
					/* Start decoding decimal number */
					int decimal = c - '0', digits = 1;

					while (p < e) {
						c = *p++;
						if (c >= '0' && c <= '9') {
							if (digits++ >= 10) {
								goto not_valid_html_entity;
							}
							decimal = (decimal * 10) + (c - '0');
						} else {
							int original = html_numeric_entity_deconvert(decimal, convmap, mapsize);
							if (original != -1) {
								mbfl_filter_feed_char(original, decoder);
								if (c != ';') {
									mbfl_filter_feed_char(c, decoder);
								}
								goto continue_decoding_html_entity;
							} else {
								goto not_valid_html_entity;
							}
						}
					}

					/* Hit the end of the string while decoding a decimal numeric entity */
					goto not_valid_html_entity;
				}
			}

not_valid_html_entity:
			/* It wasn't a valid HTML numeric entity; just pass all the characters through */
			while (entity < p) {
				mbfl_filter_feed_char(*entity++, decoder);
			}
		} else {
			/* Not an HTML numeric entity, pass it through unmodified */
			mbfl_filter_feed_char(c, decoder);
		}
	}

	efree(wc_buffer);
	mbfl_convert_filter_flush(decoder);
	mbfl_memory_device_result(&device, result);
	mbfl_convert_filter_delete(decoder);
	return result;
}
