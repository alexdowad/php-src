/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "mbstring_singlebyte.h"
#include "libmbfl/mbfl/mbfl_ident.h"

#define CK(statement) do { if ((statement) < 0) return (-1); } while (0)

/* Helper for single-byte encodings which use a conversion table */
static int mbfl_conv_singlebyte_table(int c, mbfl_convert_filter *filter, int tbl_min, const unsigned short tbl[])
{
   if (c < tbl_min) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      int s = tbl[c - tbl_min];
      if (!s) {
         s = c | MBFL_WCSGROUP_THROUGH;
      }
      CK((*filter->output_function)(s, filter->data));
   }
   return c;
}

static int mbfl_conv_reverselookup_table(int c, mbfl_convert_filter *filter, int tbl_min, const unsigned short tbl[])
{
   if (c < tbl_min) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      for (int i = 0; i < 256 - tbl_min; i++) {
         if (c == tbl[i]) {
            CK((*filter->output_function)(i + tbl_min, filter->data));
            return c;
         }
      }
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static int mbfl_ident_singlebyte_table(int c, mbfl_identify_filter *filter, int tbl_min, const unsigned short tbl[])
{
   if (c >= tbl_min && !tbl[c - tbl_min]) {
      filter->flag = 1;
   }
   return c;
}

/* Initialize data structures for a single-byte encoding */
#define __DEF_SB(id, name, mime_name, aliases, ident_filter) \
   static int mbfl_filt_conv_##id##_wchar(int c, mbfl_convert_filter *filter); \
   static int mbfl_filt_conv_wchar_##id(int c, mbfl_convert_filter *filter); \
   const struct mbfl_identify_vtbl vtbl_identify_##id = { \
      mbfl_no_encoding_##id, \
      mbfl_filt_ident_common_ctor, \
      ident_filter \
   }; \
   static const struct mbfl_convert_vtbl vtbl_##id##_wchar = { \
      mbfl_no_encoding_##id, \
      mbfl_no_encoding_wchar, \
      mbfl_filt_conv_common_ctor, \
      NULL, \
      mbfl_filt_conv_##id##_wchar, \
      mbfl_filt_conv_common_flush, \
      NULL \
   }; \
   static const struct mbfl_convert_vtbl vtbl_wchar_##id = { \
      mbfl_no_encoding_wchar, \
      mbfl_no_encoding_##id, \
      mbfl_filt_conv_common_ctor, \
      NULL, \
      mbfl_filt_conv_wchar_##id, \
      mbfl_filt_conv_common_flush, \
      NULL \
   }; \
   const mbfl_encoding mbfl_encoding_##id = { \
      mbfl_no_encoding_##id, \
      name, \
      mime_name, \
      aliases, \
      NULL, \
      MBFL_ENCTYPE_SBCS, \
      &vtbl_##id##_wchar, \
      &vtbl_wchar_##id \
   }

#define DEF_SB(id, name, mime_name, aliases) \
   static int mbfl_filt_ident_##id(int c, mbfl_identify_filter *filter); \
   __DEF_SB(id, name, mime_name, aliases, mbfl_filt_ident_##id)

/* For single-byte encodings in which all 256 single-byte values are valid */
#define DEF_SB_ALLBYTES(id, name, mime_name, aliases) \
   __DEF_SB(id, name, mime_name, aliases, mbfl_filt_ident_true)

/* For single-byte encodings which use a conversion table */
#define DEF_SB_TBL(id, name, mime_name, aliases, tbl_min, tbl) \
   static int mbfl_filt_conv_##id##_wchar(int c, mbfl_convert_filter *filter) { \
      return mbfl_conv_singlebyte_table(c, filter, tbl_min, tbl); \
   } \
   static int mbfl_filt_conv_wchar_##id(int c, mbfl_convert_filter *filter) { \
      return mbfl_conv_reverselookup_table(c, filter, tbl_min, tbl); \
   } \
   static int mbfl_filt_ident_##id(int c, mbfl_identify_filter *filter) { \
      return mbfl_ident_singlebyte_table(c, filter, tbl_min, tbl); \
   } \
   DEF_SB(id, name, mime_name, aliases)

#define DEF_SB_TBL_ALLBYTES(id, name, mime_name, aliases, tbl_min, tbl) \
   static int mbfl_filt_conv_##id##_wchar(int c, mbfl_convert_filter *filter) { \
      return mbfl_conv_singlebyte_table(c, filter, tbl_min, tbl); \
   } \
   static int mbfl_filt_conv_wchar_##id(int c, mbfl_convert_filter *filter) { \
      return mbfl_conv_reverselookup_table(c, filter, tbl_min, tbl); \
   } \
   DEF_SB_ALLBYTES(id, name, mime_name, aliases)

/* The grand-daddy of them all: ASCII */
static const char *ascii_aliases[] = {"ANSI_X3.4-1968", "iso-ir-6", "ANSI_X3.4-1986", "ISO_646.irv:1991", "US-ASCII", "ISO646-US", "us", "IBM367", "IBM-367", "cp367", "csASCII", NULL};
DEF_SB(ascii, "ASCII", "US-ASCII", ascii_aliases);

static int mbfl_filt_conv_ascii_wchar(int c, mbfl_convert_filter *filter)
{
   if (c < 0x80) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static int mbfl_filt_conv_wchar_ascii(int c, mbfl_convert_filter *filter)
{
   if (c < 0x80) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static int mbfl_filt_ident_ascii(int c, mbfl_identify_filter *filter)
{
   if (c >= 0x80) {
      filter->flag = 1;
   }
   return c;
}

/* ISO-8859-X */

static const char *iso8859_1_aliases[] = {"ISO8859-1", "latin1", NULL};
DEF_SB_ALLBYTES(8859_1, "ISO-8859-1", "ISO-8859-1", iso8859_1_aliases);

static int mbfl_filt_conv_8859_1_wchar(int c, mbfl_convert_filter *filter)
{
   return (*filter->output_function)(c, filter->data);
}

static int mbfl_filt_conv_wchar_8859_1(int c, mbfl_convert_filter *filter)
{
   if (c < 0x100) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static const char *iso8859_2_aliases[] = {"ISO8859-2", "latin2", NULL};
static const unsigned short iso8859_2_ucs_table[] = {
   0X00A0, 0X0104, 0X02D8, 0X0141, 0X00A4, 0X013D, 0X015A, 0X00A7,
   0X00A8, 0X0160, 0X015E, 0X0164, 0X0179, 0X00AD, 0X017D, 0X017B,
   0X00B0, 0X0105, 0X02DB, 0X0142, 0X00B4, 0X013E, 0X015B, 0X02C7,
   0X00B8, 0X0161, 0X015F, 0X0165, 0X017A, 0X02DD, 0X017E, 0X017C,
   0X0154, 0X00C1, 0X00C2, 0X0102, 0X00C4, 0X0139, 0X0106, 0X00C7,
   0X010C, 0X00C9, 0X0118, 0X00CB, 0X011A, 0X00CD, 0X00CE, 0X010E,
   0X0110, 0X0143, 0X0147, 0X00D3, 0X00D4, 0X0150, 0X00D6, 0X00D7,
   0X0158, 0X016E, 0X00DA, 0X0170, 0X00DC, 0X00DD, 0X0162, 0X00DF,
   0X0155, 0X00E1, 0X00E2, 0X0103, 0X00E4, 0X013A, 0X0107, 0X00E7,
   0X010D, 0X00E9, 0X0119, 0X00EB, 0X011B, 0X00ED, 0X00EE, 0X010F,
   0X0111, 0X0144, 0X0148, 0X00F3, 0X00F4, 0X0151, 0X00F6, 0X00F7,
   0X0159, 0X016F, 0X00FA, 0X0171, 0X00FC, 0X00FD, 0X0163, 0X02D9
};
DEF_SB_TBL_ALLBYTES(8859_2, "ISO-8859-2", "ISO-8859-2", iso8859_2_aliases, 0xA0, iso8859_2_ucs_table);

static const char *iso8859_3_aliases[] = {"ISO8859-3", "latin3", NULL};
static const unsigned short iso8859_3_ucs_table[] = {
   0X00A0, 0X0126, 0X02D8, 0X00A3, 0X00A4, 0X0000, 0X0124, 0X00A7,
   0X00A8, 0X0130, 0X015E, 0X011E, 0X0134, 0X00AD, 0X0000, 0X017B,
   0X00B0, 0X0127, 0X00B2, 0X00B3, 0X00B4, 0X00B5, 0X0125, 0X00B7,
   0X00B8, 0X0131, 0X015F, 0X011F, 0X0135, 0X00BD, 0X0000, 0X017C,
   0X00C0, 0X00C1, 0X00C2, 0X0000, 0X00C4, 0X010A, 0X0108, 0X00C7,
   0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
   0X0000, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X0120, 0X00D6, 0X00D7,
   0X011C, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X016C, 0X015C, 0X00DF,
   0X00E0, 0X00E1, 0X00E2, 0X0000, 0X00E4, 0X010B, 0X0109, 0X00E7,
   0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
   0X0000, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X0121, 0X00F6, 0X00F7,
   0X011D, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X016D, 0X015D, 0X02D9
};
DEF_SB_TBL(8859_3, "ISO-8859-3", "ISO-8859-3", iso8859_3_aliases, 0xA0, iso8859_3_ucs_table);

static const char *iso8859_4_aliases[] = {"ISO8859-4", "latin4", NULL};
static const unsigned short iso8859_4_ucs_table[] = {
   0X00A0, 0X0104, 0X0138, 0X0156, 0X00A4, 0X0128, 0X013B, 0X00A7,
   0X00A8, 0X0160, 0X0112, 0X0122, 0X0166, 0X00AD, 0X017D, 0X00AF,
   0X00B0, 0X0105, 0X02DB, 0X0157, 0X00B4, 0X0129, 0X013C, 0X02C7,
   0X00B8, 0X0161, 0X0113, 0X0123, 0X0167, 0X014A, 0X017E, 0X014B,
   0X0100, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X012E,
   0X010C, 0X00C9, 0X0118, 0X00CB, 0X0116, 0X00CD, 0X00CE, 0X012A,
   0X0110, 0X0145, 0X014C, 0X0136, 0X00D4, 0X00D5, 0X00D6, 0X00D7,
   0X00D8, 0X0172, 0X00DA, 0X00DB, 0X00DC, 0X0168, 0X016A, 0X00DF,
   0X0101, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X012F,
   0X010D, 0X00E9, 0X0119, 0X00EB, 0X0117, 0X00ED, 0X00EE, 0X012B,
   0X0111, 0X0146, 0X014D, 0X0137, 0X00F4, 0X00F5, 0X00F6, 0X00F7,
   0X00F8, 0X0173, 0X00FA, 0X00FB, 0X00FC, 0X0169, 0X016B, 0X02D9
};
DEF_SB_TBL_ALLBYTES(8859_4, "ISO-8859-4", "ISO-8859-4", iso8859_4_aliases, 0xA0, iso8859_4_ucs_table);

static const char *iso8859_5_aliases[] = {"ISO8859-5", "cyrillic", NULL};
static const unsigned short iso8859_5_ucs_table[] = {
   0x00a0, 0x0401, 0x0402, 0x0403, 0x0404, 0x0405, 0x0406, 0x0407,
   0x0408, 0x0409, 0x040a, 0x040b, 0x040c, 0x00ad, 0x040e, 0x040f,
   0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
   0x0418, 0x0419, 0x041a, 0x041b, 0x041c, 0x041d, 0x041e, 0x041f,
   0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
   0x0428, 0x0429, 0x042a, 0x042b, 0x042c, 0x042d, 0x042e, 0x042f,
   0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
   0x0438, 0x0439, 0x043a, 0x043b, 0x043c, 0x043d, 0x043e, 0x043f,
   0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
   0x0448, 0x0449, 0x044a, 0x044b, 0x044c, 0x044d, 0x044e, 0x044f,
   0x2116, 0x0451, 0x0452, 0x0453, 0x0454, 0x0455, 0x0456, 0x0457,
   0x0458, 0x0459, 0x045a, 0x045b, 0x045c, 0x00a7, 0x045e, 0x045f
};
DEF_SB_TBL_ALLBYTES(8859_5, "ISO-8859-5", "ISO-8859-5", iso8859_5_aliases, 0xA0, iso8859_5_ucs_table);

static const char *iso8859_6_aliases[] = {"ISO8859-6", "arabic", NULL};
static const unsigned short iso8859_6_ucs_table[] = {
   0X00A0, 0X0000, 0X0000, 0X0000, 0X00A4, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X060C, 0X00AD, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X061B, 0X0000, 0X0000, 0X0000, 0X061F,
   0X0000, 0X0621, 0X0622, 0X0623, 0X0624, 0X0625, 0X0626, 0X0627,
   0X0628, 0X0629, 0X062A, 0X062B, 0X062C, 0X062D, 0X062E, 0X062F,
   0X0630, 0X0631, 0X0632, 0X0633, 0X0634, 0X0635, 0X0636, 0X0637,
   0X0638, 0X0639, 0X063A, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0640, 0X0641, 0X0642, 0X0643, 0X0644, 0X0645, 0X0646, 0X0647,
   0X0648, 0X0649, 0X064A, 0X064B, 0X064C, 0X064D, 0X064E, 0X064F,
   0X0650, 0X0651, 0X0652, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000
};
DEF_SB_TBL(8859_6, "ISO-8859-6", "ISO-8859-6", iso8859_6_aliases, 0xA0, iso8859_6_ucs_table);

static const char *iso8859_7_aliases[] = {"ISO8859-7", "greek", NULL};
static const unsigned short iso8859_7_ucs_table[] = {
   0X00A0, 0X2018, 0X2019, 0X00A3, 0X20AC, 0X20AF, 0X00A6, 0X00A7,
   0X00A8, 0X00A9, 0X037A, 0X00AB, 0X00AC, 0X00AD, 0X0000, 0X2015,
   0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X0384, 0X0385, 0X0386, 0X00B7,
   0X0388, 0X0389, 0X038A, 0X00BB, 0X038C, 0X00BD, 0X038E, 0X038F,
   0X0390, 0X0391, 0X0392, 0X0393, 0X0394, 0X0395, 0X0396, 0X0397,
   0X0398, 0X0399, 0X039A, 0X039B, 0X039C, 0X039D, 0X039E, 0X039F,
   0X03A0, 0X03A1, 0X0000, 0X03A3, 0X03A4, 0X03A5, 0X03A6, 0X03A7,
   0X03A8, 0X03A9, 0X03AA, 0X03AB, 0X03AC, 0X03AD, 0X03AE, 0X03AF,
   0X03B0, 0X03B1, 0X03B2, 0X03B3, 0X03B4, 0X03B5, 0X03B6, 0X03B7,
   0X03B8, 0X03B9, 0X03BA, 0X03BB, 0X03BC, 0X03BD, 0X03BE, 0X03BF,
   0X03C0, 0X03C1, 0X03C2, 0X03C3, 0X03C4, 0X03C5, 0X03C6, 0X03C7,
   0X03C8, 0X03C9, 0X03CA, 0X03CB, 0X03CC, 0X03CD, 0X03CE, 0X0000
};
DEF_SB_TBL(8859_7, "ISO-8859-7", "ISO-8859-7", iso8859_7_aliases, 0xA0, iso8859_7_ucs_table);

static const char *iso8859_8_aliases[] = {"ISO8859-8", "hebrew", NULL};
static const unsigned short iso8859_8_ucs_table[] = {
   0X00A0, 0X0000, 0X00A2, 0X00A3, 0X00A4, 0X00A5, 0X00A6, 0X00A7,
   0X00A8, 0X00A9, 0X00D7, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00AF,
   0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X00B4, 0X00B5, 0X00B6, 0X00B7,
   0X00B8, 0X00B9, 0X00F7, 0X00BB, 0X00BC, 0X00BD, 0X00BE, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000,
   0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X0000, 0X2017,
   0X05D0, 0X05D1, 0X05D2, 0X05D3, 0X05D4, 0X05D5, 0X05D6, 0X05D7,
   0X05D8, 0X05D9, 0X05DA, 0X05DB, 0X05DC, 0X05DD, 0X05DE, 0X05DF,
   0X05E0, 0X05E1, 0X05E2, 0X05E3, 0X05E4, 0X05E5, 0X05E6, 0X05E7,
   0X05E8, 0X05E9, 0X05EA, 0X0000, 0X0000, 0X200E, 0X200F, 0X0000
};
DEF_SB_TBL(8859_8, "ISO-8859-8", "ISO-8859-8", iso8859_8_aliases, 0xA0, iso8859_8_ucs_table);

static const char *iso8859_9_aliases[] = {"ISO8859-9", "latin5", NULL};
static const unsigned short iso8859_9_ucs_table[] = {
   0X00A0, 0X00A1, 0X00A2, 0X00A3, 0X00A4, 0X00A5, 0X00A6, 0X00A7,
   0X00A8, 0X00A9, 0X00AA, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00AF,
   0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X00B4, 0X00B5, 0X00B6, 0X00B7,
   0X00B8, 0X00B9, 0X00BA, 0X00BB, 0X00BC, 0X00BD, 0X00BE, 0X00BF,
   0X00C0, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X00C7,
   0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
   0X011E, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X00D7,
   0X00D8, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X0130, 0X015E, 0X00DF,
   0X00E0, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X00E7,
   0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
   0X011F, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X00F7,
   0X00F8, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X0131, 0X015F, 0X00FF
};
DEF_SB_TBL_ALLBYTES(8859_9, "ISO-8859-9", "ISO-8859-9", iso8859_9_aliases, 0xA0, iso8859_9_ucs_table);

static const char *iso8859_10_aliases[] = {"ISO8859-10", "latin6", NULL};
static const unsigned short iso8859_10_ucs_table[] = {
   0X00A0, 0X0104, 0X0112, 0X0122, 0X012A, 0X0128, 0X0136, 0X00A7,
   0X013B, 0X0110, 0X0160, 0X0166, 0X017D, 0X00AD, 0X016A, 0X014A,
   0X00B0, 0X0105, 0X0113, 0X0123, 0X012B, 0X0129, 0X0137, 0X00B7,
   0X013C, 0X0111, 0X0161, 0X0167, 0X017E, 0X2015, 0X016B, 0X014B,
   0X0100, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X012E,
   0X010C, 0X00C9, 0X0118, 0X00CB, 0X0116, 0X00CD, 0X00CE, 0X00CF,
   0X00D0, 0X0145, 0X014C, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X0168,
   0X00D8, 0X0172, 0X00DA, 0X00DB, 0X00DC, 0X00DD, 0X00DE, 0X00DF,
   0X0101, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X012F,
   0X010D, 0X00E9, 0X0119, 0X00EB, 0X0117, 0X00ED, 0X00EE, 0X00EF,
   0X00F0, 0X0146, 0X014D, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X0169,
   0X00F8, 0X0173, 0X00FA, 0X00FB, 0X00FC, 0X00FD, 0X00FE, 0X0138
};
DEF_SB_TBL_ALLBYTES(8859_10, "ISO-8859-10", "ISO-8859-10", iso8859_10_aliases, 0xA0, iso8859_10_ucs_table);

static const char *iso8859_13_aliases[] = {"ISO8859-13", NULL};
static const unsigned short iso8859_13_ucs_table[] = {
   0X00A0, 0X201D, 0X00A2, 0X00A3, 0X00A4, 0X201E, 0X00A6, 0X00A7,
   0X00D8, 0X00A9, 0X0156, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00C6,
   0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X201C, 0X00B5, 0X00B6, 0X00B7,
   0X00F8, 0X00B9, 0X0157, 0X00BB, 0X00BC, 0X00BD, 0X00BE, 0X00E6,
   0X0104, 0X012E, 0X0100, 0X0106, 0X00C4, 0X00C5, 0X0118, 0X0112,
   0X010C, 0X00C9, 0X0179, 0X0116, 0X0122, 0X0136, 0X012A, 0X013B,
   0X0160, 0X0143, 0X0145, 0X00D3, 0X014C, 0X00D5, 0X00D6, 0X00D7,
   0X0172, 0X0141, 0X015A, 0X016A, 0X00DC, 0X017B, 0X017D, 0X00DF,
   0X0105, 0X012F, 0X0101, 0X0107, 0X00E4, 0X00E5, 0X0119, 0X0113,
   0X010D, 0X00E9, 0X017A, 0X0117, 0X0123, 0X0137, 0X012B, 0X013C,
   0X0161, 0X0144, 0X0146, 0X00F3, 0X014D, 0X00F5, 0X00F6, 0X00F7,
   0X0173, 0X0142, 0X015B, 0X016B, 0X00FC, 0X017C, 0X017E, 0X2019
};
DEF_SB_TBL_ALLBYTES(8859_13, "ISO-8859-13", "ISO-8859-13", iso8859_13_aliases, 0xA0, iso8859_13_ucs_table);

static const char *iso8859_14_aliases[] = {"ISO8859-14", "latin8", NULL};
static const unsigned short iso8859_14_ucs_table[] = {
   0X00A0, 0X1E02, 0X1E03, 0X00A3, 0X010A, 0X010B, 0X1E0A, 0X00A7,
   0X1E80, 0X00A9, 0X1E82, 0X1E0B, 0X1EF2, 0X00AD, 0X00AE, 0X0178,
   0X1E1E, 0X1E1F, 0X0120, 0X0121, 0X1E40, 0X1E41, 0X00B6, 0X1E56,
   0X1E81, 0X1E57, 0X1E83, 0X1E60, 0X1EF3, 0X1E84, 0X1E85, 0X1E61,
   0X00C0, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X00C7,
   0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
   0X0174, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X1E6A,
   0X00D8, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X00DD, 0X0176, 0X00DF,
   0X00E0, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X00E7,
   0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
   0X0175, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X1E6B,
   0X00F8, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X00FD, 0X0177, 0X00FF
};
DEF_SB_TBL_ALLBYTES(8859_14, "ISO-8859-14", "ISO-8859-14", iso8859_14_aliases, 0xA0, iso8859_14_ucs_table);

static const char *iso8859_15_aliases[] = {"ISO8859-15", NULL};
static const unsigned short iso8859_15_ucs_table[] = {
   0X00A0, 0X00A1, 0X00A2, 0X00A3, 0X20AC, 0X00A5, 0X0160, 0X00A7,
   0X0161, 0X00A9, 0X00AA, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00AF,
   0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X017D, 0X00B5, 0X00B6, 0X00B7,
   0X017E, 0X00B9, 0X00BA, 0X00BB, 0X0152, 0X0153, 0X0178, 0X00BF,
   0X00C0, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X00C7,
   0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
   0X00D0, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X00D7,
   0X00D8, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X00DD, 0X00DE, 0X00DF,
   0X00E0, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X00E7,
   0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
   0X00F0, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X00F7,
   0X00F8, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X00FD, 0X00FE, 0X00FF
};
DEF_SB_TBL_ALLBYTES(8859_15, "ISO-8859-15", "ISO-8859-15", iso8859_15_aliases, 0xA0, iso8859_15_ucs_table);

static const char *iso8859_16_aliases[] = {"ISO8859-16", NULL};
static const unsigned short iso8859_16_ucs_table[] = {
   0X00A0, 0X0104, 0X0105, 0X0141, 0X20AC, 0X201E, 0X0160, 0X00A7,
   0X0161, 0X00A9, 0X0218, 0X00AB, 0X0179, 0X00AD, 0X017A, 0X017B,
   0X00B0, 0X00B1, 0X010C, 0X0142, 0X017D, 0X201D, 0X00B6, 0X00B7,
   0X017E, 0X010D, 0X0219, 0X00BB, 0X0152, 0X0153, 0X0178, 0X017C,
   0X00C0, 0X00C1, 0X00C2, 0X0102, 0X00C4, 0X0106, 0X00C6, 0X00C7,
   0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
   0X0110, 0X0143, 0X00D2, 0X00D3, 0X00D4, 0X0150, 0X00D6, 0X015A,
   0X0170, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X0118, 0X021A, 0X00DF,
   0X00E0, 0X00E1, 0X00E2, 0X0103, 0X00E4, 0X0107, 0X00E6, 0X00E7,
   0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
   0X0111, 0X0144, 0X00F2, 0X00F3, 0X00F4, 0X0151, 0X00F6, 0X015B,
   0X0171, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X0119, 0X021B, 0X00FF
};
DEF_SB_TBL_ALLBYTES(8859_16, "ISO-8859-16", "ISO-8859-16", iso8859_16_aliases, 0xA0, iso8859_16_ucs_table);

static const char *cp1251_aliases[] = {"CP1251", "CP-1251", "WINDOWS-1251", NULL};
static const unsigned short cp1251_ucs_table[] = {
 0X0402, 0X0403, 0X201A, 0X0453, 0X201E, 0X2026, 0X2020, 0X2021,
 0X20AC, 0X2030, 0X0409, 0X2039, 0X040A, 0X040C, 0X040B, 0X040F,
 0X0452, 0X2018, 0X2019, 0X201C, 0X201D, 0X2022, 0X2013, 0X2014,
 0X0000, 0X2122, 0X0459, 0X203A, 0X045A, 0X045C, 0X045B, 0X045F,
 0X00A0, 0X040E, 0X045E, 0X0408, 0X00A4, 0X0490, 0X00A6, 0X00A7,
 0X0401, 0X00A9, 0X0404, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X0407,
 0X00B0, 0X00B1, 0X0406, 0X0456, 0X0491, 0X00B5, 0X00B6, 0X00B7,
 0X0451, 0X2116, 0X0454, 0X00BB, 0X0458, 0X0405, 0X0455, 0X0457,
 0X0410, 0X0411, 0X0412, 0X0413, 0X0414, 0X0415, 0X0416, 0X0417,
 0X0418, 0X0419, 0X041A, 0X041B, 0X041C, 0X041D, 0X041E, 0X041F,
 0X0420, 0X0421, 0X0422, 0X0423, 0X0424, 0X0425, 0X0426, 0X0427,
 0X0428, 0X0429, 0X042A, 0X042B, 0X042C, 0X042D, 0X042E, 0X042F,
 0X0430, 0X0431, 0X0432, 0X0433, 0X0434, 0X0435, 0X0436, 0X0437,
 0X0438, 0X0439, 0X043A, 0X043B, 0X043C, 0X043D, 0X043E, 0X043F,
 0X0440, 0X0441, 0X0442, 0X0443, 0X0444, 0X0445, 0X0446, 0X0447,
 0X0448, 0X0449, 0X044A, 0X044B, 0X044C, 0X044D, 0X044E, 0X044F
};
DEF_SB_TBL(cp1251, "Windows-1251", "Windows-1251", cp1251_aliases, 0x80, cp1251_ucs_table);

static const char *cp1252_aliases[] = {"cp1252", NULL};
static const unsigned short cp1252_ucs_table[] = {
 0X20AC,0X0000,0X201A,0X0192,0X201E,0X2026,0X2020,0X2021,
 0X02C6,0X2030,0X0160,0X2039,0X0152,0X0000,0X017D,0X0000,
 0X0000,0X2018,0X2019,0X201C,0X201D,0X2022,0X2013,0X2014,
 0X02DC,0X2122,0X0161,0X203A,0X0153,0X0000,0X017E,0X0178
};
DEF_SB(cp1252, "Windows-1252", "Windows-1252", cp1252_aliases);

static int mbfl_filt_conv_wchar_cp1252(int c, mbfl_convert_filter *filter)
{
   if (c >= 0x100)   {
      for (int n = 0; n < 32; n++) {
         if (c == cp1252_ucs_table[n]) {
            CK((*filter->output_function)(0x80 + n, filter->data));
            return c;
         }
      }
      CK(mbfl_filt_conv_illegal_output(c, filter));
   } else if (c <= 0x7F || c >= 0xA0) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static int mbfl_filt_conv_cp1252_wchar(int c, mbfl_convert_filter *filter)
{
   int s;

   if (c >= 0x80 && c < 0xA0) {
      s = cp1252_ucs_table[c - 0x80];
      if (!s) {
         s = c | MBFL_WCSGROUP_THROUGH;
      }
   } else {
      s = c;
   }

   CK((*filter->output_function)(s, filter->data));
   return c;
}

static int mbfl_filt_ident_cp1252(int c, mbfl_identify_filter *filter)
{
   if (c >= 0x80 && c < 0xA0 && !cp1252_ucs_table[c - 0x80]) {
      filter->flag = 1;
   }
   return c;
}

static const char *cp1254_aliases[] = {"CP1254", "CP-1254", "WINDOWS-1254", NULL};
static const unsigned short cp1254_ucs_table[] = {
 0X20AC, 0X0000, 0X201A, 0X0192, 0X201E, 0X2026, 0X2020, 0X2021,
 0X02C6, 0X2030, 0X0160, 0X2039, 0X0152, 0X0000, 0X0000, 0X0000,
 0X0000, 0X2018, 0X2019, 0X201C, 0X201D, 0X2022, 0X2013, 0X2014,
 0X02DC, 0X2122, 0X0161, 0X203A, 0X0153, 0X0000, 0X0000, 0X0178,
 0X00A0, 0X00A1, 0X00A2, 0X00A3, 0X00A4, 0X00A5, 0X00A6, 0X00A7,
 0X00A8, 0X00A9, 0X00AA, 0X00AB, 0X00AC, 0X00AD, 0X00AE, 0X00AF,
 0X00B0, 0X00B1, 0X00B2, 0X00B3, 0X00B4, 0X00B5, 0X00B6, 0X00B7,
 0X00B8, 0X00B9, 0X00BA, 0X00BB, 0X00BC, 0X00BD, 0X00BE, 0X00BF,
 0X00C0, 0X00C1, 0X00C2, 0X00C3, 0X00C4, 0X00C5, 0X00C6, 0X00C7,
 0X00C8, 0X00C9, 0X00CA, 0X00CB, 0X00CC, 0X00CD, 0X00CE, 0X00CF,
 0X011E, 0X00D1, 0X00D2, 0X00D3, 0X00D4, 0X00D5, 0X00D6, 0X00D7,
 0X00D8, 0X00D9, 0X00DA, 0X00DB, 0X00DC, 0X0130, 0X015E, 0X00DF,
 0X00E0, 0X00E1, 0X00E2, 0X00E3, 0X00E4, 0X00E5, 0X00E6, 0X00E7,
 0X00E8, 0X00E9, 0X00EA, 0X00EB, 0X00EC, 0X00ED, 0X00EE, 0X00EF,
 0X011F, 0X00F1, 0X00F2, 0X00F3, 0X00F4, 0X00F5, 0X00F6, 0X00F7,
 0X00F8, 0X00F9, 0X00FA, 0X00FB, 0X00FC, 0X0131, 0X015F, 0X00FF
};
DEF_SB_TBL(cp1254, "Windows-1254", "Windows-1254", cp1254_aliases, 0x80, cp1254_ucs_table);

static const char *cp866_aliases[] = {"CP-866", "IBM866", "IBM-866", NULL};
static const unsigned short cp866_ucs_table[] = {
 0X0410, 0X0411, 0X0412, 0X0413, 0X0414, 0X0415, 0X0416, 0X0417,
 0X0418, 0X0419, 0X041A, 0X041B, 0X041C, 0X041D, 0X041E, 0X041F,
 0X0420, 0X0421, 0X0422, 0X0423, 0X0424, 0X0425, 0X0426, 0X0427,
 0X0428, 0X0429, 0X042A, 0X042B, 0X042C, 0X042D, 0X042E, 0X042F,
 0X0430, 0X0431, 0X0432, 0X0433, 0X0434, 0X0435, 0X0436, 0X0437,
 0X0438, 0X0439, 0X043A, 0X043B, 0X043C, 0X043D, 0X043E, 0X043F,
 0X2591, 0X2592, 0X2593, 0X2502, 0X2524, 0X2561, 0X2562, 0X2556,
 0X2555, 0X2563, 0X2551, 0X2557, 0X255D, 0X255C, 0X255B, 0X2510,
 0X2514, 0X2534, 0X252C, 0X251C, 0X2500, 0X253C, 0X255E, 0X255F,
 0X255A, 0X2554, 0X2569, 0X2566, 0X2560, 0X2550, 0X256C, 0X2567,
 0X2568, 0X2564, 0X2565, 0X2559, 0X2558, 0X2552, 0X2553, 0X256B,
 0X256A, 0X2518, 0X250C, 0X2588, 0X2584, 0X258C, 0X2590, 0X2580,
 0X0440, 0X0441, 0X0442, 0X0443, 0X0444, 0X0445, 0X0446, 0X0447,
 0X0448, 0X0449, 0X044A, 0X044B, 0X044C, 0X044D, 0X044E, 0X044F,
 0X0401, 0X0451, 0X0404, 0X0454, 0X0407, 0X0457, 0X040E, 0X045E,
 0X00B0, 0X2219, 0X00B7, 0X221A, 0X2116, 0X00A4, 0X25A0, 0X00A0
};
DEF_SB_TBL_ALLBYTES(cp866, "CP866", "CP866", cp866_aliases, 0x80, cp866_ucs_table);

static const char *cp850_aliases[] = {"CP-850", "IBM850", "IBM-850", NULL};
static const unsigned short cp850_ucs_table[] = {
  0X00C7, 0X00FC, 0X00E9, 0X00E2, 0X00E4, 0X00E0, 0X00E5, 0X00E7,
  0X00EA, 0X00EB, 0X00E8, 0X00EF, 0X00EE, 0X00EC, 0X00C4, 0X00C5,
  0X00C9, 0X00E6, 0X00C6, 0X00F4, 0X00F6, 0X00F2, 0X00FB, 0X00F9,
  0X00FF, 0X00D6, 0X00DC, 0X00F8, 0X00A3, 0X00D8, 0X00D7, 0X0192,
  0X00E1, 0X00ED, 0X00F3, 0X00FA, 0X00F1, 0X00D1, 0X00AA, 0X00BA,
  0X00BF, 0X00AE, 0X00AC, 0X00BD, 0X00BC, 0X00A1, 0X00AB, 0X00BB,
  0X2591, 0X2592, 0X2593, 0X2502, 0X2524, 0X00C1, 0X00C2, 0X00C0,
  0X00A9, 0X2563, 0X2551, 0X2557, 0X255D, 0X00A2, 0X00A5, 0X2510,
  0X2514, 0X2534, 0X252C, 0X251C, 0X2500, 0X253C, 0X00E3, 0X00C3,
  0X255A, 0X2554, 0X2569, 0X2566, 0X2560, 0X2550, 0X256C, 0X00A4,
  0X00F0, 0X00D0, 0X00CA, 0X00CB, 0X00C8, 0X0131, 0X00CD, 0X00CE,
  0X00CF, 0X2518, 0X250C, 0X2588, 0X2584, 0X00A6, 0X00CC, 0X2580,
  0X00D3, 0X00DF, 0X00D4, 0X00D2, 0X00F5, 0X00D5, 0X00B5, 0X00FE,
  0X00DE, 0X00DA, 0X00DB, 0X00D9, 0X00FD, 0X00DD, 0X00AF, 0X00B4,
  0X00AD, 0X00B1, 0X2017, 0X00BE, 0X00B6, 0X00A7, 0X00F7, 0X00B8,
  0X00B0, 0X00A8, 0X00B7, 0X00B9, 0X00B3, 0X00B2, 0X25A0, 0X00A0
};
DEF_SB_TBL_ALLBYTES(cp850, "CP850", "CP850", cp850_aliases, 0x80, cp850_ucs_table);

static const char *koi8r_aliases[] = {"KOI8R", NULL};
static const unsigned short koi8r_ucs_table[] = {
 0X2500, 0X2502, 0X250C, 0X2510, 0X2514, 0X2518, 0X251C, 0X2524,
 0X252C, 0X2534, 0X253C, 0X2580, 0X2584, 0X2588, 0X258C, 0X2590,
 0X2591, 0X2592, 0X2593, 0X2320, 0X25A0, 0X2219, 0X221A, 0X2248,
 0X2264, 0X2265, 0X00A0, 0X2321, 0X00B0, 0X00B2, 0X00B7, 0X00F7,
 0X2550, 0X2551, 0X2552, 0X0451, 0X2553, 0X2554, 0X2555, 0X2556,
 0X2557, 0X2558, 0X2559, 0X255A, 0X255B, 0X255C, 0X255D, 0X255E,
 0X255F, 0X2560, 0X2561, 0X0401, 0X2562, 0X2563, 0X2564, 0X2565,
 0X2566, 0X2567, 0X2568, 0X2569, 0X256A, 0X256B, 0X256C, 0X00A9,
 0X044E, 0X0430, 0X0431, 0X0446, 0X0434, 0X0435, 0X0444, 0X0433,
 0X0445, 0X0438, 0X0439, 0X043A, 0X043B, 0X043C, 0X043D, 0X043E,
 0X043F, 0X044F, 0X0440, 0X0441, 0X0442, 0X0443, 0X0436, 0X0432,
 0X044C, 0X044B, 0X0437, 0X0448, 0X044D, 0X0449, 0X0447, 0X044A,
 0X042E, 0X0410, 0X0411, 0X0426, 0X0414, 0X0415, 0X0424, 0X0413,
 0X0425, 0X0418, 0X0419, 0X041A, 0X041B, 0X041C, 0X041D, 0X041E,
 0X041F, 0X042F, 0X0420, 0X0421, 0X0422, 0X0423, 0X0416, 0X0412,
 0X042C, 0X042B, 0X0417, 0X0428, 0X042D, 0X0429, 0X0427, 0X042A
};
DEF_SB_TBL_ALLBYTES(koi8r, "KOI8-R", "KOI8-R", koi8r_aliases, 0x80, koi8r_ucs_table);

static const char *koi8u_aliases[] = {"KOI8U", NULL};
static const unsigned short koi8u_ucs_table[] = {
 0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x251C, 0x2524,
 0x252C, 0x2534, 0x253C, 0x2580, 0x2584, 0x2588, 0x258C, 0x2590,
 0x2591, 0x2592, 0x2593, 0x2320, 0x25A0, 0x2219, 0x221A, 0x2248,
 0x2264, 0x2265, 0x00A0, 0x2321, 0x00B0, 0x00B2, 0x00B7, 0x00F7,
 0x2550, 0x2551, 0x2552, 0x0451, 0x0454, 0x2554, 0x0456, 0x0457,
 0x2557, 0x2558, 0x2559, 0x255A, 0x255B, 0x0491, 0x255D, 0x255E,
 0x255F, 0x2560, 0x2561, 0x0401, 0x0404, 0x2563, 0x0406, 0x0407,
 0x2566, 0x2567, 0x2568, 0x2569, 0x256A, 0x0490, 0x256C, 0x00A9,
 0x044E, 0x0430, 0x0431, 0x0446, 0x0434, 0x0435, 0x0444, 0x0433,
 0x0445, 0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E,
 0x043F, 0x044F, 0x0440, 0x0441, 0x0442, 0x0443, 0x0436, 0x0432,
 0x044C, 0x044B, 0x0437, 0x0448, 0x044D, 0x0449, 0x0447, 0x044A,
 0x042E, 0x0410, 0x0411, 0x0426, 0x0414, 0x0415, 0x0424, 0x0413,
 0x0425, 0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E,
 0x041F, 0x042F, 0x0420, 0x0421, 0x0422, 0x0423, 0x0416, 0x0412,
 0x042C, 0x042B, 0x0417, 0x0428, 0x042D, 0x0429, 0x0427, 0x042A
};
DEF_SB_TBL_ALLBYTES(koi8u, "KOI8-U", "KOI8-U", koi8u_aliases, 0x80, koi8u_ucs_table);

static const char *armscii8_aliases[] = {"ArmSCII8", "ARMSCII-8", "ARMSCII8", NULL};
static const unsigned short armscii8_ucs_table[] = {
   0X00A0, 0X0000, 0X0587, 0X0589, 0X0029, 0X0028, 0X00BB, 0X00AB,
   0X2014, 0X002E, 0X055D, 0X002C, 0X002D, 0X058A, 0X2026, 0X055C,
   0X055B, 0X055E, 0X0531, 0X0561, 0X0532, 0X0562, 0X0533, 0X0563,
   0X0534, 0X0564, 0X0535, 0X0565, 0X0536, 0X0566, 0X0537, 0X0567,
   0X0538, 0X0568, 0X0539, 0X0569, 0X053A, 0X056A, 0X053B, 0X056B,
   0X053C, 0X056C, 0X053D, 0X056D, 0X053E, 0X056E, 0X053F, 0X056F,
   0X0540, 0X0570, 0X0541, 0X0571, 0X0542, 0X0572, 0X0543, 0X0573,
   0X0544, 0X0574, 0X0545, 0X0575, 0X0546, 0X0576, 0X0547, 0X0577,
   0X0548, 0X0578, 0X0549, 0X0579, 0X054A, 0X057A, 0X054B, 0X057B,
   0X054C, 0X057C, 0X054D, 0X057D, 0X054E, 0X057E, 0X054F, 0X057F,
   0X0550, 0X0580, 0X0551, 0X0581, 0X0552, 0X0582, 0X0553, 0X0583,
   0X0554, 0X0584, 0X0555, 0X0585, 0X0556, 0X0586, 0X055A, 0X0000
};
static const unsigned char ucs_armscii8_table[] = {
   0XA5, 0XA4, 0X2A, 0X2B, 0XAB, 0XAC, 0XA9, 0X2F
};
DEF_SB(armscii8, "ArmSCII-8", "ArmSCII-8", armscii8_aliases);

int mbfl_filt_conv_armscii8_wchar(int c, mbfl_convert_filter *filter)
{
   int s;

   if (c < 0xA0) {
      s = c;
   } else {
      s = armscii8_ucs_table[c - 0xA0];
      if (!s) {
         s = c | MBFL_WCSGROUP_THROUGH;
      }
   }

   CK((*filter->output_function)(s, filter->data));
   return c;
}

int mbfl_filt_conv_wchar_armscii8(int c, mbfl_convert_filter *filter)
{
   if (c >= 0x28 && c <= 0x2F) {
      CK((*filter->output_function)(ucs_armscii8_table[c - 0x28], filter->data));
   } else if (c < 0xA0) {
      CK((*filter->output_function)(c, filter->data));
   } else {
      for (int n = 0; n < 0x60; n++) {
         if (c == armscii8_ucs_table[n]) {
            CK((*filter->output_function)(0xA0 + n, filter->data));
            return c;
         }
      }
      CK(mbfl_filt_conv_illegal_output(c, filter));
   }
   return c;
}

static int mbfl_filt_ident_armscii8(int c, mbfl_identify_filter *filter)
{
   if (c >= 0xA0 && !armscii8_ucs_table[c - 0xA0]) {
      filter->flag = 1;
   }
   return c;
}
