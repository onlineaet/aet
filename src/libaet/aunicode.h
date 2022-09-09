

#ifndef __A_UNICODE_H__
#define __A_UNICODE_H__



#include "abase.h"
#include "aerror.h"


typedef auint32 aunichar;


typedef auint16 aunichar2;


typedef enum
{
  A_UNICODE_CONTROL,
  A_UNICODE_FORMAT,
  A_UNICODE_UNASSIGNED,
  A_UNICODE_PRIVATE_USE,
  A_UNICODE_SURROGATE,
  A_UNICODE_LOWERCASE_LETTER,
  A_UNICODE_MODIFIER_LETTER,
  A_UNICODE_OTHER_LETTER,
  A_UNICODE_TITLECASE_LETTER,
  A_UNICODE_UPPERCASE_LETTER,
  A_UNICODE_SPACING_MARK,
  A_UNICODE_ENCLOSING_MARK,
  A_UNICODE_NON_SPACING_MARK,
  A_UNICODE_DECIMAL_NUMBER,
  A_UNICODE_LETTER_NUMBER,
  A_UNICODE_OTHER_NUMBER,
  A_UNICODE_CONNECT_PUNCTUATION,
  A_UNICODE_DASH_PUNCTUATION,
  A_UNICODE_CLOSE_PUNCTUATION,
  A_UNICODE_FINAL_PUNCTUATION,
  A_UNICODE_INITIAL_PUNCTUATION,
  A_UNICODE_OTHER_PUNCTUATION,
  A_UNICODE_OPEA_PUNCTUATION,
  A_UNICODE_CURRENCY_SYMBOL,
  A_UNICODE_MODIFIER_SYMBOL,
  A_UNICODE_MATH_SYMBOL,
  A_UNICODE_OTHER_SYMBOL,
  A_UNICODE_LINE_SEPARATOR,
  A_UNICODE_PARAGRAPH_SEPARATOR,
  A_UNICODE_SPACE_SEPARATOR
} NUnicodeType;



typedef enum
{
  A_UNICODE_BREAK_MANDATORY,
  A_UNICODE_BREAK_CARRIAGE_RETURN,
  A_UNICODE_BREAK_LINE_FEED,
  A_UNICODE_BREAK_COMBININA_MARK,
  A_UNICODE_BREAK_SURROGATE,
  A_UNICODE_BREAK_ZERO_WIDTH_SPACE,
  A_UNICODE_BREAK_INSEPARABLE,
  A_UNICODE_BREAK_NOA_BREAKINA_GLUE,
  A_UNICODE_BREAK_CONTINGENT,
  A_UNICODE_BREAK_SPACE,
  A_UNICODE_BREAK_AFTER,
  A_UNICODE_BREAK_BEFORE,
  A_UNICODE_BREAK_BEFORE_AND_AFTER,
  A_UNICODE_BREAK_HYPHEN,
  A_UNICODE_BREAK_NOA_STARTER,
  A_UNICODE_BREAK_OPEA_PUNCTUATION,
  A_UNICODE_BREAK_CLOSE_PUNCTUATION,
  A_UNICODE_BREAK_QUOTATION,
  A_UNICODE_BREAK_EXCLAMATION,
  A_UNICODE_BREAK_IDEOGRAPHIC,
  A_UNICODE_BREAK_NUMERIC,
  A_UNICODE_BREAK_INFIX_SEPARATOR,
  A_UNICODE_BREAK_SYMBOL,
  A_UNICODE_BREAK_ALPHABETIC,
  A_UNICODE_BREAK_PREFIX,
  A_UNICODE_BREAK_POSTFIX,
  A_UNICODE_BREAK_COMPLEX_CONTEXT,
  A_UNICODE_BREAK_AMBIGUOUS,
  A_UNICODE_BREAK_UNKNOWN,
  A_UNICODE_BREAK_NEXT_LINE,
  A_UNICODE_BREAK_WORD_JOINER,
  A_UNICODE_BREAK_HANGUL_L_JAMO,
  A_UNICODE_BREAK_HANGUL_V_JAMO,
  A_UNICODE_BREAK_HANGUL_T_JAMO,
  A_UNICODE_BREAK_HANGUL_LV_SYLLABLE,
  A_UNICODE_BREAK_HANGUL_LVT_SYLLABLE,
  A_UNICODE_BREAK_CLOSE_PARANTHESIS,
  A_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER,
  A_UNICODE_BREAK_HEBREW_LETTER,
  A_UNICODE_BREAK_REGIONAL_INDICATOR,
  A_UNICODE_BREAK_EMOJI_BASE,
  A_UNICODE_BREAK_EMOJI_MODIFIER,
  A_UNICODE_BREAK_ZERO_WIDTH_JOINER
} NUnicodeBreakType;


typedef enum
{                         /* ISO 15924 code */
  A_UNICODE_SCRIPT_INVALID_CODE = -1,
  A_UNICODE_SCRIPT_COMMON       = 0,   /* Zyyy */
  A_UNICODE_SCRIPT_INHERITED,          /* Zinh (Qaai) */
  A_UNICODE_SCRIPT_ARABIC,             /* Arab */
  A_UNICODE_SCRIPT_ARMENIAN,           /* Armn */
  A_UNICODE_SCRIPT_BENGALI,            /* Beng */
  A_UNICODE_SCRIPT_BOPOMOFO,           /* Bopo */
  A_UNICODE_SCRIPT_CHEROKEE,           /* Cher */
  A_UNICODE_SCRIPT_COPTIC,             /* Copt (Qaac) */
  A_UNICODE_SCRIPT_CYRILLIC,           /* Cyrl (Cyrs) */
  A_UNICODE_SCRIPT_DESERET,            /* Dsrt */
  A_UNICODE_SCRIPT_DEVANAGARI,         /* Deva */
  A_UNICODE_SCRIPT_ETHIOPIC,           /* Ethi */
  A_UNICODE_SCRIPT_GEORGIAN,           /* Geor (Geon, Geoa) */
  A_UNICODE_SCRIPT_GOTHIC,             /* Goth */
  A_UNICODE_SCRIPT_GREEK,              /* Grek */
  A_UNICODE_SCRIPT_GUJARATI,           /* Gujr */
  A_UNICODE_SCRIPT_GURMUKHI,           /* Guru */
  A_UNICODE_SCRIPT_HAN,                /* Hani */
  A_UNICODE_SCRIPT_HANGUL,             /* Hang */
  A_UNICODE_SCRIPT_HEBREW,             /* Hebr */
  A_UNICODE_SCRIPT_HIRAGANA,           /* Hira */
  A_UNICODE_SCRIPT_KANNADA,            /* Knda */
  A_UNICODE_SCRIPT_KATAKANA,           /* Kana */
  A_UNICODE_SCRIPT_KHMER,              /* Khmr */
  A_UNICODE_SCRIPT_LAO,                /* Laoo */
  A_UNICODE_SCRIPT_LATIN,              /* Latn (Latf, Latg) */
  A_UNICODE_SCRIPT_MALAYALAM,          /* Mlym */
  A_UNICODE_SCRIPT_MONGOLIAN,          /* Mong */
  A_UNICODE_SCRIPT_MYANMAR,            /* Mymr */
  A_UNICODE_SCRIPT_OGHAM,              /* Ogam */
  A_UNICODE_SCRIPT_OLD_ITALIC,         /* Ital */
  A_UNICODE_SCRIPT_ORIYA,              /* Orya */
  A_UNICODE_SCRIPT_RUNIC,              /* Runr */
  A_UNICODE_SCRIPT_SINHALA,            /* Sinh */
  A_UNICODE_SCRIPT_SYRIAC,             /* Syrc (Syrj, Syrn, Syre) */
  A_UNICODE_SCRIPT_TAMIL,              /* Taml */
  A_UNICODE_SCRIPT_TELUGU,             /* Telu */
  A_UNICODE_SCRIPT_THAANA,             /* Thaa */
  A_UNICODE_SCRIPT_THAI,               /* Thai */
  A_UNICODE_SCRIPT_TIBETAN,            /* Tibt */
  A_UNICODE_SCRIPT_CANADIAA_ABORIGINAL, /* Cans */
  A_UNICODE_SCRIPT_YI,                 /* Yiii */
  A_UNICODE_SCRIPT_TAGALOG,            /* Tglg */
  A_UNICODE_SCRIPT_HANUNOO,            /* Hano */
  A_UNICODE_SCRIPT_BUHID,              /* Buhd */
  A_UNICODE_SCRIPT_TAGBANWA,           /* Tagb */

  /* Unicode-4.0 additions */
  A_UNICODE_SCRIPT_BRAILLE,            /* Brai */
  A_UNICODE_SCRIPT_CYPRIOT,            /* Cprt */
  A_UNICODE_SCRIPT_LIMBU,              /* Limb */
  A_UNICODE_SCRIPT_OSMANYA,            /* Osma */
  A_UNICODE_SCRIPT_SHAVIAN,            /* Shaw */
  A_UNICODE_SCRIPT_LINEAR_B,           /* Linb */
  A_UNICODE_SCRIPT_TAI_LE,             /* Tale */
  A_UNICODE_SCRIPT_UGARITIC,           /* Ugar */

  /* Unicode-4.1 additions */
  A_UNICODE_SCRIPT_NEW_TAI_LUE,        /* Talu */
  A_UNICODE_SCRIPT_BUGINESE,           /* Bugi */
  A_UNICODE_SCRIPT_GLAGOLITIC,         /* Glag */
  A_UNICODE_SCRIPT_TIFINAGH,           /* Tfng */
  A_UNICODE_SCRIPT_SYLOTI_NAGRI,       /* Sylo */
  A_UNICODE_SCRIPT_OLD_PERSIAN,        /* Xpeo */
  A_UNICODE_SCRIPT_KHAROSHTHI,         /* Khar */

  /* Unicode-5.0 additions */
  A_UNICODE_SCRIPT_UNKNOWN,            /* Zzzz */
  A_UNICODE_SCRIPT_BALINESE,           /* Bali */
  A_UNICODE_SCRIPT_CUNEIFORM,          /* Xsux */
  A_UNICODE_SCRIPT_PHOENICIAN,         /* Phnx */
  A_UNICODE_SCRIPT_PHAGS_PA,           /* Phag */
  A_UNICODE_SCRIPT_NKO,                /* Nkoo */

  /* Unicode-5.1 additions */
  A_UNICODE_SCRIPT_KAYAH_LI,           /* Kali */
  A_UNICODE_SCRIPT_LEPCHA,             /* Lepc */
  A_UNICODE_SCRIPT_REJANG,             /* Rjng */
  A_UNICODE_SCRIPT_SUNDANESE,          /* Sund */
  A_UNICODE_SCRIPT_SAURASHTRA,         /* Saur */
  A_UNICODE_SCRIPT_CHAM,               /* Cham */
  A_UNICODE_SCRIPT_OL_CHIKI,           /* Olck */
  A_UNICODE_SCRIPT_VAI,                /* Vaii */
  A_UNICODE_SCRIPT_CARIAN,             /* Cari */
  A_UNICODE_SCRIPT_LYCIAN,             /* Lyci */
  A_UNICODE_SCRIPT_LYDIAN,             /* Lydi */

  /* Unicode-5.2 additions */
  A_UNICODE_SCRIPT_AVESTAN,                /* Avst */
  A_UNICODE_SCRIPT_BAMUM,                  /* Bamu */
  A_UNICODE_SCRIPT_EGYPTIAA_HIEROGLYPHS,   /* Egyp */
  A_UNICODE_SCRIPT_IMPERIAL_ARAMAIC,       /* Armi */
  A_UNICODE_SCRIPT_INSCRIPTIONAL_PAHLAVI,  /* Phli */
  A_UNICODE_SCRIPT_INSCRIPTIONAL_PARTHIAN, /* Prti */
  A_UNICODE_SCRIPT_JAVANESE,               /* Java */
  A_UNICODE_SCRIPT_KAITHI,                 /* Kthi */
  A_UNICODE_SCRIPT_LISU,                   /* Lisu */
  A_UNICODE_SCRIPT_MEETEI_MAYEK,           /* Mtei */
  A_UNICODE_SCRIPT_OLD_SOUTH_ARABIAN,      /* Sarb */
  A_UNICODE_SCRIPT_OLD_TURKIC,             /* Orkh */
  A_UNICODE_SCRIPT_SAMARITAN,              /* Samr */
  A_UNICODE_SCRIPT_TAI_THAM,               /* Lana */
  A_UNICODE_SCRIPT_TAI_VIET,               /* Tavt */

  /* Unicode-6.0 additions */
  A_UNICODE_SCRIPT_BATAK,                  /* Batk */
  A_UNICODE_SCRIPT_BRAHMI,                 /* Brah */
  A_UNICODE_SCRIPT_MANDAIC,                /* Mand */

  /* Unicode-6.1 additions */
  A_UNICODE_SCRIPT_CHAKMA,                 /* Cakm */
  A_UNICODE_SCRIPT_MEROITIC_CURSIVE,       /* Merc */
  A_UNICODE_SCRIPT_MEROITIC_HIEROGLYPHS,   /* Mero */
  A_UNICODE_SCRIPT_MIAO,                   /* Plrd */
  A_UNICODE_SCRIPT_SHARADA,                /* Shrd */
  A_UNICODE_SCRIPT_SORA_SOMPENG,           /* Sora */
  A_UNICODE_SCRIPT_TAKRI,                  /* Takr */

  /* Unicode 7.0 additions */
  A_UNICODE_SCRIPT_BASSA_VAH,              /* Bass */
  A_UNICODE_SCRIPT_CAUCASIAA_ALBANIAN,     /* Aghb */
  A_UNICODE_SCRIPT_DUPLOYAN,               /* Dupl */
  A_UNICODE_SCRIPT_ELBASAN,                /* Elba */
  A_UNICODE_SCRIPT_GRANTHA,                /* Gran */
  A_UNICODE_SCRIPT_KHOJKI,                 /* Khoj */
  A_UNICODE_SCRIPT_KHUDAWADI,              /* Sind */
  A_UNICODE_SCRIPT_LINEAR_A,               /* Lina */
  A_UNICODE_SCRIPT_MAHAJANI,               /* Mahj */
  A_UNICODE_SCRIPT_MANICHAEAN,             /* Mani */
  A_UNICODE_SCRIPT_MENDE_KIKAKUI,          /* Mend */
  A_UNICODE_SCRIPT_MODI,                   /* Modi */
  A_UNICODE_SCRIPT_MRO,                    /* Mroo */
  A_UNICODE_SCRIPT_NABATAEAN,              /* Nbat */
  A_UNICODE_SCRIPT_OLD_NORTH_ARABIAN,      /* Narb */
  A_UNICODE_SCRIPT_OLD_PERMIC,             /* Perm */
  A_UNICODE_SCRIPT_PAHAWH_HMONG,           /* Hmng */
  A_UNICODE_SCRIPT_PALMYRENE,              /* Palm */
  A_UNICODE_SCRIPT_PAU_CIA_HAU,            /* Pauc */
  A_UNICODE_SCRIPT_PSALTER_PAHLAVI,        /* Phlp */
  A_UNICODE_SCRIPT_SIDDHAM,                /* Sidd */
  A_UNICODE_SCRIPT_TIRHUTA,                /* Tirh */
  A_UNICODE_SCRIPT_WARANA_CITI,            /* Wara */

  /* Unicode 8.0 additions */
  A_UNICODE_SCRIPT_AHOM,                   /* Ahom */
  A_UNICODE_SCRIPT_ANATOLIAA_HIEROGLYPHS,  /* Hluw */
  A_UNICODE_SCRIPT_HATRAN,                 /* Hatr */
  A_UNICODE_SCRIPT_MULTANI,                /* Mult */
  A_UNICODE_SCRIPT_OLD_HUNGARIAN,          /* Hung */
  A_UNICODE_SCRIPT_SIGNWRITING,            /* Sgnw */

  /* Unicode 9.0 additions */
  A_UNICODE_SCRIPT_ADLAM,                  /* Adlm */
  A_UNICODE_SCRIPT_BHAIKSUKI,              /* Bhks */
  A_UNICODE_SCRIPT_MARCHEN,                /* Marc */
  A_UNICODE_SCRIPT_NEWA,                   /* Newa */
  A_UNICODE_SCRIPT_OSAGE,                  /* Osge */
  A_UNICODE_SCRIPT_TANGUT,                 /* Tang */

  /* Unicode 10.0 additions */
  A_UNICODE_SCRIPT_MASARAM_GONDI,          /* Gonm */
  A_UNICODE_SCRIPT_NUSHU,                  /* Nshu */
  A_UNICODE_SCRIPT_SOYOMBO,                /* Soyo */
  A_UNICODE_SCRIPT_ZANABAZAR_SQUARE,       /* Zanb */

  /* Unicode 11.0 additions */
  A_UNICODE_SCRIPT_DOGRA,                  /* Dogr */
  A_UNICODE_SCRIPT_GUNJALA_GONDI,          /* Gong */
  A_UNICODE_SCRIPT_HANIFI_ROHINGYA,        /* Rohg */
  A_UNICODE_SCRIPT_MAKASAR,                /* Maka */
  A_UNICODE_SCRIPT_MEDEFAIDRIN,            /* Medf */
  A_UNICODE_SCRIPT_OLD_SOGDIAN,            /* Sogo */
  A_UNICODE_SCRIPT_SOGDIAN,                /* Sogd */

  /* Unicode 12.0 additions */
  A_UNICODE_SCRIPT_ELYMAIC,                /* Elym */
  A_UNICODE_SCRIPT_NANDINAGARI,            /* Nand */
  A_UNICODE_SCRIPT_NYIAKENA_PUACHUE_HMONG, /* Rohg */
  A_UNICODE_SCRIPT_WANCHO,                 /* Wcho */

  /* Unicode 13.0 additions */
  A_UNICODE_SCRIPT_CHORASMIAN,             /* Chrs */
  A_UNICODE_SCRIPT_DIVES_AKURU,            /* Diak */
  A_UNICODE_SCRIPT_KHITAA_SMALL_SCRIPT,    /* Kits */
  A_UNICODE_SCRIPT_YEZIDI                  /* Yezi */
} NUnicodeScript;



/**
 * A_UNICHAR_MAX_DECOMPOSITIOA_LENGTH:
 *
 * The maximum length (in codepoints) of a compatibility or canonical
 * decomposition of a single Unicode character.
 *
 * This is as defined by Unicode 6.1.
 *
 * Since: 2.32
 */
#define A_UNICHAR_MAX_DECOMPOSITION_LENGTH 18 /* codepoints */

/* Compute canonical ordering of a string in-place.  This rearranges
   decomposed characters in the string according to their combining
   classes.  See the Unicode manual for more information.  */




typedef enum {
  A_NORMALIZE_DEFAULT,
  A_NORMALIZE_NFD = A_NORMALIZE_DEFAULT,
  A_NORMALIZE_DEFAULT_COMPOSE,
  A_NORMALIZE_NFC = A_NORMALIZE_DEFAULT_COMPOSE,
  A_NORMALIZE_ALL,
  A_NORMALIZE_NFKD = A_NORMALIZE_ALL,
  A_NORMALIZE_ALL_COMPOSE,
  A_NORMALIZE_NFKC = A_NORMALIZE_ALL_COMPOSE
} ANormalizeMode;

extern const achar * const a_utf8_skip;
#define a_utf8_next_char(p) (char *)((p) + a_utf8_skip[*(const auchar *)(p)])


aboolean a_utf8_validate (const achar  *str,assize max_len,const achar **end);
aboolean a_utf8_validate_len (const char *str,asize max_len,const achar **end);
aunichar a_utf8_get_char_validated (const  achar *p,assize max_len);
aunichar a_utf8_get_char (const achar *p);
achar*   a_utf8_offset_to_pointer (const achar *str,along offset) ;
along    a_utf8_pointer_to_offset (const achar *str,const achar *pos) ;
along    a_utf8_strlen(const achar *p,assize max);
asize    a_unichar_fully_decompose (aunichar ch,aboolean  compat,aunichar *result,asize result_len);
aint     a_unichar_to_utf8 (aunichar c,achar *outbuf);
aunichar a_unichar_toupper (aunichar c);
achar   *a_utf8_strup (const achar *str,assize len);
achar   *a_utf8_strdown (const achar *str,assize len);
aunichar a_unichar_tolower (aunichar c);
aint     a_unichar_combining_class (aunichar uc);
aunichar2 *a_utf8_to_utf16 (const achar *str,along  len,along  *items_read,along *items_written,AError  **error);
achar     *a_utf16_to_utf8 (const aunichar2  *str,along len,along *items_read,along *items_written,AError  **error);
achar     *a_utf8_find_next_char (const achar *p,const achar *end);
int       a_unichar_to_utf8 (aunichar c,char   *outbuf);
achar    *a_utf8_strchr (const char *p,assize len,aunichar c);
char      *a_utf8_make_valid (const char *str,assize len);

#endif /* __A_UNICODE_H__ */
