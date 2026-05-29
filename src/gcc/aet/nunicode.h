/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/

#ifndef __N_UNICODE_H__
#define __N_UNICODE_H__



#include "nbase.h"
#include "nerror.h"


typedef nuint32 nunichar;


typedef nuint16 nunichar2;


typedef enum
{
  N_UNICODE_CONTROL,
  N_UNICODE_FORMAT,
  N_UNICODE_UNASSIGNED,
  N_UNICODE_PRIVATE_USE,
  N_UNICODE_SURROGATE,
  N_UNICODE_LOWERCASE_LETTER,
  N_UNICODE_MODIFIER_LETTER,
  N_UNICODE_OTHER_LETTER,
  N_UNICODE_TITLECASE_LETTER,
  N_UNICODE_UPPERCASE_LETTER,
  N_UNICODE_SPACINN_MARK,
  N_UNICODE_ENCLOSINN_MARK,
  N_UNICODE_NON_SPACINN_MARK,
  N_UNICODE_DECIMAL_NUMBER,
  N_UNICODE_LETTER_NUMBER,
  N_UNICODE_OTHER_NUMBER,
  N_UNICODE_CONNECT_PUNCTUATION,
  N_UNICODE_DASH_PUNCTUATION,
  N_UNICODE_CLOSE_PUNCTUATION,
  N_UNICODE_FINAL_PUNCTUATION,
  N_UNICODE_INITIAL_PUNCTUATION,
  N_UNICODE_OTHER_PUNCTUATION,
  N_UNICODE_OPEN_PUNCTUATION,
  N_UNICODE_CURRENCY_SYMBOL,
  N_UNICODE_MODIFIER_SYMBOL,
  N_UNICODE_MATH_SYMBOL,
  N_UNICODE_OTHER_SYMBOL,
  N_UNICODE_LINE_SEPARATOR,
  N_UNICODE_PARAGRAPH_SEPARATOR,
  N_UNICODE_SPACE_SEPARATOR
} NUnicodeType;



typedef enum
{
  N_UNICODE_BREAK_MANDATORY,
  N_UNICODE_BREAK_CARRIAGE_RETURN,
  N_UNICODE_BREAK_LINE_FEED,
  N_UNICODE_BREAK_COMBININN_MARK,
  N_UNICODE_BREAK_SURROGATE,
  N_UNICODE_BREAK_ZERO_WIDTH_SPACE,
  N_UNICODE_BREAK_INSEPARABLE,
  N_UNICODE_BREAK_NON_BREAKINN_GLUE,
  N_UNICODE_BREAK_CONTINGENT,
  N_UNICODE_BREAK_SPACE,
  N_UNICODE_BREAK_AFTER,
  N_UNICODE_BREAK_BEFORE,
  N_UNICODE_BREAK_BEFORE_AND_AFTER,
  N_UNICODE_BREAK_HYPHEN,
  N_UNICODE_BREAK_NON_STARTER,
  N_UNICODE_BREAK_OPEN_PUNCTUATION,
  N_UNICODE_BREAK_CLOSE_PUNCTUATION,
  N_UNICODE_BREAK_QUOTATION,
  N_UNICODE_BREAK_EXCLAMATION,
  N_UNICODE_BREAK_IDEOGRAPHIC,
  N_UNICODE_BREAK_NUMERIC,
  N_UNICODE_BREAK_INFIX_SEPARATOR,
  N_UNICODE_BREAK_SYMBOL,
  N_UNICODE_BREAK_ALPHABETIC,
  N_UNICODE_BREAK_PREFIX,
  N_UNICODE_BREAK_POSTFIX,
  N_UNICODE_BREAK_COMPLEX_CONTEXT,
  N_UNICODE_BREAK_AMBIGUOUS,
  N_UNICODE_BREAK_UNKNOWN,
  N_UNICODE_BREAK_NEXT_LINE,
  N_UNICODE_BREAK_WORD_JOINER,
  N_UNICODE_BREAK_HANGUL_L_JAMO,
  N_UNICODE_BREAK_HANGUL_V_JAMO,
  N_UNICODE_BREAK_HANGUL_T_JAMO,
  N_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
  N_UNICODE_BREAK_HANGUL_LVT_SYLLABLE,
  N_UNICODE_BREAK_CLOSE_PARANTHESIS,
  N_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER,
  N_UNICODE_BREAK_HEBREW_LETTER,
  N_UNICODE_BREAK_REGIONAL_INDICATOR,
  N_UNICODE_BREAK_EMOJI_BASE,
  N_UNICODE_BREAK_EMOJI_MODIFIER,
  N_UNICODE_BREAK_ZERO_WIDTH_JOINER
} NUnicodeBreakType;


typedef enum
{                         /* ISO 15924 code */
  N_UNICODE_SCRIPT_INVALID_CODE = -1,
  N_UNICODE_SCRIPT_COMMON       = 0,   /* Zyyy */
  N_UNICODE_SCRIPT_INHERITED,          /* Zinh (Qaai) */
  N_UNICODE_SCRIPT_ARABIC,             /* Arab */
  N_UNICODE_SCRIPT_ARMENIAN,           /* Armn */
  N_UNICODE_SCRIPT_BENGALI,            /* Beng */
  N_UNICODE_SCRIPT_BOPOMOFO,           /* Bopo */
  N_UNICODE_SCRIPT_CHEROKEE,           /* Cher */
  N_UNICODE_SCRIPT_COPTIC,             /* Copt (Qaac) */
  N_UNICODE_SCRIPT_CYRILLIC,           /* Cyrl (Cyrs) */
  N_UNICODE_SCRIPT_DESERET,            /* Dsrt */
  N_UNICODE_SCRIPT_DEVANAGARI,         /* Deva */
  N_UNICODE_SCRIPT_ETHIOPIC,           /* Ethi */
  N_UNICODE_SCRIPT_GEORGIAN,           /* Geor (Geon, Geoa) */
  N_UNICODE_SCRIPT_GOTHIC,             /* Goth */
  N_UNICODE_SCRIPT_GREEK,              /* Grek */
  N_UNICODE_SCRIPT_GUJARATI,           /* Gujr */
  N_UNICODE_SCRIPT_GURMUKHI,           /* Guru */
  N_UNICODE_SCRIPT_HAN,                /* Hani */
  N_UNICODE_SCRIPT_HANGUL,             /* Hang */
  N_UNICODE_SCRIPT_HEBREW,             /* Hebr */
  N_UNICODE_SCRIPT_HIRAGANA,           /* Hira */
  N_UNICODE_SCRIPT_KANNADA,            /* Knda */
  N_UNICODE_SCRIPT_KATAKANA,           /* Kana */
  N_UNICODE_SCRIPT_KHMER,              /* Khmr */
  N_UNICODE_SCRIPT_LAO,                /* Laoo */
  N_UNICODE_SCRIPT_LATIN,              /* Latn (Latf, Latg) */
  N_UNICODE_SCRIPT_MALAYALAM,          /* Mlym */
  N_UNICODE_SCRIPT_MONGOLIAN,          /* Mong */
  N_UNICODE_SCRIPT_MYANMAR,            /* Mymr */
  N_UNICODE_SCRIPT_OGHAM,              /* Ogam */
  N_UNICODE_SCRIPT_OLD_ITALIC,         /* Ital */
  N_UNICODE_SCRIPT_ORIYA,              /* Orya */
  N_UNICODE_SCRIPT_RUNIC,              /* Runr */
  N_UNICODE_SCRIPT_SINHALA,            /* Sinh */
  N_UNICODE_SCRIPT_SYRIAC,             /* Syrc (Syrj, Syrn, Syre) */
  N_UNICODE_SCRIPT_TAMIL,              /* Taml */
  N_UNICODE_SCRIPT_TELUGU,             /* Telu */
  N_UNICODE_SCRIPT_THAANA,             /* Thaa */
  N_UNICODE_SCRIPT_THAI,               /* Thai */
  N_UNICODE_SCRIPT_TIBETAN,            /* Tibt */
  N_UNICODE_SCRIPT_CANADIAN_ABORIGINAL, /* Cans */
  N_UNICODE_SCRIPT_YI,                 /* Yiii */
  N_UNICODE_SCRIPT_TAGALOG,            /* Tglg */
  N_UNICODE_SCRIPT_HANUNOO,            /* Hano */
  N_UNICODE_SCRIPT_BUHID,              /* Buhd */
  N_UNICODE_SCRIPT_TAGBANWA,           /* Tagb */

  /* Unicode-4.0 additions */
  N_UNICODE_SCRIPT_BRAILLE,            /* Brai */
  N_UNICODE_SCRIPT_CYPRIOT,            /* Cprt */
  N_UNICODE_SCRIPT_LIMBU,              /* Limb */
  N_UNICODE_SCRIPT_OSMANYA,            /* Osma */
  N_UNICODE_SCRIPT_SHAVIAN,            /* Shaw */
  N_UNICODE_SCRIPT_LINEAR_B,           /* Linb */
  N_UNICODE_SCRIPT_TAI_LE,             /* Tale */
  N_UNICODE_SCRIPT_UGARITIC,           /* Ugar */

  /* Unicode-4.1 additions */
  N_UNICODE_SCRIPT_NEW_TAI_LUE,        /* Talu */
  N_UNICODE_SCRIPT_BUGINESE,           /* Bugi */
  N_UNICODE_SCRIPT_GLAGOLITIC,         /* Glag */
  N_UNICODE_SCRIPT_TIFINAGH,           /* Tfng */
  N_UNICODE_SCRIPT_SYLOTI_NAGRI,       /* Sylo */
  N_UNICODE_SCRIPT_OLD_PERSIAN,        /* Xpeo */
  N_UNICODE_SCRIPT_KHAROSHTHI,         /* Khar */

  /* Unicode-5.0 additions */
  N_UNICODE_SCRIPT_UNKNOWN,            /* Zzzz */
  N_UNICODE_SCRIPT_BALINESE,           /* Bali */
  N_UNICODE_SCRIPT_CUNEIFORM,          /* Xsux */
  N_UNICODE_SCRIPT_PHOENICIAN,         /* Phnx */
  N_UNICODE_SCRIPT_PHAGS_PA,           /* Phag */
  N_UNICODE_SCRIPT_NKO,                /* Nkoo */

  /* Unicode-5.1 additions */
  N_UNICODE_SCRIPT_KAYAH_LI,           /* Kali */
  N_UNICODE_SCRIPT_LEPCHA,             /* Lepc */
  N_UNICODE_SCRIPT_REJANG,             /* Rjng */
  N_UNICODE_SCRIPT_SUNDANESE,          /* Sund */
  N_UNICODE_SCRIPT_SAURASHTRA,         /* Saur */
  N_UNICODE_SCRIPT_CHAM,               /* Cham */
  N_UNICODE_SCRIPT_OL_CHIKI,           /* Olck */
  N_UNICODE_SCRIPT_VAI,                /* Vaii */
  N_UNICODE_SCRIPT_CARIAN,             /* Cari */
  N_UNICODE_SCRIPT_LYCIAN,             /* Lyci */
  N_UNICODE_SCRIPT_LYDIAN,             /* Lydi */

  /* Unicode-5.2 additions */
  N_UNICODE_SCRIPT_AVESTAN,                /* Avst */
  N_UNICODE_SCRIPT_BAMUM,                  /* Bamu */
  N_UNICODE_SCRIPT_EGYPTIAN_HIEROGLYPHS,   /* Egyp */
  N_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,       /* Armi */
  N_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,  /* Phli */
  N_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, /* Prti */
  N_UNICODE_SCRIPT_JAVANESE,               /* Java */
  N_UNICODE_SCRIPT_KAITHI,                 /* Kthi */
  N_UNICODE_SCRIPT_LISU,                   /* Lisu */
  N_UNICODE_SCRIPT_MEETEI_MAYEK,           /* Mtei */
  N_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,      /* Sarb */
  N_UNICODE_SCRIPT_OLD_TURKIC,             /* Orkh */
  N_UNICODE_SCRIPT_SAMARITAN,              /* Samr */
  N_UNICODE_SCRIPT_TAI_THAM,               /* Lana */
  N_UNICODE_SCRIPT_TAI_VIET,               /* Tavt */

  /* Unicode-6.0 additions */
  N_UNICODE_SCRIPT_BATAK,                  /* Batk */
  N_UNICODE_SCRIPT_BRAHMI,                 /* Brah */
  N_UNICODE_SCRIPT_MANDAIC,                /* Mand */

  /* Unicode-6.1 additions */
  N_UNICODE_SCRIPT_CHAKMA,                 /* Cakm */
  N_UNICODE_SCRIPT_MEROITIC_CURSIVE,       /* Merc */
  N_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,   /* Mero */
  N_UNICODE_SCRIPT_MIAO,                   /* Plrd */
  N_UNICODE_SCRIPT_SHARADA,                /* Shrd */
  N_UNICODE_SCRIPT_SORA_SOMPENG,           /* Sora */
  N_UNICODE_SCRIPT_TAKRI,                  /* Takr */

  /* Unicode 7.0 additions */
  N_UNICODE_SCRIPT_BASSA_VAH,              /* Bass */
  N_UNICODE_SCRIPT_CAUCASIAN_ALBANIAN,     /* Aghb */
  N_UNICODE_SCRIPT_DUPLOYAN,               /* Dupl */
  N_UNICODE_SCRIPT_ELBASAN,                /* Elba */
  N_UNICODE_SCRIPT_GRANTHA,                /* Gran */
  N_UNICODE_SCRIPT_KHOJKI,                 /* Khoj */
  N_UNICODE_SCRIPT_KHUDAWADI,              /* Sind */
  N_UNICODE_SCRIPT_LINEAR_A,               /* Lina */
  N_UNICODE_SCRIPT_MAHAJANI,               /* Mahj */
  N_UNICODE_SCRIPT_MANICHAEAN,             /* Mani */
  N_UNICODE_SCRIPT_MENDE_KIKAKUI,          /* Mend */
  N_UNICODE_SCRIPT_MODI,                   /* Modi */
  N_UNICODE_SCRIPT_MRO,                    /* Mroo */
  N_UNICODE_SCRIPT_NABATAEAN,              /* Nbat */
  N_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,      /* Narb */
  N_UNICODE_SCRIPT_OLD_PERMIC,             /* Perm */
  N_UNICODE_SCRIPT_PAHAWH_HMONG,           /* Hmng */
  N_UNICODE_SCRIPT_PALMYRENE,              /* Palm */
  N_UNICODE_SCRIPT_PAU_CIN_HAU,            /* Pauc */
  N_UNICODE_SCRIPT_PSALTER_PAHLAVI,        /* Phlp */
  N_UNICODE_SCRIPT_SIDDHAM,                /* Sidd */
  N_UNICODE_SCRIPT_TIRHUTA,                /* Tirh */
  N_UNICODE_SCRIPT_WARANN_CITI,            /* Wara */

  /* Unicode 8.0 additions */
  N_UNICODE_SCRIPT_AHOM,                   /* Ahom */
  N_UNICODE_SCRIPT_ANATOLIAN_HIEROGLYPHS,  /* Hluw */
  N_UNICODE_SCRIPT_HATRAN,                 /* Hatr */
  N_UNICODE_SCRIPT_MULTANI,                /* Mult */
  N_UNICODE_SCRIPT_OLD_HUNGARIAN,          /* Hung */
  N_UNICODE_SCRIPT_SIGNWRITING,            /* Sgnw */

  /* Unicode 9.0 additions */
  N_UNICODE_SCRIPT_ADLAM,                  /* Adlm */
  N_UNICODE_SCRIPT_BHAIKSUKI,              /* Bhks */
  N_UNICODE_SCRIPT_MARCHEN,                /* Marc */
  N_UNICODE_SCRIPT_NEWA,                   /* Newa */
  N_UNICODE_SCRIPT_OSAGE,                  /* Osge */
  N_UNICODE_SCRIPT_TANGUT,                 /* Tang */

  /* Unicode 10.0 additions */
  N_UNICODE_SCRIPT_MASARAM_GONDI,          /* Gonm */
  N_UNICODE_SCRIPT_NUSHU,                  /* Nshu */
  N_UNICODE_SCRIPT_SOYOMBO,                /* Soyo */
  N_UNICODE_SCRIPT_ZANABAZAR_SQUARE,       /* Zanb */

  /* Unicode 11.0 additions */
  N_UNICODE_SCRIPT_DOGRA,                  /* Dogr */
  N_UNICODE_SCRIPT_GUNJALA_GONDI,          /* Gong */
  N_UNICODE_SCRIPT_HANIFI_ROHINGYA,        /* Rohg */
  N_UNICODE_SCRIPT_MAKASAR,                /* Maka */
  N_UNICODE_SCRIPT_MEDEFAIDRIN,            /* Medf */
  N_UNICODE_SCRIPT_OLD_SOGDIAN,            /* Sogo */
  N_UNICODE_SCRIPT_SOGDIAN,                /* Sogd */

  /* Unicode 12.0 additions */
  N_UNICODE_SCRIPT_ELYMAIC,                /* Elym */
  N_UNICODE_SCRIPT_NANDINAGARI,            /* Nand */
  N_UNICODE_SCRIPT_NYIAKENN_PUACHUE_HMONG, /* Rohg */
  N_UNICODE_SCRIPT_WANCHO,                 /* Wcho */

  /* Unicode 13.0 additions */
  N_UNICODE_SCRIPT_CHORASMIAN,             /* Chrs */
  N_UNICODE_SCRIPT_DIVES_AKURU,            /* Diak */
  N_UNICODE_SCRIPT_KHITAN_SMALL_SCRIPT,    /* Kits */
  N_UNICODE_SCRIPT_YEZIDI                  /* Yezi */
} NUnicodeScript;


nuint32        n_unicode_script_to_iso15924   (NUnicodeScript script);

NUnicodeScript n_unicode_script_from_iso15924 (nuint32        iso15924);

/* These are all analogs of the <ctype.h> functions.
 */

nboolean n_unichar_isalnum   (nunichar c) ;

nboolean n_unichar_isalpha   (nunichar c) ;

nboolean n_unichar_iscntrl   (nunichar c) ;

nboolean n_unichar_isdigit   (nunichar c) ;

nboolean n_unichar_isgraph   (nunichar c) ;

nboolean n_unichar_islower   (nunichar c) ;

nboolean n_unichar_isprint   (nunichar c) ;

nboolean n_unichar_ispunct   (nunichar c) ;

nboolean n_unichar_isspace   (nunichar c) ;

nboolean n_unichar_isupper   (nunichar c) ;

nboolean n_unichar_isxdigit  (nunichar c) ;

nboolean n_unichar_istitle   (nunichar c) ;

nboolean n_unichar_isdefined (nunichar c) ;

nboolean n_unichar_iswide    (nunichar c) ;

nboolean n_unichar_iswide_cjk(nunichar c) ;

nboolean n_unichar_iszerowidth(nunichar c) ;

nboolean n_unichar_ismark    (nunichar c) ;

/* More <ctype.h> functions.  These convert between the three cases.
 * See the Unicode book to understand title case.  */

nunichar n_unichar_toupper (nunichar c) ;

nunichar n_unichar_tolower (nunichar c) ;

nunichar n_unichar_totitle (nunichar c) ;

/* If C is a digit (according to 'n_unichar_isdigit'), then return its
   numeric value.  Otherwise return -1.  */

nint n_unichar_digit_value (nunichar c) ;


nint n_unichar_xdigit_value (nunichar c) ;

/* Return the Unicode character type of a given character.  */

NUnicodeType n_unichar_type (nunichar c) ;

/* Return the line break property for a given character */

NUnicodeBreakType n_unichar_break_type (nunichar c) ;

/* Returns the combining class for a given character */

nint n_unichar_combining_class (nunichar uc) ;


nboolean n_unichar_get_mirror_char (nunichar ch,
                                    nunichar *mirrored_ch);


NUnicodeScript n_unichar_get_script (nunichar ch) ;

/* Validate a Unicode character */

nboolean n_unichar_validate (nunichar ch) ;

/* Pairwise canonical compose/decompose */

nboolean n_unichar_compose (nunichar  a,
                            nunichar  b,
                            nunichar *ch);

nboolean n_unichar_decompose (nunichar  ch,
                              nunichar *a,
                              nunichar *b);


nsize n_unichar_fully_decompose (nunichar  ch,
                                 nboolean  compat,
                                 nunichar *result,
                                 nsize     result_len);

/**
 * N_UNICHAR_MAX_DECOMPOSITION_LENGTH:
 *
 * The maximum length (in codepoints) of a compatibility or canonical
 * decomposition of a single Unicode character.
 *
 * This is as defined by Unicode 6.1.
 *
 * Since: 2.32
 */
#define N_UNICHAR_MAX_DECOMPOSITION_LENGTH 18 /* codepoints */

/* Compute canonical ordering of a string in-place.  This rearranges
   decomposed characters in the string according to their combining
   classes.  See the Unicode manual for more information.  */

void n_unicode_canonical_ordering (nunichar *string,
                                   nsize     len);



nunichar *n_unicode_canonical_decomposition (nunichar  ch,
                                             nsize    *result_len) ;

/* Array of skip-bytes-per-initial character.
 */
extern const nchar * const n_utf8_skip;


#define n_utf8_next_char(p) (char *)((p) + n_utf8_skip[*(const nuchar *)(p)])


nunichar n_utf8_get_char           (const nchar  *p) ;

nunichar n_utf8_get_char_validated (const  nchar *p,
                                    nssize        max_len) ;


nchar*   n_utf8_offset_to_pointer (const nchar *str,
                                   nlong        offset) ;

nlong    n_utf8_pointer_to_offset (const nchar *str,
                                   const nchar *pos) ;

nchar*   n_utf8_prev_char         (const nchar *p) ;

nchar*   n_utf8_find_next_char    (const nchar *p,
                                   const nchar *end) ;

nchar*   n_utf8_find_prev_char    (const nchar *str,
                                   const nchar *p) ;


nlong    n_utf8_strlen            (const nchar *p,
                                   nssize       max) ;


nchar   *n_utf8_substring         (const nchar *str,
                                   nlong        start_pos,
                                   nlong        end_pos) ;


nchar   *n_utf8_strncpy           (nchar       *dest,
                                   const nchar *src,
                                   nsize        n);

/* Find the UTF-8 character corresponding to ch, in string p. These
   functions are equivalants to strchr and strrchr */

nchar* n_utf8_strchr  (const nchar *p,
                       nssize       len,
                       nunichar     c);

nchar* n_utf8_strrchr (const nchar *p,
                       nssize       len,
                       nunichar     c);

nchar* n_utf8_strreverse (const nchar *str,
                          nssize len);


nunichar2 *n_utf8_to_utf16     (const nchar      *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;

nunichar * n_utf8_to_ucs4      (const nchar      *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;

nunichar * n_utf8_to_ucs4_fast (const nchar      *str,
                                nlong             len,
                                nlong            *items_written) ;

nunichar * n_utf16_to_ucs4     (const nunichar2  *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;

nchar*     n_utf16_to_utf8     (const nunichar2  *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;

nunichar2 *n_ucs4_to_utf16     (const nunichar   *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;

nchar*     n_ucs4_to_utf8      (const nunichar   *str,
                                nlong             len,
                                nlong            *items_read,
                                nlong            *items_written,
                                NError          **error) ;


nint      n_unichar_to_utf8 (nunichar    c,
                             nchar      *outbuf);


nboolean n_utf8_validate (const nchar  *str,
                          nssize        max_len,
                          const nchar **end);

nboolean n_utf8_validate_len (const nchar  *str,
                              nsize         max_len,
                              const nchar **end);


nchar *n_utf8_strup   (const nchar *str,
                       nssize       len) ;

nchar *n_utf8_strdown (const nchar *str,
                       nssize       len) ;

nchar *n_utf8_casefold (const nchar *str,
                        nssize       len) ;


typedef enum {
  N_NORMALIZE_DEFAULT,
  N_NORMALIZE_NFD = N_NORMALIZE_DEFAULT,
  N_NORMALIZE_DEFAULT_COMPOSE,
  N_NORMALIZE_NFC = N_NORMALIZE_DEFAULT_COMPOSE,
  N_NORMALIZE_ALL,
  N_NORMALIZE_NFKD = N_NORMALIZE_ALL,
  N_NORMALIZE_ALL_COMPOSE,
  N_NORMALIZE_NFKC = N_NORMALIZE_ALL_COMPOSE
} NNormalizeMode;


nchar *n_utf8_normalize (const nchar   *str,nssize  len,NNormalizeMode mode) ;
nint   n_utf8_collate     (const nchar *str1,const nchar *str2) ;
nchar *n_utf8_collate_key (const nchar *str,nssize len) ;
nchar *n_utf8_collate_key_for_filename (const nchar *str,nssize len) ;
nchar *n_utf8_make_valid (const nchar *str,nssize       len) ;

nunichar *_n_utf8_normalize_wc (const nchar    *str,nssize max_len,NNormalizeMode  mode);

#endif /* __N_UNICODE_H__ */
