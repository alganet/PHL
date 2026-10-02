# src/ph7/builtin_iconv.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1212/1312 lines (92.38%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `/*` |
|     - |    7 | ` * Section:` |
|     - |    8 | ` *    php's iconv extension: the character-set converter and the string,` |
|     - |    9 | ` *    encoding-setting and MIME halves built on it.` |
|     - |   10 | ` * Status:` |
|     - |   11 | ` *    Stable.` |
|     - |   12 | ` *` |
|     - |   13 | ` * php's iconv is a thin shell over the C library's iconv(3), so its contract is` |
|     - |   14 | ` * really glibc's -- and that is what is reproduced here, over PHL's own` |
|     - |   15 | ` * converter rather than the platform's, so a Windows build answers what a Linux` |
|     - |   16 | ` * one does. Three things had to be measured rather than read, because each is` |
|     - |   17 | ` * the library's behaviour and not php's:` |
|     - |   18 | ` *` |
|     - |   19 | ` *   The NAME grammar. Everything before the first '/' is the code-set name,` |
|     - |   20 | ` *   compared case-insensitively after every character outside` |
|     - |   21 | ``  *   [alnum] `_ - . , :` is DROPPED -- so `" ASCII "`, `"A SCII"` and `"ASCII\t"` `` |
|     - |   22 | ` *   all name ASCII. The segment between the first and second '/' must be empty;` |
|     - |   23 | ` *   what follows is a list of error-handler tokens split on '/' and ',', and` |
|     - |   24 | `` *   `TRANSLIT` anywhere in that list turns transliteration on. That is why`` |
|     - |   25 | `` *   `ASCII//TRANSLITX` is not transliteration and `ASCII//A/B/TRANSLIT` is.`` |
|     - |   26 | ` *` |
|     - |   27 | ` *   //IGNORE is mostly NOT the library's. The library has one -- it drops a` |
|     - |   28 | ` *   character the target cannot hold and carries on -- but it then reports the` |
|     - |   29 | ` *   whole call as an illegal sequence anyway, so on its own it only turns one` |
|     - |   30 | `` *   failure into another: `iconv("UTF-8","ASCII//IGNORE,X","a\u{4e2d}b")` is`` |
|     - |   31 | `` *   FALSE. What makes //IGNORE useful is php's own, in `_php_check_ignore()`,`` |
|     - |   32 | ` *   and that check is a case-SENSITIVE SUFFIX test on the TO charset only, for` |
|     - |   33 | `` *   `//IGNORE` or `//IGNORE//TRANSLIT`, with a length guard that makes the bare`` |
|     - |   34 | `` *   string `"//IGNORE"` fail it. So `ASCII//ignore` ignores nothing,`` |
|     - |   35 | `` *   `UTF-8//IGNORE` as the FROM charset ignores nothing, and what php's does is`` |
|     - |   36 | ` *   byte-granular: on an illegal sequence it advances the input by ONE BYTE and` |
|     - |   37 | ` *   converts again, and a failure with a single byte left is taken for success.` |
|     - |   38 | ` *` |
|     - |   39 | ` *   The UTF-8 accepted here is the ORIGINAL six-byte one, not Unicode's` |
|     - |   40 | `` *   four-byte cut: `\xFC\x84\x80\x80\x80\x80` (U+4000000) converts, while`` |
|     - |   41 | ` *   overlong forms, the surrogate range and the lead bytes C0/C1/FE/FF do not.` |
|     - |   42 | ` *   The difference between the two diagnostics is only WHERE the input ran out:` |
|     - |   43 | ` *   a truncated but so-far-valid sequence at the END of the string is` |
|     - |   44 | `` *   `Detected an incomplete multibyte character`, and everything else is`` |
|     - |   45 | `` *   `Detected an illegal character`.`` |
|     - |   46 | ` *` |
|     - |   47 | ` * The transliteration table is glibc's own, swept out of it code point by code` |
|     - |   48 | ` * point (3425 rows, 967 of them the EMPTY replacement that makes a combining` |
|     - |   49 | ` * mark disappear, plus 55 whose ISO-8859-1 answer differs from their ASCII` |
|     - |   50 | `` * one). A code point with no row is `?`. PHL models UTF-8, ISO-8859-1 and`` |
|     - |   51 | ` * US-ASCII only (the scope cut, the same one mb_ carries); a php-valid name` |
|     - |   52 | ` * outside those three gets php's own "Wrong encoding" warning, which is what` |
|     - |   53 | ` * php answers for a name the platform's iconv does not have either.` |
|     - |   54 | ` *` |
|     - |   55 | ` * Verified differentially against php 8.5 over 16000 randomized conversions` |
|     - |   56 | ` * (every encoding pair, every suffix spelling, well-formed and malformed` |
|     - |   57 | ` * input). The one recorded divergence: an IGNORE token that is NOT` |
|     - |   58 | `` * php's suffix (`ASCII//TRANSLIT,IGNORE`, `ASCII//ignore`) reaches the`` |
|     - |   59 | ` * library's error handler, whose skip-and-continue behaviour on MALFORMED input` |
|     - |   60 | ` * is neither documented nor stable across iconv implementations; PHL answers` |
|     - |   61 | ` * php's own suffix rule there, so those spellings fail where php sometimes` |
|     - |   62 | ` * recovers. Well-formed input agrees for every spelling.` |
|     - |   63 | ` */` |
|     - |   64 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|     - |   65 |  |
|     - |   66 | `/* --- Encodings --------------------------------------------------------- */` |
|     - |   67 |  |
|     - |   68 | `#define ICV_UTF8       0` |
|     - |   69 | `#define ICV_LATIN1     1` |
|     - |   70 | `#define ICV_ASCII      2` |
|     - |   71 | `#define ICV_ISO2022JP  3` |
|     - |   72 |  |
|     - |   73 | `/*` |
|     - |   74 | ` * ISO-2022-JP is the one code set here that is not a function of the bytes in` |
|     - |   75 | ` * front of the converter: an escape sequence shifts which set the following` |
|     - |   76 | ` * bytes are read in, and it stays shifted until the next one. So a decode may` |
|     - |   77 | ` * consume bytes and hand back NO character, and an encode has to know which` |
|     - |   78 | ` * set it is already in and return to ASCII before the string ends.` |
|     - |   79 | ` *` |
|     - |   80 | ` * Only the four shifts glibc's plain ISO-2022-JP converter answers are` |
|     - |   81 | ` * recognised -- ASCII, JIS X 0201 Roman, and the two spellings of JIS X 0208.` |
|     - |   82 | ` * ESC ( I (halfwidth katakana) is NOT one of them, which is why a halfwidth` |
|     - |   83 | ` * katakana is unconvertible in this direction, and every other escape is left` |
|     - |   84 | ` * to the pass-through arm below rather than refused.` |
|     - |   85 | ` */` |
|     - |   86 | `#define ICV_G0_ASCII  0` |
|     - |   87 | `#define ICV_G0_ROMAN  1` |
|     - |   88 | `#define ICV_G0_KANJI  2` |
|     - |   89 |  |
|     - |   90 | `/* A decode that consumed a shift and produced nothing. Above every code point` |
|     - |   91 | ` * IcvDecode() can answer -- the six-byte UTF-8 form tops out at 0x7FFFFFFF. */` |
|     - |   92 | `#define ICV_NOCHAR    0xFFFFFFFF` |
|     - |   93 |  |
|     - |   94 | `/*` |
|     - |   95 | ` * php's own cap on an encoding NAME, checked before the name is looked at:` |
|     - |   96 | `` * ICONV_CSNMAXLEN is 64 and the test is `>=`, so a 64-character name is already`` |
|     - |   97 | ` * "exceeds the maximum allowed length of 64 characters".` |
|     - |   98 | ` */` |
|     - |   99 | `#define ICV_CSNMAXLEN 64` |
|     - |  100 |  |
|     - |  101 | `/*` |
|     - |  102 | ` * The names glibc registers for the code sets PHL models, minus its` |
|     - |  103 | `` * numeric OSF/IBM aliases (`OSF00010020`, `IBM903`…), which are recorded as`` |
|     - |  104 | ` * refused rather than implemented. Compared after ICV normalisation, so the` |
|     - |  105 | ` * spelling here is the canonical upper-case one.` |
|     - |  106 | ` */` |
|     - |  107 | `static const struct IcvEncName {` |
|     - |  108 | `	const char *zName;` |
|     - |  109 | `	int iEnc;` |
|     - |  110 | `} aIcvEncName[] = {` |
|     - |  111 | `	{ "UTF-8",            ICV_UTF8   },` |
|     - |  112 | `	{ "UTF8",             ICV_UTF8   },` |
|     - |  113 | `	{ "ISO-IR-193",       ICV_UTF8   },` |
|     - |  114 | `	{ "ISO-8859-1",       ICV_LATIN1 },` |
|     - |  115 | `	{ "ISO_8859-1",       ICV_LATIN1 },` |
|     - |  116 | `	{ "ISO8859-1",        ICV_LATIN1 },` |
|     - |  117 | `	{ "ISO88591",         ICV_LATIN1 },` |
|     - |  118 | `	{ "ISO_8859-1:1987",  ICV_LATIN1 },` |
|     - |  119 | `	{ "ISO-IR-100",       ICV_LATIN1 },` |
|     - |  120 | `	{ "LATIN1",           ICV_LATIN1 },` |
|     - |  121 | `	{ "L1",               ICV_LATIN1 },` |
|     - |  122 | `	{ "CP819",            ICV_LATIN1 },` |
|     - |  123 | `	{ "IBM819",           ICV_LATIN1 },` |
|     - |  124 | `	{ "CSISOLATIN1",      ICV_LATIN1 },` |
|     - |  125 | `	{ "8859_1",           ICV_LATIN1 },` |
|     - |  126 | `	{ "ASCII",            ICV_ASCII  },` |
|     - |  127 | `	{ "US-ASCII",         ICV_ASCII  },` |
|     - |  128 | `	{ "US",               ICV_ASCII  },` |
|     - |  129 | `	{ "ANSI_X3.4-1968",   ICV_ASCII  },` |
|     - |  130 | `	{ "ANSI_X3.4-1986",   ICV_ASCII  },` |
|     - |  131 | `	{ "ANSI_X3.4",        ICV_ASCII  },` |
|     - |  132 | `	{ "ISO646-US",        ICV_ASCII  },` |
|     - |  133 | `	{ "ISO_646.IRV:1991", ICV_ASCII  },` |
|     - |  134 | `	{ "ISO-IR-6",         ICV_ASCII  },` |
|     - |  135 | `	{ "CP367",            ICV_ASCII  },` |
|     - |  136 | `	{ "IBM367",           ICV_ASCII  },` |
|     - |  137 | `	{ "CSASCII",          ICV_ASCII  },` |
|     - |  138 | `#ifdef PH7_ENABLE_JIS` |
|     - |  139 | `	/* glibc registers exactly these three for the plain converter. Its` |
|     - |  140 | `	 * ISO-2022-JP-2 and -3 are separate converters with wider set repertoires,` |
|     - |  141 | `	 * so they stay unknown here rather than being answered by a narrower one. */` |
|     - |  142 | `	{ "ISO-2022-JP",      ICV_ISO2022JP },` |
|     - |  143 | `	{ "ISO2022JP",        ICV_ISO2022JP },` |
|     - |  144 | `	{ "CSISO2022JP",      ICV_ISO2022JP }` |
|     - |  145 | `#endif` |
|     - |  146 | `};` |
|     - |  147 |  |
|     - |  148 | `/* Is c one of the characters the name normaliser KEEPS? */` |
| 10861 |  149 | `static int IcvNameChar(int c)` |
|     5 |  150 | `{` |
| 11650 |  151 | `	return (c >= '0' && c <= '9') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= 'a' && c <= 'z')` |
| 16469 |  152 | `		\|\| c == '_' \|\| c == '-' \|\| c == '.' \|\| c == ',' \|\| c == ':';` |
|     5 |  153 | `}` |
| 10879 |  154 | `static int IcvUpper(int c)` |
|     5 |  155 | `{` |
| 10884 |  156 | `	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;` |
|     5 |  157 | `}` |
|     - |  158 |  |
|     - |  159 | `/* What a parsed charset argument came to. */` |
|     - |  160 | `typedef struct icv_cs icv_cs;` |
|     - |  161 | `struct icv_cs {` |
|     - |  162 | `	int iEnc;        /* ICV_* id, or -1 when the name is not one PHL models */` |
|     - |  163 | `	int bTranslit;   /* a TRANSLIT token appeared in the error-handler list */` |
|     - |  164 | `	int bIgnore;     /* an IGNORE token did -- the LIBRARY's ignore, not php's */` |
|     - |  165 | `	/* The name as it was GIVEN, kept because the string family reports an` |
|     - |  166 | `	 * unknown one late -- at the point a conversion would have been opened --` |
|     - |  167 | `	 * and by then the argument that carried it may be gone. Bounded by the` |
|     - |  168 | `	 * length cap, which is checked before any of this. */` |
|     - |  169 | `	char zName[ICV_CSNMAXLEN];` |
|     - |  170 | `	int nName;` |
|     - |  171 | `};` |
|     - |  172 |  |
|     - |  173 | `/*` |
|     - |  174 | ` * How much of a charset argument the converter can SEE. php's ZPP hands the` |
|     - |  175 | ` * whole php string down, but iconv_open() takes a C string, so a name stops at` |
|     - |  176 | ` * its first NUL -- and so does the "Wrong encoding" message, which prints the` |
|     - |  177 | ` * same pointer with %s.` |
|     - |  178 | ` */` |
|  2082 |  179 | `static int IcvNameLen(const char *z,int n)` |
|     5 |  180 | `{` |
|     - |  181 | `	int i;` |
| 18032 |  182 | `	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}` |
|  2087 |  183 | `	return i;` |
|     5 |  184 | `}` |
|     - |  185 | `/* Copy z[0..n-1] into zOut, keeping only the characters the normaliser keeps` |
|     - |  186 | ` * and upper-casing them. Answers the length written, capped at nOut. */` |
|  1758 |  187 | `static int IcvNormalize(char *zOut,int nOut,const char *z,int n)` |
|     5 |  188 | `{` |
|  1763 |  189 | `	int i,k = 0;` |
| 11440 |  190 | `	for( i = 0 ; i < n ; ++i ){` |
|  9682 |  191 | `		int c = (unsigned char)z[i];` |
|  9682 |  192 | `		if( IcvNameChar(c) && k < nOut ){` |
|  9668 |  193 | `			zOut[k++] = (char)IcvUpper(c);` |
|  4654 |  194 | `		}` |
|  4666 |  195 | `	}` |
|  1763 |  196 | `	return k;` |
|     5 |  197 | `}` |
|     - |  198 | `/*` |
|     - |  199 | ` * Parse one charset argument. The code-set name is z[0..] up to the first '/',` |
|     - |  200 | ` * normalised; the segment up to the second '/' must be empty; every remaining` |
|     - |  201 | `` * '/'- or ','-separated token is an error handler, and `TRANSLIT` is the one`` |
|     - |  202 | ` * that means anything here.` |
|     - |  203 | ` */` |
|  1606 |  204 | `static void IcvParseCharset(const char *z,int n,icv_cs *pCs)` |
|     5 |  205 | `{` |
|     - |  206 | `	char zBuf[ICV_CSNMAXLEN];` |
|     - |  207 | `	int iFirst,iSecond,nBuf,k;` |
|  1611 |  208 | `	pCs->iEnc = -1;` |
|  1611 |  209 | `	pCs->bTranslit = 0;` |
|  1611 |  210 | `	pCs->bIgnore = 0;` |
|  1611 |  211 | `	n = IcvNameLen(z,n);` |
|  1611 |  212 | `	pCs->nName = n < ICV_CSNMAXLEN ? n : ICV_CSNMAXLEN;` |
|  1611 |  213 | `	SyMemcpy(z,pCs->zName,(sxu32)pCs->nName);` |
| 11276 |  214 | `	for( iFirst = 0 ; iFirst < n && z[iFirst] != '/' ; ++iFirst ){}` |
|  1611 |  215 | `	nBuf = IcvNormalize(zBuf,(int)sizeof(zBuf),z,iFirst);` |
|  1611 |  216 | `	if( nBuf == 0 ){` |
|     - |  217 | `		/* php hands the name straight to iconv_open(), where an empty CODE SET` |
|     - |  218 | ``		 * means the LOCALE's charset -- which is why `"//IGNORE"` on its own`` |
|     - |  219 | `		 * names one. PHL has no locale and one charset it is written in, so the` |
|     - |  220 | `		 * empty name is UTF-8 here: what php answers on any UTF-8 locale,` |
|     - |  221 | `		 * deterministically rather than by environment. */` |
|     3 |  222 | `		pCs->iEnc = ICV_UTF8;` |
|     1 |  223 | `	}` |
| 10598 |  224 | `	for( k = 0 ; nBuf > 0 && k < (int)SX_ARRAYSIZE(aIcvEncName) ; ++k ){` |
| 10519 |  225 | `		const char *zCand = aIcvEncName[k].zName;` |
| 10519 |  226 | `		int nCand = (int)SyStrlen(zCand);` |
| 10519 |  227 | `		if( nCand == nBuf && SyMemcmp(zCand,zBuf,(sxu32)nCand) == 0 ){` |
|  1532 |  228 | `			pCs->iEnc = aIcvEncName[k].iEnc;` |
|  1532 |  229 | `			break;` |
|     - |  230 | `		}` |
|  4482 |  231 | `	}` |
|  1611 |  232 | `	if( iFirst >= n ){` |
|  1459 |  233 | `		return;` |
|     - |  234 | `	}` |
|     - |  235 | `	/* Between the first and second '/' is the segment that carries no handler,` |
|     - |  236 | ``	 * and it has to be empty: `ASCII/x/TRANSLIT` names nothing at all, which is`` |
|     - |  237 | ``	 * why php answers "Wrong encoding" for it while `ASCII/ /TRANSLIT` -- whose`` |
|     - |  238 | `	 * space the normaliser drops -- converts. */` |
|   167 |  239 | `	for( iSecond = iFirst + 1 ; iSecond < n && z[iSecond] != '/' ; ++iSecond ){}` |
|   155 |  240 | `	if( IcvNormalize(zBuf,(int)sizeof(zBuf),&z[iFirst+1],iSecond - iFirst - 1) > 0 ){` |
|     5 |  241 | `		pCs->iEnc = -1;` |
|     2 |  242 | `	}` |
|     - |  243 | `	/* Everything past it is a '/'- or ','-separated list of error handlers. */` |
|   155 |  244 | `	nBuf = 0;` |
|  1515 |  245 | `	for( k = iSecond + 1 ; k <= n ; ++k ){` |
|  1363 |  246 | `		int c = (k < n) ? (unsigned char)z[k] : '/';` |
|  1363 |  247 | `		if( c == '/' \|\| c == ',' ){` |
|   179 |  248 | `			if( nBuf == 8 && SyMemcmp(zBuf,"TRANSLIT",8) == 0 ){` |
|   115 |  249 | `				pCs->bTranslit = 1;` |
|    56 |  250 | `			}` |
|   179 |  251 | `			if( nBuf == 6 && SyMemcmp(zBuf,"IGNORE",6) == 0 ){` |
|    40 |  252 | `				pCs->bIgnore = 1;` |
|    19 |  253 | `			}` |
|   179 |  254 | `			nBuf = 0;` |
|   179 |  255 | `			continue;` |
|     - |  256 | `		}` |
|  1187 |  257 | `		if( IcvNameChar(c) ){` |
|  1187 |  258 | `			if( nBuf < (int)sizeof(zBuf) ){` |
|  1187 |  259 | `				zBuf[nBuf++] = (char)IcvUpper(c);` |
|   595 |  260 | `			}else{` |
|   ! 0 |  261 | `				nBuf = (int)sizeof(zBuf) + 1;` |
|     - |  262 | `			}` |
|   592 |  263 | `		}` |
|   595 |  264 | `	}` |
|   774 |  265 | `}` |
|     - |  266 |  |
|     - |  267 | `/*` |
|     - |  268 | `` * php's `_php_check_ignore()`: a case-SENSITIVE suffix test on the RAW name,`` |
|     - |  269 | ` * with the length guard that keeps the bare "//IGNORE" from matching.` |
|     - |  270 | ` */` |
|   396 |  271 | `static int IcvCheckIgnore(const char *z,int n)` |
|     3 |  272 | `{` |
|   399 |  273 | `	n = IcvNameLen(z,n);` |
|   399 |  274 | `	if( n >= 9 && SyMemcmp(&z[n-8],"//IGNORE",8) == 0 ){` |
|    32 |  275 | `		return 1;` |
|     - |  276 | `	}` |
|   369 |  277 | `	if( n >= 19 && SyMemcmp(&z[n-18],"//IGNORE//TRANSLIT",18) == 0 ){` |
|     3 |  278 | `		return 1;` |
|     - |  279 | `	}` |
|   367 |  280 | `	return 0;` |
|   201 |  281 | `}` |
|     - |  282 |  |
|     - |  283 | `/* --- Transliteration --------------------------------------------------- */` |
|     - |  284 |  |
|     - |  285 | `/*` |
|     - |  286 | ` * One transliteration row: the source code point and the offset/length of its` |
|     - |  287 | ` * replacement in the shared blob beside it. glibc's table gives an ORDERED list` |
|     - |  288 | ` * of candidate replacements and takes the first that fits the target, which for` |
|     - |  289 | ` * the two targets PHL can miss with comes to exactly two columns: the general` |
|     - |  290 | ` * (ASCII) answer here, and the 55 rows whose ISO-8859-1 answer is a different` |
|     - |  291 | ` * string.` |
|     - |  292 | ` */` |
|     - |  293 | `typedef struct icv_translit icv_translit;` |
|     - |  294 | `struct icv_translit {` |
|     - |  295 | `	sxu32 cp;` |
|     - |  296 | `	sxu16 iOfft;` |
|     - |  297 | `	sxu16 nByte;` |
|     - |  298 | `};` |
|     - |  299 | `#include "builtin_iconv_translit.h"` |
|     - |  300 |  |
|     - |  301 | `/* Binary-search a cp-sorted transliteration table. */` |
|   124 |  302 | `static const icv_translit * IcvTranslitFind(const icv_translit *aTab,int nTab,sxu32 cp)` |
|     3 |  303 | `{` |
|   127 |  304 | `	int lo = 0,hi = nTab - 1;` |
|  1321 |  305 | `	while( lo <= hi ){` |
|  1305 |  306 | `		int mid = lo + (hi - lo) / 2;` |
|  1305 |  307 | `		if( aTab[mid].cp == cp ){` |
|   110 |  308 | `			return &aTab[mid];` |
|     - |  309 | `		}` |
|  1197 |  310 | `		if( aTab[mid].cp < cp ){` |
|   547 |  311 | `			lo = mid + 1;` |
|   275 |  312 | `		}else{` |
|   653 |  313 | `			hi = mid - 1;` |
|     - |  314 | `		}` |
|     3 |  315 | `	}` |
|    18 |  316 | `	return 0;` |
|    65 |  317 | `}` |
|     - |  318 |  |
|     - |  319 | `/* --- The converter ----------------------------------------------------- */` |
|     - |  320 |  |
|     - |  321 | `/* php's php_iconv_err_t, in php's own order of preference. */` |
|     - |  322 | `#define ICV_OK            0` |
|     - |  323 | `#define ICV_ILLEGAL_CHAR  1   /* an incomplete sequence at the end of the input */` |
|     - |  324 | `#define ICV_ILLEGAL_SEQ   2   /* anything else the source encoding refuses */` |
|     - |  325 | `#define ICV_WRONG_CHARSET 3` |
|     - |  326 | `#define ICV_TOO_BIG       4` |
|     - |  327 | `#define ICV_MALFORMED     5` |
|     - |  328 | `#define ICV_OUT_BY_BOUNDS 6` |
|     - |  329 | `#define ICV_UNKNOWN_ERR   7` |
|     - |  330 | `#define ICV_TOO_LONG      8` |
|     - |  331 |  |
|     - |  332 | `/*` |
|     - |  333 | ` * Decode the character at z[0..n-1] under iEnc. Answers its byte length with` |
|     - |  334 | ` * *pCp set to the code point, or 0 with *pErr set to the diagnostic the` |
|     - |  335 | ` * sequence earns. The UTF-8 accepted is the original six-byte encoding, minus` |
|     - |  336 | ` * overlongs, the surrogate range and the C0/C1/FE/FF lead bytes -- glibc's set,` |
|     - |  337 | ` * not Unicode's.` |
|     - |  338 | ` *` |
|     - |  339 | ` * *pG0 is the shift state ISO-2022-JP reads in and carries forward; every other` |
|     - |  340 | ` * code set leaves it alone. A shift consumes its three bytes and answers` |
|     - |  341 | ` * ICV_NOCHAR, so a caller counts characters by what it stores, not by how many` |
|     - |  342 | ` * times it went round.` |
|     - |  343 | ` */` |
|  7327 |  344 | `static int IcvDecode(const unsigned char *z,int n,int iEnc,sxu32 *pCp,int *pErr,int *pG0)` |
|     5 |  345 | `{` |
|     - |  346 | `	static const struct { unsigned char iLow,iHigh; int nSeq; sxu32 iMin; } aLead[] = {` |
|     - |  347 | `		{ 0xC2,0xDF,2,0x80       },` |
|     - |  348 | `		{ 0xE0,0xEF,3,0x800      },` |
|     - |  349 | `		{ 0xF0,0xF7,4,0x10000    },` |
|     - |  350 | `		{ 0xF8,0xFB,5,0x200000   },` |
|     - |  351 | `		{ 0xFC,0xFD,6,0x4000000  }` |
|     - |  352 | `	};` |
|  7332 |  353 | `	unsigned int c = z[0];` |
|     - |  354 | `	int i,k;` |
|  7332 |  355 | `	if( iEnc == ICV_LATIN1 ){` |
|   160 |  356 | `		*pCp = c;` |
|   160 |  357 | `		return 1;` |
|     - |  358 | `	}` |
|  7174 |  359 | `	if( iEnc == ICV_ASCII ){` |
|  1215 |  360 | `		if( c > 0x7F ){` |
|     8 |  361 | `			*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  362 | `			return 0;` |
|     - |  363 | `		}` |
|  1209 |  364 | `		*pCp = c;` |
|  1209 |  365 | `		return 1;` |
|     - |  366 | `	}` |
|     - |  367 | `#ifdef PH7_ENABLE_JIS` |
|  5961 |  368 | `	if( iEnc == ICV_ISO2022JP ){` |
|     - |  369 | `		sxu32 cp;` |
|   255 |  370 | `		if( c == 0x1B ){` |
|     - |  371 | `			/* An escape needs three bytes before it can be told from an` |
|     - |  372 | `			 * unrecognised one, so two bytes at the end of the input are an` |
|     - |  373 | `			 * INCOMPLETE character rather than an illegal sequence -- which is` |
|     - |  374 | `			 * what makes a truncated shift refuse even under //IGNORE. */` |
|    91 |  375 | `			if( n < 3 ){` |
|     5 |  376 | `				*pErr = ICV_ILLEGAL_CHAR;` |
|     5 |  377 | `				return 0;` |
|     - |  378 | `			}` |
|    87 |  379 | `			if( z[1] == '(' && (z[2] == 'B' \|\| z[2] == 'J') ){` |
|    39 |  380 | `				*pG0 = z[2] == 'B' ? ICV_G0_ASCII : ICV_G0_ROMAN;` |
|    39 |  381 | `				*pCp = ICV_NOCHAR;` |
|    39 |  382 | `				return 3;` |
|     - |  383 | `			}` |
|    49 |  384 | `			if( z[1] == '$' && (z[2] == 'B' \|\| z[2] == '@') ){` |
|     - |  385 | `				/* The 1978 and 1983 spellings name the same cells. */` |
|    41 |  386 | `				*pG0 = ICV_G0_KANJI;` |
|    41 |  387 | `				*pCp = ICV_NOCHAR;` |
|    41 |  388 | `				return 3;` |
|     - |  389 | `			}` |
|     - |  390 | `			/* Anything else is not a shift at all: 0x1B is below 0x21, so it` |
|     - |  391 | `			 * falls into the pass-through arm below and comes out as U+001B` |
|     - |  392 | `			 * with the two bytes after it read in whatever set is current. */` |
|     4 |  393 | `		}` |
|   173 |  394 | `		if( c > 0x7F ){` |
|     5 |  395 | `			*pErr = ICV_ILLEGAL_SEQ;` |
|     5 |  396 | `			return 0;` |
|     - |  397 | `		}` |
|   169 |  398 | `		if( *pG0 == ICV_G0_ASCII \|\| c < 0x21 \|\| c == 0x7F ){` |
|     - |  399 | `			/* The controls, the space and DEL sit outside every shifted set` |
|     - |  400 | `			 * and are read as themselves wherever the converter stands. */` |
|    95 |  401 | `			*pCp = c;` |
|    95 |  402 | `			return 1;` |
|     - |  403 | `		}` |
|    75 |  404 | `		if( *pG0 == ICV_G0_ROMAN ){` |
|     9 |  405 | `			*pCp = PH7_JisX0201RomanToUni((int)c);` |
|     9 |  406 | `			return 1;` |
|     - |  407 | `		}` |
|    67 |  408 | `		if( n < 2 ){` |
|     5 |  409 | `			*pErr = ICV_ILLEGAL_CHAR;` |
|     5 |  410 | `			return 0;` |
|     - |  411 | `		}` |
|    63 |  412 | `		cp = PH7_JisX0208ToUni((int)c,(int)z[1]);` |
|    63 |  413 | `		if( cp == 0 ){` |
|     5 |  414 | `			*pErr = ICV_ILLEGAL_SEQ;` |
|     5 |  415 | `			return 0;` |
|     - |  416 | `		}` |
|    59 |  417 | `		*pCp = cp;` |
|    59 |  418 | `		return 2;` |
|     - |  419 | `	}` |
|     - |  420 | `#endif /* PH7_ENABLE_JIS */` |
|  5707 |  421 | `	if( c < 0x80 ){` |
|  4866 |  422 | `		*pCp = c;` |
|  4866 |  423 | `		return 1;` |
|     - |  424 | `	}` |
|  1389 |  425 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(aLead) ; ++k ){` |
|     - |  426 | `		int nSeq;` |
|     - |  427 | `		sxu32 cp;` |
|  1337 |  428 | `		if( c < aLead[k].iLow \|\| c > aLead[k].iHigh ){` |
|   548 |  429 | `			continue;` |
|     - |  430 | `		}` |
|   793 |  431 | `		nSeq = aLead[k].nSeq;` |
|   793 |  432 | `		cp = c & (sxu32)(0x7F >> nSeq);` |
|  1782 |  433 | `		for( i = 1 ; i < nSeq ; ++i ){` |
|  1027 |  434 | `			if( i >= n ){` |
|     - |  435 | `				/* Ran out mid-sequence with everything so far valid: php's` |
|     - |  436 | `				 * "incomplete multibyte character", not its "illegal" one. */` |
|    29 |  437 | `				*pErr = ICV_ILLEGAL_CHAR;` |
|    29 |  438 | `				return 0;` |
|     - |  439 | `			}` |
|   999 |  440 | `			if( (z[i] & 0xC0) != 0x80 ){` |
|     8 |  441 | `				*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  442 | `				return 0;` |
|     - |  443 | `			}` |
|   993 |  444 | `			cp = (cp << 6) \| (sxu32)(z[i] & 0x3F);` |
|   498 |  445 | `		}` |
|   759 |  446 | `		if( cp < aLead[k].iMin \|\| (cp >= 0xD800 && cp <= 0xDFFF) ){` |
|     9 |  447 | `			*pErr = ICV_ILLEGAL_SEQ;   /* overlong, or a lone surrogate */` |
|     9 |  448 | `			return 0;` |
|     - |  449 | `		}` |
|   751 |  450 | `		*pCp = cp;` |
|   751 |  451 | `		return nSeq;` |
|   ! 0 |  452 | `	}` |
|    53 |  453 | `	*pErr = ICV_ILLEGAL_SEQ;` |
|    53 |  454 | `	return 0;` |
|  3667 |  455 | `}` |
|     - |  456 |  |
|     - |  457 | `/*` |
|     - |  458 | ` * Write cp in iEnc when the encoding can hold it; 0 when it cannot. *pG0 is` |
|     - |  459 | ` * the shift state, read and advanced for ISO-2022-JP and untouched by every` |
|     - |  460 | ` * other code set.` |
|     - |  461 | ` */` |
|  6317 |  462 | `static int IcvEncodeDirect(SyBlob *pOut,sxu32 cp,int iEnc,int *pG0)` |
|     4 |  463 | `{` |
|     - |  464 | `	unsigned char zEnc[6];` |
|  6321 |  465 | `	int n = 0;` |
|  6321 |  466 | `	if( iEnc == ICV_ASCII ){` |
|   656 |  467 | `		if( cp > 0x7F ){` |
|   137 |  468 | `			return 0;` |
|     2 |  469 | `		}` |
|  5926 |  470 | `	}else if( iEnc == ICV_LATIN1 ){` |
|   169 |  471 | `		if( cp > 0xFF ){` |
|    25 |  472 | `			return 0;` |
|     2 |  473 | `		}` |
|     - |  474 | `#ifdef PH7_ENABLE_JIS` |
|  5570 |  475 | `	}else if( iEnc == ICV_ISO2022JP ){` |
|     - |  476 | `		int iRow,iCell,iByte;` |
|   146 |  477 | `		if( cp < 0x80 ){` |
|     - |  478 | `			/* An ASCII character is written where it stands when the current` |
|     - |  479 | `			 * set spells it the same way. JIS X 0201 Roman covers 0x21..0x7E` |
|     - |  480 | `			 * and spells all but two of them the same way, so the backslash` |
|     - |  481 | `			 * and the tilde -- the two cells that set moved -- cost a shift` |
|     - |  482 | `			 * back, and so do the controls, the space and DEL, which sit` |
|     - |  483 | `			 * outside every shifted set in this direction too. */` |
|    48 |  484 | `			if( *pG0 == ICV_G0_KANJI` |
|    46 |  485 | `			 \|\| (*pG0 == ICV_G0_ROMAN` |
|    24 |  486 | `			  && (cp < 0x21 \|\| cp == 0x7F \|\| cp == 0x5C \|\| cp == 0x7E)) ){` |
|    19 |  487 | `				SyBlobAppend(pOut,"\033(B",3);` |
|    19 |  488 | `				*pG0 = ICV_G0_ASCII;` |
|     9 |  489 | `			}` |
|    49 |  490 | `			zEnc[0] = (unsigned char)cp;` |
|    49 |  491 | `			SyBlobAppend(pOut,zEnc,1);` |
|    49 |  492 | `			return 1;` |
|     - |  493 | `		}` |
|    98 |  494 | `		if( PH7_JisX0201RomanFromUni(cp,&iByte) ){` |
|     - |  495 | `			/* Only the yen sign and the overline reach here: everything else` |
|     - |  496 | `			 * this set holds is below 0x80 and was answered above. */` |
|    17 |  497 | `			if( *pG0 != ICV_G0_ROMAN ){` |
|    17 |  498 | `				SyBlobAppend(pOut,"\033(J",3);` |
|    17 |  499 | `				*pG0 = ICV_G0_ROMAN;` |
|     8 |  500 | `			}` |
|    17 |  501 | `			zEnc[0] = (unsigned char)iByte;` |
|    17 |  502 | `			SyBlobAppend(pOut,zEnc,1);` |
|    17 |  503 | `			return 1;` |
|     - |  504 | `		}` |
|    82 |  505 | `		if( PH7_JisX0208FromUni(cp,&iRow,&iCell) ){` |
|    67 |  506 | `			if( *pG0 != ICV_G0_KANJI ){` |
|    43 |  507 | `				SyBlobAppend(pOut,"\033$B",3);` |
|    43 |  508 | `				*pG0 = ICV_G0_KANJI;` |
|    21 |  509 | `			}` |
|    67 |  510 | `			zEnc[0] = (unsigned char)iRow;` |
|    67 |  511 | `			zEnc[1] = (unsigned char)iCell;` |
|    67 |  512 | `			SyBlobAppend(pOut,zEnc,2);` |
|    67 |  513 | `			return 1;` |
|     - |  514 | `		}` |
|     - |  515 | `		/* The halfwidth katakana land here, and so does everything JIS X 0208` |
|     - |  516 | `		 * has no cell for. */` |
|    16 |  517 | `		return 0;` |
|     - |  518 | `#endif /* PH7_ENABLE_JIS */` |
|   ! 0 |  519 | `	}else{` |
|     - |  520 | `		/* The six-byte encoding, so every code point IcvDecode() produced can` |
|     - |  521 | `		 * be written back. */` |
|  5355 |  522 | `		if( cp < 0x80 ){` |
|  4935 |  523 | `			zEnc[n++] = (unsigned char)cp;` |
|  2888 |  524 | `		}else if( cp < 0x800 ){` |
|   374 |  525 | `			zEnc[n++] = (unsigned char)(0xC0 \| (cp >> 6));` |
|   374 |  526 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   236 |  527 | `		}else if( cp < 0x10000 ){` |
|    39 |  528 | `			zEnc[n++] = (unsigned char)(0xE0 \| (cp >> 12));` |
|    39 |  529 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|    39 |  530 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    30 |  531 | `		}else if( cp < 0x200000 ){` |
|     7 |  532 | `			zEnc[n++] = (unsigned char)(0xF0 \| (cp >> 18));` |
|     7 |  533 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     7 |  534 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     7 |  535 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     8 |  536 | `		}else if( cp < 0x4000000 ){` |
|     3 |  537 | `			zEnc[n++] = (unsigned char)(0xF8 \| (cp >> 24));` |
|     3 |  538 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  539 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  540 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  541 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     2 |  542 | `		}else{` |
|     3 |  543 | `			zEnc[n++] = (unsigned char)(0xFC \| (cp >> 30));` |
|     3 |  544 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 24) & 0x3F));` |
|     3 |  545 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  546 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  547 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  548 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     - |  549 | `		}` |
|  5355 |  550 | `		SyBlobAppend(pOut,zEnc,(sxu32)n);` |
|  5355 |  551 | `		return 1;` |
|     - |  552 | `	}` |
|   663 |  553 | `	zEnc[0] = (unsigned char)cp;` |
|   663 |  554 | `	SyBlobAppend(pOut,zEnc,1);` |
|   663 |  555 | `	return 1;` |
|  3161 |  556 | `}` |
|     - |  557 |  |
|     - |  558 | `/*` |
|     - |  559 | ` * Return a stateful target to ASCII. Every byte a caller is about to write` |
|     - |  560 | ` * outside IcvEncodeDirect() -- a transliteration, the '?' a miss produces, the` |
|     - |  561 | ` * tail of a finished string -- is ASCII, and ISO-2022-JP cannot carry one` |
|     - |  562 | ` * while it stands in another set.` |
|     - |  563 | ` */` |
|  1643 |  564 | `static void IcvEncodeShiftAscii(SyBlob *pOut,int iEnc,int *pG0)` |
|     4 |  565 | `{` |
|     - |  566 | `#ifdef PH7_ENABLE_JIS` |
|  1647 |  567 | `	if( iEnc == ICV_ISO2022JP && *pG0 != ICV_G0_ASCII ){` |
|    39 |  568 | `		SyBlobAppend(pOut,"\033(B",3);` |
|    39 |  569 | `		*pG0 = ICV_G0_ASCII;` |
|    19 |  570 | `	}` |
|     - |  571 | `#else` |
|     - |  572 | `	SXUNUSED(pOut); SXUNUSED(iEnc); SXUNUSED(pG0);` |
|     - |  573 | `#endif` |
|  1647 |  574 | `}` |
|     - |  575 |  |
|     - |  576 | `/*` |
|     - |  577 | ` * Write cp in iEnc, transliterating when bTranslit is set. Answers 0 only when` |
|     - |  578 | ` * the encoding cannot hold cp and transliteration is off -- the '?' a` |
|     - |  579 | ` * transliterated miss produces is glibc's own answer, and it is a SUCCESS, as` |
|     - |  580 | ` * is the EMPTY replacement a combining mark transliterates to.` |
|     - |  581 | ` */` |
|  6305 |  582 | `static int IcvEncode(SyBlob *pOut,sxu32 cp,int iEnc,int bTranslit,int *pG0)` |
|     4 |  583 | `{` |
|     - |  584 | `	const icv_translit *pRow;` |
|  6309 |  585 | `	if( IcvEncodeDirect(pOut,cp,iEnc,pG0) ){` |
|  6134 |  586 | `		return 1;` |
|     - |  587 | `	}` |
|   177 |  588 | `	if( !bTranslit ){` |
|    56 |  589 | `		return 0;` |
|     - |  590 | `	}` |
|     - |  591 | `	/* Everything below writes bytes of its own, and they are ASCII. */` |
|   123 |  592 | `	IcvEncodeShiftAscii(pOut,iEnc,pG0);` |
|   120 |  593 | `	if( iEnc == ICV_LATIN1` |
|    70 |  594 | `	 && (pRow = IcvTranslitFind(aIcvTranslit1,(int)SX_ARRAYSIZE(aIcvTranslit1),cp)) != 0 ){` |
|    11 |  595 | `		SyBlobAppend(pOut,&zIcvTranslit1[pRow->iOfft],pRow->nByte);` |
|    11 |  596 | `		return 1;` |
|     - |  597 | `	}` |
|   113 |  598 | `	pRow = IcvTranslitFind(aIcvTranslit,(int)SX_ARRAYSIZE(aIcvTranslit),cp);` |
|   113 |  599 | `	if( pRow ){` |
|   100 |  600 | `		SyBlobAppend(pOut,&zIcvTranslit[pRow->iOfft],pRow->nByte);` |
|   100 |  601 | `		return 1;` |
|     - |  602 | `	}` |
|    14 |  603 | `	SyBlobAppend(pOut,"?",1);` |
|    14 |  604 | `	return 1;` |
|  3155 |  605 | `}` |
|     - |  606 |  |
|     - |  607 | `/*` |
|     - |  608 | ` * One iconv(3) CALL over zIn[*pi..nIn-1]: convert until the input runs out or` |
|     - |  609 | ` * the library would stop, leaving *pi where it stopped and answering the status` |
|     - |  610 | ` * php's loop then reads.` |
|     - |  611 | ` *` |
|     - |  612 | ` * There are TWO ignore mechanisms and they are not the same one. The LIBRARY's` |
|     - |  613 | `` * (`pTo->bIgnore`, an IGNORE token anywhere in the error-handler list) drops a`` |
|     - |  614 | ` * character the TARGET cannot hold and carries on, and reports the whole call` |
|     - |  615 | ` * as an illegal sequence AFTERWARDS -- so on its own it only ever turns one` |
|     - |  616 | `` * failure into another, which is why `iconv("UTF-8","ASCII//IGNORE,X",…)` is`` |
|     - |  617 | `` * still FALSE. php's own (`bIgnore` in IcvConvert()) is what makes //IGNORE`` |
|     - |  618 | ` * useful, and it lives one level up.` |
|     - |  619 | ` *` |
|     - |  620 | ` * *pbDropped is that afterwards, and it is STICKY across the calls php makes:` |
|     - |  621 | ` * once a character has been dropped because the target could not hold it, a` |
|     - |  622 | ` * TRUNCATED tail is reported as an illegal sequence rather than an incomplete` |
|     - |  623 | ` * character -- which is what lets` |
|     - |  624 | `` * `iconv("UTF-8","ISO-8859-1//IGNORE","\xE1\x88\xA2ab\xFD")` answer "ab"`` |
|     - |  625 | ` * while the same string without the unconvertible character in front of it is a` |
|     - |  626 | ` * hard failure.` |
|     - |  627 | ` */` |
|  1403 |  628 | `static int IcvConvertCall(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  629 | `	int iFrom,const icv_cs *pTo,int *pbDropped,int *pG0In,int *pG0Out)` |
|     4 |  630 | `{` |
|  1407 |  631 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  1407 |  632 | `	int i = *pi,rc = ICV_OK;` |
|  3858 |  633 | `	while( i < nIn ){` |
|  2565 |  634 | `		sxu32 cp = 0;` |
|  2565 |  635 | `		int err = ICV_OK;` |
|  2565 |  636 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err,pG0In);` |
|  2565 |  637 | `		if( nSeq == 0 ){` |
|    86 |  638 | `			rc = err;` |
|    99 |  639 | `			break;` |
|     - |  640 | `		}` |
|  2481 |  641 | `		if( cp == ICV_NOCHAR ){` |
|     - |  642 | `			/* A shift: it moved the source's state and produced nothing. */` |
|    55 |  643 | `			i += nSeq;` |
|  1255 |  644 | `			continue;` |
|     - |  645 | `		}` |
|  2427 |  646 | `		if( IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit,pG0Out) ){` |
|  2375 |  647 | `			i += nSeq;` |
|  2375 |  648 | `			continue;` |
|     - |  649 | `		}` |
|    54 |  650 | `		if( pTo->bIgnore ){` |
|    28 |  651 | `			i += nSeq;` |
|    28 |  652 | `			*pbDropped = 1;` |
|    28 |  653 | `			continue;` |
|     - |  654 | `		}` |
|    28 |  655 | `		rc = ICV_ILLEGAL_SEQ;` |
|    28 |  656 | `		break;` |
|   ! 0 |  657 | `	}` |
|  1407 |  658 | `	if( *pbDropped && (rc == ICV_OK \|\| rc == ICV_ILLEGAL_CHAR) ){` |
|    18 |  659 | `		rc = ICV_ILLEGAL_SEQ;` |
|     8 |  660 | `	}` |
|  1407 |  661 | `	*pi = i;` |
|  1407 |  662 | `	return rc;` |
|     4 |  663 | `}` |
|     - |  664 |  |
|     - |  665 | `/*` |
|     - |  666 | ` * The whole of php_iconv_string(): drive IcvConvertCall() the way php's loop` |
|     - |  667 | `` * drives iconv(3). php's own `//IGNORE` handling sits HERE and is byte-granular`` |
|     - |  668 | ` * -- on an illegal sequence it steps the input forward by one byte and converts` |
|     - |  669 | ` * again, and a failure with a single byte left is taken for a success.` |
|     - |  670 | ` */` |
|  1387 |  671 | `static int IcvConvert(SyBlob *pOut,const char *zIn,int nIn,int iFrom,const icv_cs *pTo,int bIgnore)` |
|     4 |  672 | `{` |
|  1391 |  673 | `	int i = 0,bDropped = 0;` |
|     - |  674 | `	/* Both states live across the whole string and across every call the` |
|     - |  675 | `	 * //IGNORE loop makes: php reuses one conversion descriptor, so stepping` |
|     - |  676 | `	 * over a bad byte does not put the converter back in ASCII. */` |
|  1391 |  677 | `	int iG0In = ICV_G0_ASCII,iG0Out = ICV_G0_ASCII;` |
|   701 |  678 | `	for(;;){` |
|  1407 |  679 | `		int rc = IcvConvertCall(pOut,zIn,nIn,&i,iFrom,pTo,&bDropped,&iG0In,&iG0Out);` |
|  1407 |  680 | `		if( rc == ICV_OK ){` |
|  1285 |  681 | `			IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);` |
|  1285 |  682 | `			return ICV_OK;` |
|     - |  683 | `		}` |
|   124 |  684 | `		if( bIgnore && rc == ICV_ILLEGAL_SEQ ){` |
|    34 |  685 | `			if( nIn - i <= 1 ){` |
|    18 |  686 | `				IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);` |
|    18 |  687 | `				return ICV_OK;` |
|     - |  688 | `			}` |
|    17 |  689 | `			i++;` |
|    17 |  690 | `			continue;` |
|     - |  691 | `		}` |
|    92 |  692 | `		return rc;` |
|   ! 0 |  693 | `	}` |
|   697 |  694 | `}` |
|     - |  695 |  |
|     - |  696 | `/*` |
|     - |  697 | ` * Convert from zIn[*pi..] until the OUTPUT would pass iRoom bytes, leaving *pi` |
|     - |  698 | ` * on a character boundary and *pnTook set to how many characters went. This is` |
|     - |  699 | ` * how the MIME encoder fills one encoded word: it has a byte budget for the` |
|     - |  700 | ` * line and has to stop on a whole character, which is exactly what iconv(3)` |
|     - |  701 | ` * does when its output buffer runs out.` |
|     - |  702 | ` */` |
|   224 |  703 | `static int IcvConvertBounded(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  704 | `	int iFrom,const icv_cs *pTo,sxi64 iRoom,int *pnTook)` |
|     2 |  705 | `{` |
|   226 |  706 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   226 |  707 | `	int i = *pi,nTook = 0;` |
|   226 |  708 | `	int iG0In = ICV_G0_ASCII,iG0Out = ICV_G0_ASCII;` |
|   226 |  709 | `	if( pTo->iEnc == ICV_ISO2022JP ){` |
|     - |  710 | `		/* The word has to end back in ASCII, so those three bytes are spoken` |
|     - |  711 | `		 * for before the first character is written. */` |
|     3 |  712 | `		iRoom -= 3;` |
|     1 |  713 | `	}` |
|  4026 |  714 | `	while( i < nIn ){` |
|  3886 |  715 | `		sxu32 cp = 0;` |
|  3886 |  716 | `		int err = ICV_OK;` |
|  3886 |  717 | `		sxu32 nBefore = SyBlobLength(pOut);` |
|  3886 |  718 | `		int iG0Before = iG0Out;` |
|  3886 |  719 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err,&iG0In);` |
|  3886 |  720 | `		if( nSeq == 0 ){` |
|     3 |  721 | `			*pi = i;` |
|     3 |  722 | `			*pnTook = nTook;` |
|     4 |  723 | `			return err;` |
|     - |  724 | `		}` |
|  3884 |  725 | `		if( cp == ICV_NOCHAR ){` |
|   ! 0 |  726 | `			i += nSeq;` |
|   ! 0 |  727 | `			continue;` |
|     - |  728 | `		}` |
|  3884 |  729 | `		if( !IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit,&iG0Out) ){` |
|     3 |  730 | `			*pi = i;` |
|     3 |  731 | `			*pnTook = nTook;` |
|     3 |  732 | `			return ICV_ILLEGAL_SEQ;` |
|     - |  733 | `		}` |
|  3882 |  734 | `		if( (sxi64)SyBlobLength(pOut) > iRoom ){` |
|     - |  735 | `			/* One character too many: give the bytes back and stop here. */` |
|    81 |  736 | `			SyBlobLength(pOut) = nBefore;` |
|    81 |  737 | `			iG0Out = iG0Before;` |
|    81 |  738 | `			break;` |
|     - |  739 | `		}` |
|  3802 |  740 | `		i += nSeq;` |
|  3802 |  741 | `		nTook++;` |
|     2 |  742 | `	}` |
|   222 |  743 | `	IcvEncodeShiftAscii(pOut,pTo->iEnc,&iG0Out);` |
|   222 |  744 | `	*pi = i;` |
|   222 |  745 | `	*pnTook = nTook;` |
|   222 |  746 | `	return ICV_OK;` |
|   114 |  747 | `}` |
|     - |  748 |  |
|     - |  749 | `/*` |
|     - |  750 | ` * One buffer converted between two charsets NAMED the way a MO catalog's` |
|     - |  751 | `` * `Content-Type` header names them: ext/gettext's whole conversion door.`` |
|     - |  752 | ` *` |
|     - |  753 | ` * Transliteration is on unconditionally because glibc's gettext opens its own` |
|     - |  754 | `` * conversion that way (`norm_add_slashes (outcharset, "TRANSLIT")` in`` |
|     - |  755 | ` * loadmsgcat.c), which is why a translated string a narrower target cannot hold` |
|     - |  756 | ` * comes back approximated rather than refused. Answers -1 when either name is` |
|     - |  757 | ` * outside the code sets PHL models, which is the case php answers the` |
|     - |  758 | ` * msgid UNTRANSLATED for -- the same thing its iconv_open() failure does.` |
|     - |  759 | ` */` |
|    48 |  760 | `PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,` |
|     - |  761 | `	const char *zFrom,int nFrom,const char *zTo,int nTo)` |
|     2 |  762 | `{` |
|     - |  763 | `	icv_cs sFrom,sTo;` |
|    50 |  764 | `	if( nFrom < 1 \|\| nTo < 1 ){` |
|   ! 0 |  765 | `		return -1;` |
|     - |  766 | `	}` |
|    50 |  767 | `	IcvParseCharset(zFrom,nFrom,&sFrom);` |
|    50 |  768 | `	IcvParseCharset(zTo,nTo,&sTo);` |
|    50 |  769 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|     2 |  770 | `		return -1;` |
|     - |  771 | `	}` |
|    49 |  772 | `	if( sFrom.iEnc == sTo.iEnc ){` |
|     - |  773 | `		/* Same code set: glibc's conversion is a copy, and so is this. */` |
|    48 |  774 | `		SyBlobAppend(pOut,zIn,(sxu32)nIn);` |
|    48 |  775 | `		return PH7_OK;` |
|     - |  776 | `	}` |
|     2 |  777 | `	sTo.bTranslit = 1;` |
|     2 |  778 | `	sTo.bIgnore = 0;` |
|     2 |  779 | `	return IcvConvert(pOut,zIn,nIn,sFrom.iEnc,&sTo,0) == ICV_OK ? PH7_OK : -1;` |
|     9 |  780 | `}` |
|     - |  781 |  |
|     - |  782 | `/* --- Diagnostics ------------------------------------------------------- */` |
|     - |  783 |  |
|     - |  784 | `/*` |
|     - |  785 | `` * php's `_php_iconv_show_error()`. The context prefixes the function name, so`` |
|     - |  786 | ` * these are the message bodies only; the two decoder diagnostics are E_NOTICE` |
|     - |  787 | ` * and everything else E_WARNING, which is php's own split.` |
|     - |  788 | ` */` |
|   224 |  789 | `static void IcvShowError(ph7_context *pCtx,int err,const char *zTo,int nTo,` |
|     - |  790 | `	const char *zFrom,int nFrom)` |
|     5 |  791 | `{` |
|   229 |  792 | `	switch( err ){` |
|   ! 0 |  793 | `	case ICV_OK:` |
|   ! 0 |  794 | `		break;` |
|    32 |  795 | `	case ICV_WRONG_CHARSET:` |
|    98 |  796 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  797 | `			"Wrong encoding, conversion from \"%.*s\" to \"%.*s\" is not allowed",` |
|    32 |  798 | `			nFrom,zFrom,nTo,zTo);` |
|    66 |  799 | `		break;` |
|    16 |  800 | `	case ICV_ILLEGAL_CHAR:` |
|    34 |  801 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  802 | `			"Detected an incomplete multibyte character in input string");` |
|    34 |  803 | `		break;` |
|    41 |  804 | `	case ICV_ILLEGAL_SEQ:` |
|    85 |  805 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  806 | `			"Detected an illegal character in input string");` |
|    85 |  807 | `		break;` |
|     4 |  808 | `	case ICV_TOO_BIG:` |
|     9 |  809 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Buffer length exceeded");` |
|     9 |  810 | `		break;` |
|    16 |  811 | `	case ICV_MALFORMED:` |
|    33 |  812 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Malformed string");` |
|    33 |  813 | `		break;` |
|     3 |  814 | `	case ICV_UNKNOWN_ERR:` |
|     - |  815 | ``		/* php prints `errno` here, and nothing has SET it -- the number is`` |
|     - |  816 | `		 * whatever the last libc call in the process left behind (22 and 84` |
|     - |  817 | `		 * are what the same input produces on this box, in the same run). PHL` |
|     - |  818 | `		 * has no stale errno to leak, so it says 0; twin-paired. */` |
|     7 |  819 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Unknown error (0)");` |
|     6 |  820 | `		break;` |
|   ! 0 |  821 | `	default:` |
|   ! 0 |  822 | `		break;` |
|     - |  823 | `	}` |
|   229 |  824 | `}` |
|     - |  825 |  |
|     - |  826 | `/*` |
|     - |  827 | ` * Read one charset ARGUMENT: php's length cap first (it is checked before the` |
|     - |  828 | ` * name is looked at, so an over-long name never reaches the "Wrong encoding"` |
|     - |  829 | ` * message), then the grammar. Answers 0 after raising the length warning.` |
|     - |  830 | ` */` |
|  1254 |  831 | `static int IcvCharsetArg(ph7_context *pCtx,const char *z,int n,icv_cs *pCs)` |
|     5 |  832 | `{` |
|  1259 |  833 | `	if( n >= ICV_CSNMAXLEN ){` |
|    11 |  834 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  835 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - |  836 | `			ICV_CSNMAXLEN);` |
|    11 |  837 | `		return 0;` |
|     - |  838 | `	}` |
|  1249 |  839 | `	IcvParseCharset(z,n,pCs);` |
|  1249 |  840 | `	return 1;` |
|   632 |  841 | `}` |
|     - |  842 |  |
|     - |  843 | `/* --- The functions ----------------------------------------------------- */` |
|     - |  844 |  |
|     - |  845 | `/* string\|false iconv(string $from_encoding, string $to_encoding, string $string) */` |
|   440 |  846 | `static int PH7_builtin_iconv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 |  847 | `{` |
|     - |  848 | `	const char *zFrom,*zTo,*zIn;` |
|     - |  849 | `	int nFrom,nTo,nIn,err;` |
|     - |  850 | `	icv_cs sFrom,sTo;` |
|     - |  851 | `	SyBlob sOut;` |
|   443 |  852 | `	if( nArg < 3 ){` |
|   ! 0 |  853 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  854 | `		return PH7_OK;` |
|     - |  855 | `	}` |
|   443 |  856 | `	zFrom = ph7_value_to_string(apArg[0],&nFrom);` |
|   443 |  857 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|   443 |  858 | `	zIn = ph7_value_to_string(apArg[2],&nIn);` |
|   443 |  859 | `	if( !IcvCharsetArg(pCtx,zFrom,nFrom,&sFrom) \|\| !IcvCharsetArg(pCtx,zTo,nTo,&sTo) ){` |
|     5 |  860 | `		ph7_result_bool(pCtx,0);` |
|     5 |  861 | `		return PH7_OK;` |
|     - |  862 | `	}` |
|   439 |  863 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|    62 |  864 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,` |
|    20 |  865 | `			zTo,IcvNameLen(zTo,nTo),zFrom,IcvNameLen(zFrom,nFrom));` |
|    42 |  866 | `		ph7_result_bool(pCtx,0);` |
|    42 |  867 | `		return PH7_OK;` |
|     - |  868 | `	}` |
|   399 |  869 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   399 |  870 | `	err = IcvConvert(&sOut,zIn,nIn,sFrom.iEnc,&sTo,IcvCheckIgnore(zTo,nTo));` |
|   399 |  871 | `	if( err != ICV_OK ){` |
|    88 |  872 | `		SyBlobRelease(&sOut);` |
|    88 |  873 | `		IcvShowError(pCtx,err,zTo,nTo,zFrom,nFrom);` |
|    88 |  874 | `		ph7_result_bool(pCtx,0);` |
|    88 |  875 | `		return PH7_OK;` |
|     - |  876 | `	}` |
|   313 |  877 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   313 |  878 | `	SyBlobRelease(&sOut);` |
|   313 |  879 | `	return PH7_OK;` |
|   223 |  880 | `}` |
|     - |  881 |  |
|     - |  882 |  |
|     - |  883 | `/* --- The string quartet ------------------------------------------------ */` |
|     - |  884 |  |
|     - |  885 | `/*` |
|     - |  886 | ` * php's own name for the wide intermediate every one of these four converts` |
|     - |  887 | ` * THROUGH, and the one that shows up in their "Wrong encoding" message: the` |
|     - |  888 | ` * conversion that fails is the one INTO it, so the message always reads` |
|     - |  889 | `` * `from "<the argument>" to "UCS-4LE"`.`` |
|     - |  890 | ` */` |
|     - |  891 | `#define ICV_SUPERSET "UCS-4LE"` |
|     - |  892 |  |
|     - |  893 | `/*` |
|     - |  894 | `` * The `?string $encoding = null` the string family shares. A missing or null`` |
|     - |  895 | ``  * argument is php's INTERNAL encoding, which is `iconv.internal_encoding` `` |
|     - |  896 | `` * falling back to `default_charset` -- and since the scope policy removes the deprecated`` |
|     - |  897 | `` * `iconv.*` directives, `default_charset` is the whole of it here.`` |
|     - |  898 | ` *` |
|     - |  899 | ` * Only php's LENGTH cap is a diagnostic at this point (answers 0 for it). An` |
|     - |  900 | ` * unknown NAME is not, because php does not learn of one until it opens a` |
|     - |  901 | `` * conversion -- which is why `iconv_strpos($h, "", 0, "NOPE")` is a silent`` |
|     - |  902 | ` * false while the same call with a needle warns. IcvEncReady() is that moment.` |
|     - |  903 | ` */` |
|   376 |  904 | `static int IcvStrEncArg(ph7_context *pCtx,ph7_value *pArg,icv_cs *pCs)` |
|     5 |  905 | `{` |
|     - |  906 | `	SyBlob sIni;` |
|     - |  907 | `	int rc;` |
|   381 |  908 | `	if( pArg != 0 && !ph7_value_is_null(pArg) ){` |
|     - |  909 | `		int nEnc;` |
|    55 |  910 | `		const char *zEnc = ph7_value_to_string(pArg,&nEnc);` |
|    55 |  911 | `		return IcvCharsetArg(pCtx,zEnc,nEnc,pCs);` |
|     - |  912 | `	}` |
|   328 |  913 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|   328 |  914 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|   328 |  915 | `	rc = IcvCharsetArg(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni),pCs);` |
|   328 |  916 | `	SyBlobRelease(&sIni);` |
|   328 |  917 | `	return rc;` |
|   193 |  918 | `}` |
|     - |  919 | `/*` |
|     - |  920 | ` * The "Wrong encoding" php raises where it would have opened the conversion.` |
|     - |  921 | ` * The conversion the string family opens is the one INTO the wide intermediate,` |
|     - |  922 | ` * so the message always names UCS-4LE as the target. Answers 0 after raising.` |
|     - |  923 | ` */` |
|   162 |  924 | `static int IcvEncReady(ph7_context *pCtx,const icv_cs *pCs)` |
|     4 |  925 | `{` |
|   166 |  926 | `	if( pCs->iEnc >= 0 ){` |
|   160 |  927 | `		return 1;` |
|     - |  928 | `	}` |
|    10 |  929 | `	IcvShowError(pCtx,ICV_WRONG_CHARSET,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,` |
|     6 |  930 | `		pCs->zName,pCs->nName);` |
|     7 |  931 | `	return 0;` |
|    85 |  932 | `}` |
|     - |  933 |  |
|     - |  934 | `/*` |
|     - |  935 | ` * A decoded string: one code point per character plus the byte offset each one` |
|     - |  936 | ` * starts at (nChar+1 entries, so the last is the buffer length). php works the` |
|     - |  937 | ` * same way -- it converts to UCS-4 and operates there -- and it is what lets a` |
|     - |  938 | ` * search answer in CHARACTERS while a slice is taken in BYTES.` |
|     - |  939 | ` */` |
|     - |  940 | `typedef struct icv_text icv_text;` |
|     - |  941 | `struct icv_text {` |
|     - |  942 | `	const char *zIn;` |
|     - |  943 | `	int nByte;` |
|     - |  944 | `	sxu32 *aCode;` |
|     - |  945 | `	int *aOfft;` |
|     - |  946 | `	int nChar;` |
|     - |  947 | `};` |
|     - |  948 | `#define ICV_TEXT_MAX 0x0FFFFFFF` |
|     - |  949 |  |
|     - |  950 | `/*` |
|     - |  951 | ` * Decode zIn under iEnc into pText. Answers PH7_OK, or PH7_OK with *pErr set to` |
|     - |  952 | ` * the diagnostic the input earns -- in which case pText is not usable but was` |
|     - |  953 | ` * still allocated, so IcvTextRelease() is safe either way.` |
|     - |  954 | ` */` |
|   212 |  955 | `static int IcvTextDecode(ph7_context *pCtx,icv_text *pText,const char *zIn,int nByte,` |
|     - |  956 | `	int iEnc,int *pErr)` |
|     4 |  957 | `{` |
|   216 |  958 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   216 |  959 | `	int i = 0,n = 0,nSlot,iG0 = ICV_G0_ASCII;` |
|   216 |  960 | `	pText->zIn = zIn;` |
|   216 |  961 | `	pText->nByte = nByte;` |
|   216 |  962 | `	pText->nChar = 0;` |
|   216 |  963 | `	pText->aCode = 0;` |
|   216 |  964 | `	pText->aOfft = 0;` |
|   216 |  965 | `	*pErr = ICV_OK;` |
|   216 |  966 | `	if( nByte > ICV_TEXT_MAX ){` |
|   ! 0 |  967 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  968 | `	}` |
|   216 |  969 | `	nSlot = nByte + 1;` |
|   428 |  970 | `	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|   212 |  971 | `		(unsigned int)((sxu32)nSlot * (sizeof(sxu32) + sizeof(int))),FALSE,TRUE);` |
|   216 |  972 | `	if( pText->aCode == 0 ){` |
|   ! 0 |  973 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  974 | `	}` |
|   216 |  975 | `	pText->aOfft = (int *)&pText->aCode[nSlot];` |
|  1068 |  976 | `	while( i < nByte ){` |
|   886 |  977 | `		sxu32 cp = 0;` |
|   886 |  978 | `		int nSeq = IcvDecode(&z[i],nByte - i,iEnc,&cp,pErr,&iG0);` |
|   886 |  979 | `		if( nSeq == 0 ){` |
|     - |  980 | `			/* The good PREFIX is the answer here, not zero: php's own walk` |
|     - |  981 | `			 * stops at the bad character and everything before it has already` |
|     - |  982 | `			 * been converted, so nChar is how far a search got and the count` |
|     - |  983 | `			 * an out-of-bounds $offset is measured against. */` |
|    32 |  984 | `			pText->aOfft[n] = i;` |
|    32 |  985 | `			pText->nChar = n;` |
|    32 |  986 | `			return PH7_OK;` |
|     - |  987 | `		}` |
|   856 |  988 | `		if( cp == ICV_NOCHAR ){` |
|     - |  989 | `			/* A shift is not a character: it is not counted, and no offset` |
|     - |  990 | `			 * points at it. */` |
|    25 |  991 | `			i += nSeq;` |
|    25 |  992 | `			continue;` |
|     - |  993 | `		}` |
|   832 |  994 | `		pText->aCode[n] = cp;` |
|   832 |  995 | `		pText->aOfft[n] = i;` |
|   832 |  996 | `		i += nSeq;` |
|   832 |  997 | `		n++;` |
|     4 |  998 | `	}` |
|   186 |  999 | `	pText->aOfft[n] = nByte;` |
|   186 | 1000 | `	pText->nChar = n;` |
|   186 | 1001 | `	return PH7_OK;` |
|   110 | 1002 | `}` |
|   212 | 1003 | `static void IcvTextRelease(ph7_context *pCtx,icv_text *pText)` |
|     4 | 1004 | `{` |
|   216 | 1005 | `	if( pText->aCode ){` |
|   216 | 1006 | `		ph7_context_free_chunk(pCtx,pText->aCode);` |
|   216 | 1007 | `		pText->aCode = 0;` |
|   106 | 1008 | `	}` |
|   216 | 1009 | `}` |
|     - | 1010 |  |
|     - | 1011 | `/* int\|false iconv_strlen(string $string, ?string $encoding = null) */` |
|    40 | 1012 | `static int PH7_builtin_iconv_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1013 | `{` |
|     - | 1014 | `	const char *zIn;` |
|    43 | 1015 | `	int nIn,err = ICV_OK;` |
|     - | 1016 | `	icv_cs sCs;` |
|     - | 1017 | `	icv_text sText;` |
|    40 | 1018 | `	if( nArg < 1 \|\| !IcvStrEncArg(pCtx,nArg > 1 ? apArg[1] : 0,&sCs)` |
|    42 | 1019 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     5 | 1020 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1021 | `		return PH7_OK;` |
|     - | 1022 | `	}` |
|    39 | 1023 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    39 | 1024 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1025 | `		return PH7_OK;` |
|     - | 1026 | `	}` |
|    39 | 1027 | `	IcvTextRelease(pCtx,&sText);` |
|    39 | 1028 | `	if( err != ICV_OK ){` |
|    12 | 1029 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|    12 | 1030 | `		ph7_result_bool(pCtx,0);` |
|    12 | 1031 | `		return PH7_OK;` |
|     - | 1032 | `	}` |
|    29 | 1033 | `	ph7_result_int(pCtx,sText.nChar);` |
|    29 | 1034 | `	return PH7_OK;` |
|    23 | 1035 | `}` |
|     - | 1036 |  |
|     - | 1037 | `/*` |
|     - | 1038 | ` * string\|false iconv_substr(string $string, int $offset, ?int $length = null,` |
|     - | 1039 | ` *                           ?string $encoding = null)` |
|     - | 1040 | ` *` |
|     - | 1041 | ` * php clamps in the order its own code does, and the order is visible: a null` |
|     - | 1042 | ` * $length starts life as the string's BYTE count and is then clamped to the` |
|     - | 1043 | ` * character count, so it can never reach past the end however the two differ.` |
|     - | 1044 | ` */` |
|    58 | 1045 | `static int PH7_builtin_iconv_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     4 | 1046 | `{` |
|     - | 1047 | `	const char *zIn;` |
|    62 | 1048 | `	int nIn,err = ICV_OK,iStart,iStop;` |
|     - | 1049 | `	sxi64 iOfft,iLen;` |
|     - | 1050 | `	icv_cs sCs;` |
|     - | 1051 | `	icv_text sText;` |
|    58 | 1052 | `	if( nArg < 2 \|\| !IcvStrEncArg(pCtx,nArg > 3 ? apArg[3] : 0,&sCs)` |
|    62 | 1053 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     3 | 1054 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1055 | `		return PH7_OK;` |
|     - | 1056 | `	}` |
|    60 | 1057 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    60 | 1058 | `	if( PH7_IntArgResolve(pCtx,apArg[1],"iconv_substr",2,"$offset","int",&iOfft) != PH7_OK ){` |
|   ! 0 | 1059 | `		return PH7_OK;` |
|     - | 1060 | `	}` |
|    60 | 1061 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    39 | 1062 | `		if( PH7_IntArgResolve(pCtx,apArg[2],"iconv_substr",3,"$length","?int",&iLen) != PH7_OK ){` |
|   ! 0 | 1063 | `			return PH7_OK;` |
|     - | 1064 | `		}` |
|    21 | 1065 | `	}else{` |
|    22 | 1066 | `		iLen = nIn;` |
|     - | 1067 | `	}` |
|    60 | 1068 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1069 | `		return PH7_OK;` |
|     - | 1070 | `	}` |
|    60 | 1071 | `	if( err != ICV_OK ){` |
|     5 | 1072 | `		IcvTextRelease(pCtx,&sText);` |
|     5 | 1073 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1074 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1075 | `		return PH7_OK;` |
|     - | 1076 | `	}` |
|    56 | 1077 | `	if( iOfft < 0 ){` |
|    11 | 1078 | `		iOfft += sText.nChar;` |
|    11 | 1079 | `		if( iOfft < 0 ){` |
|     7 | 1080 | `			iOfft = 0;` |
|     4 | 1081 | `		}` |
|    51 | 1082 | `	}else if( iOfft > sText.nChar ){` |
|     5 | 1083 | `		iOfft = sText.nChar;` |
|     2 | 1084 | `	}` |
|    56 | 1085 | `	if( iLen < 0 ){` |
|     9 | 1086 | `		iLen += sText.nChar - iOfft;` |
|     9 | 1087 | `		if( iLen < 0 ){` |
|     5 | 1088 | `			iLen = 0;` |
|     3 | 1089 | `		}` |
|    52 | 1090 | `	}else if( iLen > sText.nChar ){` |
|    23 | 1091 | `		iLen = sText.nChar;` |
|    11 | 1092 | `	}` |
|    56 | 1093 | `	if( iOfft + iLen > sText.nChar ){` |
|    16 | 1094 | `		iLen = sText.nChar - iOfft;` |
|     7 | 1095 | `	}` |
|     - | 1096 | `#ifdef PH7_ENABLE_JIS` |
|    56 | 1097 | `	if( sCs.iEnc == ICV_ISO2022JP ){` |
|     - | 1098 | `		/* A slice of a SHIFTED encoding is not a slice of its bytes. php goes` |
|     - | 1099 | `		 * through code points and back, so the answer carries the shifts it` |
|     - | 1100 | `		 * needs of its own and none of the ones it inherited -- a substring` |
|     - | 1101 | `		 * that starts inside a kanji run opens with the shift into it, and one` |
|     - | 1102 | `		 * that ends there closes with the shift back out. */` |
|     - | 1103 | `		SyBlob sOut;` |
|     7 | 1104 | `		int k,iG0 = ICV_G0_ASCII;` |
|     7 | 1105 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    19 | 1106 | `		for( k = 0 ; k < (int)iLen ; ++k ){` |
|    13 | 1107 | `			IcvEncodeDirect(&sOut,sText.aCode[iOfft + k],sCs.iEnc,&iG0);` |
|     7 | 1108 | `		}` |
|     7 | 1109 | `		IcvEncodeShiftAscii(&sOut,sCs.iEnc,&iG0);` |
|     7 | 1110 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     7 | 1111 | `		SyBlobRelease(&sOut);` |
|     7 | 1112 | `		IcvTextRelease(pCtx,&sText);` |
|     7 | 1113 | `		return PH7_OK;` |
|     - | 1114 | `	}` |
|     - | 1115 | `#endif /* PH7_ENABLE_JIS */` |
|    49 | 1116 | `	iStart = sText.aOfft[iOfft];` |
|    49 | 1117 | `	iStop = sText.aOfft[iOfft + iLen];` |
|    49 | 1118 | `	ph7_result_string(pCtx,&zIn[iStart],iStop - iStart);` |
|    49 | 1119 | `	IcvTextRelease(pCtx,&sText);` |
|    49 | 1120 | `	return PH7_OK;` |
|    33 | 1121 | `}` |
|     - | 1122 |  |
|     - | 1123 | `/*` |
|     - | 1124 | ` * The search both position builtins run: the first match at or after iFrom, or` |
|     - | 1125 | ` * the LAST one when bReverse is set. Answers the character index or -1.` |
|     - | 1126 | ` */` |
|    56 | 1127 | `static int IcvSearch(const icv_text *pH,const icv_text *pN,int iFrom,int bReverse)` |
|     3 | 1128 | `{` |
|    59 | 1129 | `	int i,iFound = -1;` |
|    59 | 1130 | `	if( pN->nChar < 1 \|\| pN->nChar > pH->nChar ){` |
|    11 | 1131 | `		return -1;` |
|     - | 1132 | `	}` |
|   135 | 1133 | `	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){` |
|     - | 1134 | `		int k;` |
|   189 | 1135 | `		for( k = 0 ; k < pN->nChar && pH->aCode[i+k] == pN->aCode[k] ; ++k ){}` |
|   113 | 1136 | `		if( k == pN->nChar ){` |
|    51 | 1137 | `			if( !bReverse ){` |
|    27 | 1138 | `				return i;` |
|     - | 1139 | `			}` |
|    25 | 1140 | `			iFound = i;` |
|    12 | 1141 | `		}` |
|    46 | 1142 | `	}` |
|    23 | 1143 | `	return iFound;` |
|    31 | 1144 | `}` |
|     - | 1145 |  |
|     - | 1146 | `/*` |
|     - | 1147 | ` * int\|false iconv_strpos(string $haystack, string $needle, int $offset = 0,` |
|     - | 1148 | ` *                        ?string $encoding = null)` |
|     - | 1149 | ` * int\|false iconv_strrpos(string $haystack, string $needle,` |
|     - | 1150 | ` *                         ?string $encoding = null)` |
|     - | 1151 | ` *` |
|     - | 1152 | ` * One body, because php's two differ only in four places: strrpos has no` |
|     - | 1153 | ` * $offset at all, it tests the empty needle BEFORE the encoding is looked at` |
|     - | 1154 | ` * (so an over-long name is silent there and warns in strpos), it keeps looking` |
|     - | 1155 | ` * after a match instead of stopping at the first, and it has no out-of-bounds` |
|     - | 1156 | ` * ValueError to raise.` |
|     - | 1157 | ` *` |
|     - | 1158 | ` * The ORDER below is php's and it is visible from the outside, because the two` |
|     - | 1159 | ` * halves of the search report differently. The NEEDLE goes through a whole` |
|     - | 1160 | ` * conversion, so an ill-formed one raises the same two diagnostics anything` |
|     - | 1161 | ` * else does. The HAYSTACK is walked one character at a time into a buffer` |
|     - | 1162 | ` * exactly one wide, and php's loop leaves that walk the moment a character` |
|     - | 1163 | ` * fails to convert -- WITHOUT recording why. So an ill-formed haystack is` |
|     - | 1164 | ` * silent: the search simply cannot see past the bad byte, and` |
|     - | 1165 | `` * `iconv_strpos("ab\xFFcd","cd")` is FALSE with nothing said. What it does`` |
|     - | 1166 | ` * decide is the count the out-of-bounds ValueError is measured against, which` |
|     - | 1167 | ` * is why an $offset past the first bad byte raises where the same call on a` |
|     - | 1168 | ` * clean string answers false.` |
|     - | 1169 | ` */` |
|    76 | 1170 | `static int IcvStrposBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReverse)` |
|     3 | 1171 | `{` |
|     - | 1172 | `	const char *zH,*zN;` |
|    79 | 1173 | `	int nH,nN,err = ICV_OK,iScanned,iFound;` |
|    79 | 1174 | `	sxi64 iOfft = 0;` |
|     - | 1175 | `	icv_cs sCs;` |
|     - | 1176 | `	icv_text sH,sN;` |
|    79 | 1177 | `	if( nArg < 2 ){` |
|   ! 0 | 1178 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1179 | `		return PH7_OK;` |
|     - | 1180 | `	}` |
|    79 | 1181 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|    79 | 1182 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|    79 | 1183 | `	if( bReverse && nN < 1 ){` |
|     - | 1184 | `		/* php's order: the empty needle answers false before the name is` |
|     - | 1185 | `		 * measured, which is why an over-long $encoding is silent here. */` |
|     5 | 1186 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1187 | `		return PH7_OK;` |
|     - | 1188 | `	}` |
|    75 | 1189 | `	if( !IcvStrEncArg(pCtx,nArg > (bReverse ? 2 : 3) ? apArg[bReverse ? 2 : 3] : 0,&sCs) ){` |
|     3 | 1190 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1191 | `		return PH7_OK;` |
|     - | 1192 | `	}` |
|    70 | 1193 | `	if( !bReverse && nArg > 2` |
|    41 | 1194 | `	 && PH7_IntArgResolve(pCtx,apArg[2],"iconv_strpos",3,"$offset","int",&iOfft) != PH7_OK ){` |
|     - | 1195 | ``		/* $offset is php's plain `int`, so a null is the deprecation the scope policy turns`` |
|     - | 1196 | `		 * into a TypeError -- and it has to be REFUSED here rather than skipped,` |
|     - | 1197 | `		 * which is what treating a null argument as "not passed" would do. */` |
|   ! 0 | 1198 | `		return PH7_OK;` |
|     - | 1199 | `	}` |
|    73 | 1200 | `	if( iOfft < 0 ){` |
|     - | 1201 | `		/* A negative offset counts from the end, so THIS is the one path that` |
|     - | 1202 | `		 * measures the haystack before the needle -- and the one place an` |
|     - | 1203 | `		 * ill-formed haystack is heard about at all. */` |
|     5 | 1204 | `		if( !IcvEncReady(pCtx,&sCs) ){` |
|   ! 0 | 1205 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 1206 | `			return PH7_OK;` |
|     - | 1207 | `		}` |
|     5 | 1208 | `		if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1209 | `			return PH7_OK;` |
|     - | 1210 | `		}` |
|     5 | 1211 | `		IcvTextRelease(pCtx,&sH);` |
|     5 | 1212 | `		if( err != ICV_OK ){` |
|   ! 0 | 1213 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|   ! 0 | 1214 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 1215 | `			return PH7_OK;` |
|     - | 1216 | `		}` |
|     5 | 1217 | `		iOfft += sH.nChar;` |
|     5 | 1218 | `		if( iOfft < 0 ){` |
|     3 | 1219 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1220 | `				"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     3 | 1221 | `			return PH7_OK;` |
|     - | 1222 | `		}` |
|     1 | 1223 | `	}` |
|    71 | 1224 | `	if( nN < 1 ){` |
|     7 | 1225 | `		ph7_result_bool(pCtx,0);` |
|     7 | 1226 | `		return PH7_OK;` |
|     - | 1227 | `	}` |
|     - | 1228 | `	/* php converts the NEEDLE whole before it scans, so an ill-formed needle is` |
|     - | 1229 | `	 * the same two diagnostics as an ill-formed argument anywhere else. */` |
|    65 | 1230 | `	if( !IcvEncReady(pCtx,&sCs) ){` |
|     3 | 1231 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1232 | `		return PH7_OK;` |
|     - | 1233 | `	}` |
|    63 | 1234 | `	if( IcvTextDecode(pCtx,&sN,zN,nN,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1235 | `		return PH7_OK;` |
|     - | 1236 | `	}` |
|    63 | 1237 | `	if( err != ICV_OK ){` |
|     5 | 1238 | `		IcvTextRelease(pCtx,&sN);` |
|     5 | 1239 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1240 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1241 | `		return PH7_OK;` |
|     - | 1242 | `	}` |
|    59 | 1243 | `	if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1244 | `		IcvTextRelease(pCtx,&sN);` |
|   ! 0 | 1245 | `		return PH7_OK;` |
|     - | 1246 | `	}` |
|    59 | 1247 | `	iScanned = sH.nChar;` |
|    59 | 1248 | `	iFound = IcvSearch(&sH,&sN,bReverse ? 0 : (int)iOfft,bReverse);` |
|    59 | 1249 | `	IcvTextRelease(pCtx,&sN);` |
|    59 | 1250 | `	IcvTextRelease(pCtx,&sH);` |
|    59 | 1251 | `	if( err != ICV_OK ){` |
|     - | 1252 | `		/* Whether the walk SAYS anything about the bad character depends on how` |
|     - | 1253 | `		 * far it got, because php hears about it from the call that converted` |
|     - | 1254 | `		 * the character BEFORE it. An error at index 0 is therefore silent, and` |
|     - | 1255 | `		 * so is one the walk never reached -- a full match ends the walk, so` |
|     - | 1256 | ``		 * `iconv_strpos(str_repeat("x",100)."\xFF","xx")` answers 0 with`` |
|     - | 1257 | ``		 * nothing said while `iconv_strpos("ab\xFF","ab")` answers FALSE with`` |
|     - | 1258 | `		 * the notice. */` |
|    13 | 1259 | `		int iStop = (!bReverse && iFound >= 0) ? iFound + sN.nChar - 1 : iScanned;` |
|    13 | 1260 | `		if( iScanned >= 1 && iStop >= iScanned - 1 ){` |
|     5 | 1261 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1262 | `			ph7_result_bool(pCtx,0);` |
|     5 | 1263 | `			return PH7_OK;` |
|     - | 1264 | `		}` |
|     4 | 1265 | `	}` |
|    55 | 1266 | `	if( !bReverse && iOfft > iScanned ){` |
|     5 | 1267 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1268 | `			"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     5 | 1269 | `		return PH7_OK;` |
|     - | 1270 | `	}` |
|    51 | 1271 | `	if( iFound < 0 ){` |
|    15 | 1272 | `		ph7_result_bool(pCtx,0);` |
|     8 | 1273 | `	}else{` |
|    37 | 1274 | `		ph7_result_int(pCtx,iFound);` |
|     - | 1275 | `	}` |
|    51 | 1276 | `	return PH7_OK;` |
|    41 | 1277 | `}` |
|    52 | 1278 | `static int PH7_builtin_iconv_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     3 | 1279 | `{` |
|    55 | 1280 | `	return IcvStrposBody(pCtx,nArg,apArg,0);` |
|     3 | 1281 | `}` |
|    24 | 1282 | `static int PH7_builtin_iconv_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1283 | `{` |
|    25 | 1284 | `	return IcvStrposBody(pCtx,nArg,apArg,1);` |
|     1 | 1285 | `}` |
|     - | 1286 |  |
|     - | 1287 | `/* --- The encoding settings --------------------------------------------- */` |
|     - | 1288 |  |
|     - | 1289 | `/*` |
|     - | 1290 | ` * array\|string\|false iconv_get_encoding(string $type = "all")` |
|     - | 1291 | ` *` |
|     - | 1292 | `` * php answers `iconv.input_encoding` / `output_encoding` / `internal_encoding`,`` |
|     - | 1293 | `` * each falling back to `default_charset`. The scope policy removes all three of those`` |
|     - | 1294 | ` * directives -- every one of them is deprecated, which is also why this` |
|     - | 1295 | ` * function's SETTER counterpart is not here at all (see the twin pair in` |
|     - | 1296 | `` * 002-integration) -- so `default_charset` is what all three answer, and`` |
|     - | 1297 | ` * moving it moves them together, exactly as php does when the iconv directives` |
|     - | 1298 | ` * are left unset. $type matches case-insensitively; anything else is FALSE,` |
|     - | 1299 | ` * with no diagnostic of any kind.` |
|     - | 1300 | ` */` |
|    36 | 1301 | `static int PH7_builtin_iconv_get_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1302 | `{` |
|     - | 1303 | `	static const char * const azType[] = { "input_encoding", "output_encoding", "internal_encoding" };` |
|     - | 1304 | `	SyBlob sIni;` |
|    38 | 1305 | `	const char *zType = "all";` |
|    38 | 1306 | `	int nType = 3,k;` |
|    38 | 1307 | `	if( nArg > 0 ){` |
|    34 | 1308 | `		zType = ph7_value_to_string(apArg[0],&nType);` |
|    16 | 1309 | `	}` |
|    38 | 1310 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|    38 | 1311 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|    38 | 1312 | `	if( nType == 3 && SyStrnicmp(zType,"all",3) == 0 ){` |
|    11 | 1313 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    11 | 1314 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    11 | 1315 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 1316 | `			SyBlobRelease(&sIni);` |
|   ! 0 | 1317 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1318 | `		}` |
|    11 | 1319 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    41 | 1320 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    31 | 1321 | `			ph7_array_add_strkey_elem(pArray,azType[k],pVal);` |
|    16 | 1322 | `		}` |
|    11 | 1323 | `		ph7_result_value(pCtx,pArray);` |
|    11 | 1324 | `		ph7_context_release_value(pCtx,pVal);` |
|    11 | 1325 | `		SyBlobRelease(&sIni);` |
|    11 | 1326 | `		return PH7_OK;` |
|     - | 1327 | `	}` |
|    80 | 1328 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    70 | 1329 | `		if( nType == (int)SyStrlen(azType[k]) && SyStrnicmp(zType,azType[k],(sxu32)nType) == 0 ){` |
|    18 | 1330 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    18 | 1331 | `			SyBlobRelease(&sIni);` |
|    18 | 1332 | `			return PH7_OK;` |
|     - | 1333 | `		}` |
|    28 | 1334 | `	}` |
|    11 | 1335 | `	SyBlobRelease(&sIni);` |
|    11 | 1336 | `	ph7_result_bool(pCtx,0);` |
|    11 | 1337 | `	return PH7_OK;` |
|    20 | 1338 | `}` |
|     - | 1339 |  |
|     - | 1340 | `/* --- MIME header words -------------------------------------------------- */` |
|     - | 1341 |  |
|     - | 1342 | `/*` |
|     - | 1343 | ` * Convert zIn into pOut, appending NOTHING unless the whole run converts. This` |
|     - | 1344 | ` * is php's _php_iconv_appendl(), which builds its own buffer and only hands it` |
|     - | 1345 | ` * over on success -- so a header run that fails part-way leaves no partial` |
|     - | 1346 | ` * bytes behind, which is visible whenever CONTINUE_ON_ERROR lets the scan go on` |
|     - | 1347 | ` * afterwards.` |
|     - | 1348 | ` */` |
|   990 | 1349 | `static int IcvAppendConv(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1350 | `	int iFrom,const icv_cs *pTo)` |
|     3 | 1351 | `{` |
|     - | 1352 | `	SyBlob sTmp;` |
|     - | 1353 | `	int rc;` |
|   993 | 1354 | `	SyBlobInit(&sTmp,&pCtx->pVm->sAllocator);` |
|   993 | 1355 | `	rc = IcvConvert(&sTmp,zIn,nIn,iFrom,pTo,0);` |
|   993 | 1356 | `	if( rc == ICV_OK ){` |
|   989 | 1357 | `		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|   493 | 1358 | `	}` |
|   993 | 1359 | `	SyBlobRelease(&sTmp);` |
|   993 | 1360 | `	return rc;` |
|     3 | 1361 | `}` |
|     - | 1362 |  |
|     - | 1363 |  |
|     - | 1364 | `/*` |
|     - | 1365 | ` * base64, in the two shapes RFC 2047 needs. The DECODER is php's` |
|     - | 1366 | ` * php_base64_decode() in its non-strict mode: every byte outside the alphabet` |
|     - | 1367 | ` * is skipped, and a partial final group contributes what its bits allow, so` |
|     - | 1368 | ` * "!!!" decodes to the empty string rather than failing.` |
|     - | 1369 | ` */` |
|     - | 1370 | `static const signed char aIcvB64[128] = {` |
|     - | 1371 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1372 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1373 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,` |
|     - | 1374 | `	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,` |
|     - | 1375 | `	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 1376 | `	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,` |
|     - | 1377 | `	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 1378 | `	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1` |
|     - | 1379 | `};` |
|    52 | 1380 | `static void IcvBase64Encode(SyBlob *pOut,const unsigned char *z,int n)` |
|     2 | 1381 | `{` |
|     - | 1382 | `	static const char zAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     - | 1383 | `	char zGrp[4];` |
|    54 | 1384 | `	int i = 0;` |
|   202 | 1385 | `	while( i + 2 < n ){` |
|   150 | 1386 | `		sxu32 v = ((sxu32)z[i] << 16) \| ((sxu32)z[i+1] << 8) \| z[i+2];` |
|   150 | 1387 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F]; zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|   150 | 1388 | `		zGrp[2] = zAlpha[(v >> 6) & 0x3F];  zGrp[3] = zAlpha[v & 0x3F];` |
|   150 | 1389 | `		SyBlobAppend(pOut,zGrp,4);` |
|   150 | 1390 | `		i += 3;` |
|     2 | 1391 | `	}` |
|    54 | 1392 | `	if( i < n ){` |
|    48 | 1393 | `		sxu32 v = (sxu32)z[i] << 16;` |
|    48 | 1394 | `		int nRem = n - i;` |
|    48 | 1395 | `		if( nRem > 1 ){` |
|    35 | 1396 | `			v \|= (sxu32)z[i+1] << 8;` |
|    17 | 1397 | `		}` |
|    48 | 1398 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F];` |
|    48 | 1399 | `		zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|    48 | 1400 | `		zGrp[2] = nRem > 1 ? zAlpha[(v >> 6) & 0x3F] : '=';` |
|    48 | 1401 | `		zGrp[3] = '=';` |
|    48 | 1402 | `		SyBlobAppend(pOut,zGrp,4);` |
|    23 | 1403 | `	}` |
|    54 | 1404 | `}` |
|     - | 1405 | `/* How many base64 characters n input bytes become. */` |
|   104 | 1406 | `static int IcvBase64Len(int n)` |
|     2 | 1407 | `{` |
|   106 | 1408 | `	return ((n + 2) / 3) * 4;` |
|     2 | 1409 | `}` |
|    48 | 1410 | `static void IcvBase64Decode(SyBlob *pOut,const char *z,int n)` |
|     1 | 1411 | `{` |
|    49 | 1412 | `	sxu32 v = 0;` |
|    49 | 1413 | `	int i,nBit = 0;` |
|   357 | 1414 | `	for( i = 0 ; i < n ; ++i ){` |
|   309 | 1415 | `		int c = (unsigned char)z[i];` |
|   309 | 1416 | `		int d = (c < 128) ? aIcvB64[c] : -1;` |
|   309 | 1417 | `		if( d < 0 ){` |
|     - | 1418 | `			/* php's non-strict decoder skips EVERYTHING outside the alphabet,` |
|     - | 1419 | ``			 * and that includes the padding: `=` is counted and stepped over,`` |
|     - | 1420 | `			 * not treated as the end, so ":!!!=ZZ" still decodes its "ZZ". */` |
|    57 | 1421 | `			continue;` |
|     - | 1422 | `		}` |
|   253 | 1423 | `		v = (v << 6) \| (sxu32)d;` |
|   253 | 1424 | `		nBit += 6;` |
|   253 | 1425 | `		if( nBit >= 8 ){` |
|   179 | 1426 | `			unsigned char b = (unsigned char)((v >> (nBit - 8)) & 0xFF);` |
|   179 | 1427 | `			SyBlobAppend(pOut,&b,1);` |
|   179 | 1428 | `			nBit -= 8;` |
|    89 | 1429 | `		}` |
|   127 | 1430 | `	}` |
|    49 | 1431 | `}` |
|     - | 1432 |  |
|     - | 1433 | `/*` |
|     - | 1434 | `` * quoted-printable, php's php_quot_print_decode() with `replace_us_by_ws` on --`` |
|     - | 1435 | `` * which is the `Q` of RFC 2047, where `_` stands for a space. Answers 0 for the`` |
|     - | 1436 | `` * strings php refuses (a `=` followed by one hex digit and a non-hex one, and a`` |
|     - | 1437 | ` * soft break that runs off the end); those are what make its caller fall back` |
|     - | 1438 | ` * to the raw encoded word. Stops at a NUL, as php's does.` |
|     - | 1439 | ` */` |
|    20 | 1440 | `static int IcvQPrintDecode(SyBlob *pOut,const char *z,int n,int bUnderscore)` |
|     2 | 1441 | `{` |
|    22 | 1442 | `	int i = 0;` |
|    56 | 1443 | `	while( i < n && z[i] != 0 ){` |
|    44 | 1444 | `		int c = (unsigned char)z[i];` |
|    44 | 1445 | `		if( c != '=' ){` |
|    23 | 1446 | `			unsigned char b = (unsigned char)((bUnderscore && c == '_') ? ' ' : c);` |
|    23 | 1447 | `			SyBlobAppend(pOut,&b,1);` |
|    23 | 1448 | `			i++;` |
|    23 | 1449 | `			continue;` |
|     - | 1450 | `		}` |
|    22 | 1451 | `		i++;` |
|    22 | 1452 | `		if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1453 | `			break;` |
|     - | 1454 | `		}` |
|    22 | 1455 | `		if( SyisHex(z[i]) ){` |
|    13 | 1456 | `			int hi = SyHexToint(z[i]);` |
|    13 | 1457 | `			if( i + 1 >= n \|\| !SyisHex(z[i+1]) ){` |
|   ! 0 | 1458 | `				return 0;` |
|     - | 1459 | `			}` |
|     - | 1460 | `			{` |
|    13 | 1461 | `				unsigned char b = (unsigned char)((hi << 4) \| SyHexToint(z[i+1]));` |
|    13 | 1462 | `				SyBlobAppend(pOut,&b,1);` |
|     - | 1463 | `			}` |
|    13 | 1464 | `			i += 2;` |
|    13 | 1465 | `			continue;` |
|     - | 1466 | `		}` |
|     - | 1467 | `		/* A soft line break: any run of spaces and tabs, then the newline. */` |
|     9 | 1468 | `		while( z[i] == ' ' \|\| z[i] == '\t' ){` |
|   ! 0 | 1469 | `			i++;` |
|   ! 0 | 1470 | `			if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1471 | `				return 0;` |
|     - | 1472 | `			}` |
|   ! 0 | 1473 | `		}` |
|     9 | 1474 | `		if( z[i] != '\r' && z[i] != '\n' ){` |
|     9 | 1475 | `			return 0;` |
|     - | 1476 | `		}` |
|   ! 0 | 1477 | `		if( z[i] == '\r' && i + 1 < n && z[i+1] == '\n' ){` |
|   ! 0 | 1478 | `			i++;` |
|   ! 0 | 1479 | `		}` |
|   ! 0 | 1480 | `		i++;` |
|   ! 0 | 1481 | `	}` |
|    13 | 1482 | `	return 1;` |
|    12 | 1483 | `}` |
|     - | 1484 |  |
|     - | 1485 | ``/* php's qp_table: 1 = the byte may be written as itself, 3 = it needs `=XX`. */`` |
|  4064 | 1486 | `static int IcvQPrintCost(int c)` |
|     1 | 1487 | `{` |
|  4065 | 1488 | `	if( c < 0x21 \|\| c > 0x7E \|\| c == '=' \|\| c == '?' \|\| c == '_' ){` |
|  1231 | 1489 | `		return 3;` |
|     - | 1490 | `	}` |
|  2835 | 1491 | `	return 1;` |
|  2033 | 1492 | `}` |
|     - | 1493 |  |
|     - | 1494 | `#define ICV_SCHEME_B64 0` |
|     - | 1495 | `#define ICV_SCHEME_QP  1` |
|     - | 1496 |  |
|     - | 1497 | `/*` |
|     - | 1498 | ` * php's _php_iconv_mime_encode(). The line budget is a single running counter` |
|     - | 1499 | ` * that every piece of the header subtracts from, and the two schemes spend it` |
|     - | 1500 | ` * differently: base64 works out how many INPUT bytes fit a line by arithmetic` |
|     - | 1501 | `` * (`(char_cnt - 2) / 4 * 3`, minus a four-byte reserve), while quoted-printable`` |
|     - | 1502 | ` * converts a candidate run, PRICES it through the table above, and shrinks the` |
|     - | 1503 | ` * run until the price fits -- which is why a value made of three-byte` |
|     - | 1504 | ` * characters wraps where it does. The counter is charged the field name's RAW` |
|     - | 1505 | ` * byte length even when the name is dropped for not being ASCII.` |
|     - | 1506 | ` */` |
|    66 | 1507 | `static int IcvMimeEncode(ph7_context *pCtx,SyBlob *pOut,` |
|     - | 1508 | `	const char *zName,int nName,const char *zVal,int nVal,` |
|     - | 1509 | `	sxi64 iMaxLine,const char *zLf,int nLf,int iScheme,` |
|     - | 1510 | `	const icv_cs *pTo,const char *zToName,int nToName,int iFrom)` |
|     2 | 1511 | `{` |
|     - | 1512 | `	SyBlob sChunk;` |
|     - | 1513 | `	sxi64 iBudget;` |
|    68 | 1514 | `	int i = 0,rc = ICV_OK;` |
|    68 | 1515 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nToName + 12 >= iMaxLine ){` |
|   ! 0 | 1516 | `		return ICV_TOO_BIG;` |
|     - | 1517 | `	}` |
|    68 | 1518 | `	iBudget = iMaxLine;` |
|    68 | 1519 | `	SyBlobInit(&sChunk,&pCtx->pVm->sAllocator);` |
|     - | 1520 | `	/* The field NAME goes out in ASCII, and php ignores whether that worked --` |
|     - | 1521 | `	 * so a name carrying a byte ASCII cannot hold contributes NOTHING (the` |
|     - | 1522 | `	 * append is all-or-nothing) while still costing its raw length. */` |
|     - | 1523 | `	{` |
|     - | 1524 | `		icv_cs sAscii;` |
|    68 | 1525 | `		sAscii.iEnc = ICV_ASCII;` |
|    68 | 1526 | `		sAscii.bTranslit = 0;` |
|    68 | 1527 | `		sAscii.bIgnore = 0;` |
|    68 | 1528 | `		sAscii.nName = 0;` |
|    68 | 1529 | `		(void)IcvAppendConv(pCtx,pOut,zName,nName,iFrom,&sAscii);` |
|     - | 1530 | `	}` |
|    68 | 1531 | `	iBudget -= nName;` |
|    68 | 1532 | `	SyBlobAppend(pOut,": ",2);` |
|    68 | 1533 | `	iBudget -= 2;` |
|    33 | 1534 | `	do{` |
|     - | 1535 | ``		/* `=?`, the charset, `?`, the scheme letter, `?` and the closing `?=`;`` |
|     - | 1536 | `		 * base64 needs one more character than quoted-printable can get away` |
|     - | 1537 | `		 * with, which is what makes the two minimums differ. */` |
|   106 | 1538 | `		sxi64 iMinWord = 7 + nToName + (iScheme == ICV_SCHEME_B64 ? 4 : 3);` |
|   106 | 1539 | `		if( iBudget < iMinWord + nLf + 1 ){` |
|    41 | 1540 | `			SyBlobAppend(pOut,zLf,(sxu32)nLf);` |
|    41 | 1541 | `			SyBlobAppend(pOut," ",1);` |
|    41 | 1542 | `			iBudget = iMaxLine - 1;` |
|    20 | 1543 | `		}` |
|   106 | 1544 | `		SyBlobAppend(pOut,"=?",2);` |
|   106 | 1545 | `		SyBlobAppend(pOut,zToName,(sxu32)nToName);` |
|   106 | 1546 | `		SyBlobAppend(pOut,"?",1);` |
|   106 | 1547 | `		SyBlobAppend(pOut,iScheme == ICV_SCHEME_B64 ? "B" : "Q",1);` |
|   106 | 1548 | `		SyBlobAppend(pOut,"?",1);` |
|   106 | 1549 | `		iBudget -= 2 + nToName + 3;` |
|   106 | 1550 | `		if( iScheme == ICV_SCHEME_B64 ){` |
|    58 | 1551 | `			sxi64 iRoom = (iBudget - 2) / 4 * 3 - 4;` |
|     - | 1552 | `			int nTook;` |
|    58 | 1553 | `			if( iRoom <= 0 ){` |
|   ! 0 | 1554 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1555 | `				break;` |
|     - | 1556 | `			}` |
|    58 | 1557 | `			SyBlobReset(&sChunk);` |
|    58 | 1558 | `			rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|    58 | 1559 | `			if( rc != ICV_OK ){` |
|     5 | 1560 | `				break;` |
|     - | 1561 | `			}` |
|    54 | 1562 | `			if( nTook == 0 && i < nVal ){` |
|     - | 1563 | `				/* The line has room for a word but not for one CHARACTER of the` |
|     - | 1564 | `				 * value: php's iconv comes back E2BIG having consumed nothing,` |
|     - | 1565 | `				 * and refuses rather than emitting an empty word forever. */` |
|   ! 0 | 1566 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1567 | `				break;` |
|     - | 1568 | `			}` |
|    54 | 1569 | `			if( IcvBase64Len((int)SyBlobLength(&sChunk)) > iBudget ){` |
|   ! 0 | 1570 | `				rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1571 | `				break;` |
|     - | 1572 | `			}` |
|    80 | 1573 | `			IcvBase64Encode(pOut,(const unsigned char *)SyBlobData(&sChunk),` |
|    52 | 1574 | `				(int)SyBlobLength(&sChunk));` |
|    54 | 1575 | `			iBudget -= IcvBase64Len((int)SyBlobLength(&sChunk));` |
|    28 | 1576 | `		}else{` |
|    49 | 1577 | `			sxi64 iRoom = iBudget - 2;` |
|    49 | 1578 | `			int nTook = 0,k,iCost = 0;` |
|   144 | 1579 | `			for(;;){` |
|   169 | 1580 | `				int iStart = i;` |
|   169 | 1581 | `				if( iRoom <= 0 ){` |
|   ! 0 | 1582 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1583 | `					break;` |
|     - | 1584 | `				}` |
|   169 | 1585 | `				SyBlobReset(&sChunk);` |
|   169 | 1586 | `				rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|   169 | 1587 | `				if( rc != ICV_OK ){` |
|   ! 0 | 1588 | `					break;` |
|     - | 1589 | `				}` |
|   169 | 1590 | `				if( nTook == 0 && i < nVal ){` |
|     - | 1591 | `					/* Not even one character fits: php's own E2BIG-with-no-progress` |
|     - | 1592 | `					 * refusal, which is what keeps this from emitting an empty` |
|     - | 1593 | `					 * encoded word for ever. */` |
|   ! 0 | 1594 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1595 | `					break;` |
|     - | 1596 | `				}` |
|   169 | 1597 | `				iCost = 0;` |
|  3775 | 1598 | `				for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|  3607 | 1599 | `					iCost += IcvQPrintCost(((const unsigned char *)SyBlobData(&sChunk))[k]);` |
|  1804 | 1600 | `				}` |
|   169 | 1601 | `				if( iCost <= iBudget - 2 ){` |
|    49 | 1602 | `					break;` |
|     - | 1603 | `				}` |
|   121 | 1604 | `				iRoom -= ((iCost - (iBudget - 2)) + 2) / 3;` |
|   121 | 1605 | `				i = iStart;` |
|     1 | 1606 | `			}` |
|    49 | 1607 | `			if( rc != ICV_OK ){` |
|   ! 0 | 1608 | `				break;` |
|     - | 1609 | `			}` |
|   507 | 1610 | `			for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|   459 | 1611 | `				int c = ((const unsigned char *)SyBlobData(&sChunk))[k];` |
|   459 | 1612 | `				if( IcvQPrintCost(c) == 1 ){` |
|   333 | 1613 | `					char b = (char)c;` |
|   333 | 1614 | `					SyBlobAppend(pOut,&b,1);` |
|   333 | 1615 | `					iBudget--;` |
|   167 | 1616 | `				}else{` |
|     - | 1617 | `					static const char zHex[] = "0123456789ABCDEF";` |
|     - | 1618 | `					char zEsc[3];` |
|   127 | 1619 | `					zEsc[0] = '='; zEsc[1] = zHex[(c >> 4) & 0x0F]; zEsc[2] = zHex[c & 0x0F];` |
|   127 | 1620 | `					SyBlobAppend(pOut,zEsc,3);` |
|   127 | 1621 | `					iBudget -= 3;` |
|     - | 1622 | `				}` |
|   230 | 1623 | `			}` |
|     - | 1624 | `		}` |
|   102 | 1625 | `		SyBlobAppend(pOut,"?=",2);` |
|   102 | 1626 | `		iBudget -= 2;` |
|   102 | 1627 | `	}while( i < nVal );` |
|    68 | 1628 | `	SyBlobRelease(&sChunk);` |
|    68 | 1629 | `	return rc;` |
|    35 | 1630 | `}` |
|     - | 1631 |  |
|     - | 1632 | `/*` |
|     - | 1633 | ` * php's _php_iconv_mime_decode(), a thirteen-state scanner over the header` |
|     - | 1634 | ` * text. What makes it a scanner rather than a matcher is what it does with the` |
|     - | 1635 | ` * things RFC 2047 does not allow: an encoded word that turns out not to be one` |
|     - | 1636 | ` * is re-emitted as the RAW TEXT it was, from the '=' the scan started at, and` |
|     - | 1637 | ` * the whitespace BETWEEN two encoded words is dropped while whitespace between` |
|     - | 1638 | `` * a word and plain text is kept -- which is why `=?..?= =?..?=` joins and`` |
|     - | 1639 | `` * `=?..?= x` does not.`` |
|     - | 1640 | ` *` |
|     - | 1641 | ` * $mode carries two bits. STRICT (1) refuses the non-RFC forms php otherwise` |
|     - | 1642 | ` * accepts -- an encoded word not followed by whitespace, above all -- and` |
|     - | 1643 | ` * CONTINUE_ON_ERROR (2) turns every refusal into "emit the raw word and carry` |
|     - | 1644 | ` * on". *pNext is left at the byte the scan stopped on, which is how` |
|     - | 1645 | ` * iconv_mime_decode_headers() walks a whole header block one field at a time.` |
|     - | 1646 | ` */` |
|   232 | 1647 | `static int IcvMimeDecode(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1648 | `	const icv_cs *pTo,int *pNext,int *pbTouched,int iMode)` |
|     2 | 1649 | `{` |
|     - | 1650 | `	SyBlob sWord;` |
|   234 | 1651 | `	const char *z = zIn;` |
|   234 | 1652 | `	int i = 0,rc = ICV_OK;` |
|   234 | 1653 | `	int iState = 0,iScheme = ICV_SCHEME_B64,nLeft,bTouched = 0;` |
|   234 | 1654 | `	int iWord = -1,iSpaces = -1,iCsName = -1,nCsName = 0,iText = -1,nText = 0;` |
|     - | 1655 | `	icv_cs sWordCs;` |
|   234 | 1656 | `	int bStrict = (iMode & 1) != 0,bGoOn = (iMode & 2) != 0;` |
|   234 | 1657 | `	SyBlobInit(&sWord,&pCtx->pVm->sAllocator);` |
|   234 | 1658 | `	sWordCs.iEnc = -1;` |
|   234 | 1659 | `	sWordCs.bTranslit = 0;` |
|   234 | 1660 | `	sWordCs.bIgnore = 0;` |
|   234 | 1661 | `	sWordCs.nName = 0;` |
|     - | 1662 | `	/*` |
|     - | 1663 | `	 * Emit z[iFrom..iTo) as literal text. php converts it from US-ASCII into the` |
|     - | 1664 | `	 * output charset, so a byte over 0x7F cannot go -- and what happens then is` |
|     - | 1665 | `	 * NOT one rule but three, because php checks the conversion's answer at some` |
|     - | 1666 | `	 * of these sites and not others. HARD fails whatever $mode says; CHECKED is` |
|     - | 1667 | `	 * the one CONTINUE_ON_ERROR was written for; SILENT drops the byte and says` |
|     - | 1668 | `	 * nothing, which is why a stray high byte AFTER a complete encoded word` |
|     - | 1669 | `	 * disappears while the same byte inside one is a failure.` |
|     - | 1670 | `	 */` |
|     - | 1671 | `/* php's _php_iconv_appendl() is ALL OR NOTHING: it converts into a buffer of` |
|     - | 1672 | ` * its own and only hands that to the output when the whole run went, so a run` |
|     - | 1673 | ` * that fails part-way appends nothing at all. And with a NULL source it appends` |
|     - | 1674 | ``  * nothing and answers success, which is what an already-consumed `encoded_word` `` |
|     - | 1675 | ` * becomes -- load-bearing, because state 9 stays in state 9 and re-runs for` |
|     - | 1676 | ` * every character after a word it could not convert. */` |
|     - | 1677 | `#define ICV_RAW_AT(iFrom,iTo,eMode) do { \` |
|     - | 1678 | `		int rcRaw = (iFrom) < 0 ? ICV_OK \` |
|     - | 1679 | `			: IcvAppendConv(pCtx,pOut,&z[iFrom],(iTo) - (iFrom),ICV_ASCII,pTo); \` |
|     - | 1680 | `		bTouched = 1; \` |
|     - | 1681 | `		if( (eMode) != 2 ){ \` |
|     - | 1682 | ``			/* php's `err` is a RUNNING variable at the checked sites: a later \`` |
|     - | 1683 | `			 * successful append assigns SUCCESS over an earlier failure, so a \` |
|     - | 1684 | `			 * refusal recorded mid-scan can still be forgotten. The silent \` |
|     - | 1685 | `			 * sites never touch it at all. */ \` |
|     - | 1686 | `			rc = rcRaw; \` |
|     - | 1687 | `			if( rcRaw != ICV_OK && ((eMode) == 1 \|\| !bGoOn) ){ \` |
|     - | 1688 | `				goto done; \` |
|     - | 1689 | `			} \` |
|     - | 1690 | `			if( rcRaw != ICV_OK ){ rc = ICV_OK; } \` |
|     - | 1691 | `		} \` |
|     - | 1692 | `	} while(0)` |
|     - | 1693 | `#define ICV_RAW(iFrom,iTo)        ICV_RAW_AT(iFrom,iTo,0)` |
|     - | 1694 | `#define ICV_RAW_HARD(iFrom,iTo)   ICV_RAW_AT(iFrom,iTo,1)` |
|     - | 1695 | `#define ICV_RAW_SILENT(iFrom,iTo) ICV_RAW_AT(iFrom,iTo,2)` |
|     - | 1696 | `	/*` |
|     - | 1697 | `	 * php walks with a POSITION and a separate COUNT, and the two go out of step` |
|     - | 1698 | ``	 * on purpose: several states rewind the position by one (`--p1`) without`` |
|     - | 1699 | `	 * giving the count back, so the scan re-reads a byte and then stops one byte` |
|     - | 1700 | `	 * SHORT of the end. That is not an accident of style -- it is what makes` |
|     - | 1701 | ``	 * `iconv_mime_decode("UHLDvGZ1bmc=\r\n")` a "Malformed string": the final`` |
|     - | 1702 | `	 * "\n" is never reached, so the scanner ends mid-EOL instead of after one.` |
|     - | 1703 | `	 */` |
|   234 | 1704 | `	nLeft = nIn;` |
|  3188 | 1705 | `	while( nLeft > 0 ){` |
|  2978 | 1706 | `		int c = (unsigned char)z[i];` |
|  2978 | 1707 | `		int bEos = 0;` |
|  2978 | 1708 | `		switch( iState ){` |
|   393 | 1709 | `		case 0:   /* anything at all */` |
|   788 | 1710 | `			if( c == '\r' ){ iState = 7; break; }` |
|   736 | 1711 | `			if( c == '\n' ){ iState = 8; break; }` |
|   730 | 1712 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|   613 | 1713 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   529 | 1714 | `			ICV_RAW(i,i+1);` |
|   529 | 1715 | `			iWord = -1;` |
|   529 | 1716 | `			if( bStrict ){ iState = 12; }` |
|   529 | 1717 | `			break;` |
|    75 | 1718 | `		case 1:   /* after '=': expecting '?' */` |
|   152 | 1719 | `			if( c != '?' ){` |
|   ! 0 | 1720 | `				if( c == '\r' \|\| c == '\n' ){ i--; }` |
|   ! 0 | 1721 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1722 | `				iWord = -1;` |
|   ! 0 | 1723 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1724 | `				break;` |
|     - | 1725 | `			}` |
|   152 | 1726 | `			iCsName = i + 1;` |
|   152 | 1727 | `			iState = 2;` |
|   152 | 1728 | `			break;` |
|   405 | 1729 | `		case 2:   /* the charset name */` |
|   812 | 1730 | `			if( c == '\r' \|\| c == '\n' ){` |
|   ! 0 | 1731 | `				i--;` |
|   ! 0 | 1732 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1733 | `				iCsName = -1;` |
|   ! 0 | 1734 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1735 | `				break;` |
|     - | 1736 | `			}` |
|   812 | 1737 | `			if( c != '?' && c != '*' ){` |
|   686 | 1738 | `				break;` |
|     - | 1739 | `			}` |
|   128 | 1740 | `			nCsName = i - iCsName;` |
|   128 | 1741 | `			if( nCsName > ICV_CSNMAXLEN + 15 ){` |
|     - | 1742 | `				/* php's own 80-byte scratch buffer for the name. */` |
|   ! 0 | 1743 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1744 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1745 | `				iWord = -1;` |
|   ! 0 | 1746 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1747 | `				break;` |
|     - | 1748 | `			}` |
|   128 | 1749 | `			IcvParseCharset(&z[iCsName],nCsName,&sWordCs);` |
|   128 | 1750 | `			if( sWordCs.iEnc < 0 ){` |
|    23 | 1751 | `				if( !bGoOn ){` |
|    11 | 1752 | `					rc = ICV_WRONG_CHARSET;` |
|    11 | 1753 | `					goto done;` |
|     - | 1754 | `				}` |
|     - | 1755 | `				/* php skips to the end of the word and hands it over raw. */` |
|     - | 1756 | `				{` |
|    13 | 1757 | `					int nQ = 2;` |
|    97 | 1758 | `					while( nQ > 0 && nLeft > 1 ){` |
|    85 | 1759 | `						if( z[++i] == '?' ){ nQ--; }` |
|    85 | 1760 | `						nLeft--;` |
|     1 | 1761 | `					}` |
|    13 | 1762 | `					if( i + 1 < nIn && z[i+1] == '=' ){` |
|    13 | 1763 | `						i++;` |
|    13 | 1764 | `						if( nLeft > 1 ){ nLeft--; }` |
|     6 | 1765 | `					}` |
|     - | 1766 | `				}` |
|    13 | 1767 | `				ICV_RAW_HARD(iWord,i+1);` |
|    13 | 1768 | `				iState = 12;` |
|    13 | 1769 | `				break;` |
|     - | 1770 | `			}` |
|   106 | 1771 | `			iState = (c == '*') ? 10 : 3;` |
|   106 | 1772 | `			break;` |
|    48 | 1773 | `		case 3:   /* the scheme letter */` |
|    98 | 1774 | `			if( c == 'b' \|\| c == 'B' ){ iScheme = ICV_SCHEME_B64; iState = 4; break; }` |
|    30 | 1775 | `			if( c == 'q' \|\| c == 'Q' ){ iScheme = ICV_SCHEME_QP; iState = 4; break; }` |
|     9 | 1776 | `			if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|     5 | 1777 | `			ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1778 | `			iWord = -1;` |
|     5 | 1779 | `			iState = bStrict ? 12 : 0;` |
|     5 | 1780 | `			break;` |
|    44 | 1781 | `		case 4:   /* expecting '?' */` |
|    90 | 1782 | `			if( c != '?' ){` |
|   ! 0 | 1783 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1784 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1785 | `				iWord = -1;` |
|   ! 0 | 1786 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1787 | `				break;` |
|     - | 1788 | `			}` |
|    90 | 1789 | `			iText = i + 1;` |
|    90 | 1790 | `			iState = 5;` |
|    90 | 1791 | `			break;` |
|   271 | 1792 | `		case 5:   /* the encoded text */` |
|   544 | 1793 | `			if( c == '?' ){` |
|    82 | 1794 | `				nText = i - iText;` |
|    82 | 1795 | `				iState = 6;` |
|    40 | 1796 | `			}` |
|   544 | 1797 | `			break;` |
|    36 | 1798 | `		case 6:   /* expecting the closing '=' */` |
|    74 | 1799 | `			if( c != '=' ){` |
|   ! 0 | 1800 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1801 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1802 | `				iWord = -1;` |
|   ! 0 | 1803 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1804 | `				break;` |
|     - | 1805 | `			}` |
|    74 | 1806 | `			iState = 9;` |
|    96 | 1807 | `			if( nLeft == 1 ){` |
|    46 | 1808 | `				bEos = 1;` |
|    24 | 1809 | `			}else{` |
|    29 | 1810 | `				break;` |
|     - | 1811 | `			}` |
|     - | 1812 | `			/* fall through -- the word ended with the string */` |
|     - | 1813 | `			/* FALLTHROUGH */` |
|     - | 1814 | `		case 9:   /* what follows a complete word */` |
|    74 | 1815 | `			if( !bEos && c != '\r' && c != '\n' && c != ' ' && c != '\t' && bStrict ){` |
|     5 | 1816 | `				ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1817 | `				iState = 12;` |
|     5 | 1818 | `				break;` |
|     - | 1819 | `			}` |
|    70 | 1820 | `			SyBlobReset(&sWord);` |
|    70 | 1821 | `			if( iScheme == ICV_SCHEME_B64 ){` |
|    49 | 1822 | `				IcvBase64Decode(&sWord,&z[iText],nText);` |
|    46 | 1823 | `			}else if( !IcvQPrintDecode(&sWord,&z[iText],nText,1) ){` |
|     9 | 1824 | `				if( !bGoOn ){ rc = ICV_UNKNOWN_ERR; goto done; }` |
|     3 | 1825 | `				ICV_RAW_HARD(iWord,i+1);` |
|     3 | 1826 | `				iWord = -1;` |
|     3 | 1827 | `				iState = bStrict ? 12 : 0;` |
|     3 | 1828 | `				break;` |
|     - | 1829 | `			}` |
|     - | 1830 | `			{` |
|     - | 1831 | `				int rcW;` |
|    61 | 1832 | `				bTouched = 1;` |
|    91 | 1833 | `				rcW = IcvAppendConv(pCtx,pOut,(const char *)SyBlobData(&sWord),` |
|    60 | 1834 | `					(int)SyBlobLength(&sWord),sWordCs.iEnc,pTo);` |
|    61 | 1835 | `				if( rcW != ICV_OK ){` |
|     3 | 1836 | `					if( !bGoOn ){ rc = rcW; goto done; }` |
|     - | 1837 | `					/* php hands the raw word over and carries on. If THAT will` |
|     - | 1838 | ``					 * not convert either it keeps the failure in `err` and stays`` |
|     - | 1839 | `					 * in this state, so the next character re-runs the whole` |
|     - | 1840 | `					 * branch; when it DOES convert the state moves on below. */` |
|   ! 0 | 1841 | `				rc = iWord < 0 ? ICV_OK` |
|   ! 0 | 1842 | `						: IcvAppendConv(pCtx,pOut,&z[iWord],i - iWord,ICV_ASCII,pTo);` |
|   ! 0 | 1843 | `					bTouched = 1;` |
|   ! 0 | 1844 | `					iWord = -1;` |
|   ! 0 | 1845 | `					if( rc != ICV_OK ){` |
|   ! 0 | 1846 | `						break;` |
|     - | 1847 | `					}` |
|   ! 0 | 1848 | `				}` |
|     - | 1849 | `			}` |
|    59 | 1850 | `			if( bEos ){` |
|    35 | 1851 | `				iState = 0;` |
|    35 | 1852 | `				break;` |
|     - | 1853 | `			}` |
|    25 | 1854 | `			if( c == '\r' ){ iState = 7; break; }` |
|    15 | 1855 | `			if( c == '\n' ){ iState = 8; break; }` |
|    13 | 1856 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|    13 | 1857 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|     7 | 1858 | `			bTouched = 1;` |
|     7 | 1859 | `			ICV_RAW_SILENT(i,i+1);` |
|     7 | 1860 | `			iState = 12;` |
|     7 | 1861 | `			break;` |
|    36 | 1862 | `		case 7:   /* after CR: expecting LF */` |
|    73 | 1863 | `			if( c == '\n' ){` |
|    71 | 1864 | `				iState = 8;` |
|    36 | 1865 | `			}else{` |
|     3 | 1866 | `				SyBlobAppend(pOut,"\r",1);` |
|     3 | 1867 | `				bTouched = 1;` |
|     3 | 1868 | `				ICV_RAW_SILENT(i,i+1);` |
|     3 | 1869 | `				iState = 0;` |
|     - | 1870 | `			}` |
|    73 | 1871 | `			break;` |
|    22 | 1872 | `		case 8:   /* after a newline: is the next line a continuation? */` |
|    45 | 1873 | `			if( c != ' ' && c != '\t' ){` |
|     - | 1874 | `				/* The field ended here. Rewinding the position and setting the` |
|     - | 1875 | `				 * count to one leaves *pNext pointing AT this character, so the` |
|     - | 1876 | `				 * caller's next field starts on it. */` |
|    39 | 1877 | `				nLeft = 1;` |
|    39 | 1878 | `				i--;` |
|    39 | 1879 | `				break;` |
|     - | 1880 | `			}` |
|     7 | 1881 | `			if( iWord < 0 ){` |
|     7 | 1882 | `				SyBlobAppend(pOut," ",1);` |
|     7 | 1883 | `				bTouched = 1;` |
|     3 | 1884 | `			}` |
|     7 | 1885 | `			iSpaces = -1;` |
|     7 | 1886 | `			iState = 11;` |
|     7 | 1887 | `			break;` |
|     3 | 1888 | `		case 10:  /* a language tag after the charset: dismissed */` |
|     7 | 1889 | `			if( c == '?' ){ iState = 3; }` |
|     7 | 1890 | `			break;` |
|    67 | 1891 | `		case 11:  /* a run of whitespace */` |
|   135 | 1892 | `			if( c == '\r' ){ iState = 7; break; }` |
|   135 | 1893 | `			if( c == '\n' ){ iState = 8; break; }` |
|   135 | 1894 | `			if( c == '=' ){` |
|     - | 1895 | `				/* Whitespace BETWEEN two encoded words disappears; whitespace` |
|     - | 1896 | `				 * that followed plain text does not. */` |
|    43 | 1897 | `				if( iSpaces >= 0 && iWord < 0 ){` |
|    39 | 1898 | `					bTouched = 1;` |
|    39 | 1899 | `					ICV_RAW_SILENT(iSpaces,i);` |
|    39 | 1900 | `					iSpaces = -1;` |
|    19 | 1901 | `				}` |
|    43 | 1902 | `				iWord = i;` |
|    43 | 1903 | `				iState = 1;` |
|    43 | 1904 | `				break;` |
|     - | 1905 | `			}` |
|    93 | 1906 | `			if( c == ' ' \|\| c == '\t' ){` |
|     4 | 1907 | `				break;` |
|     - | 1908 | `			}` |
|    87 | 1909 | `			if( iSpaces >= 0 ){` |
|    81 | 1910 | `				ICV_RAW_SILENT(iSpaces,i);` |
|    40 | 1911 | `			}` |
|    87 | 1912 | `			iSpaces = -1;` |
|    87 | 1913 | `			bTouched = 1;` |
|    87 | 1914 | `			ICV_RAW_SILENT(i,i+1);` |
|    87 | 1915 | `			iWord = -1;` |
|    87 | 1916 | `			iState = bStrict ? 12 : 0;` |
|    87 | 1917 | `			break;` |
|    74 | 1918 | `		case 12:  /* plain text */` |
|   149 | 1919 | `			if( c == '\r' ){ iState = 7; break; }` |
|   139 | 1920 | `			if( c == '\n' ){ iState = 8; break; }` |
|   139 | 1921 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   105 | 1922 | `			if( c == '=' && !bStrict ){ iWord = i; iState = 1; break; }` |
|   103 | 1923 | `			bTouched = 1;` |
|   103 | 1924 | `			ICV_RAW_SILENT(i,i+1);` |
|   102 | 1925 | `			break;` |
|     - | 1926 | `		}` |
|  2956 | 1927 | `		nLeft--;` |
|  2956 | 1928 | `		i++;` |
|     2 | 1929 | `	}` |
|   212 | 1930 | `	switch( iState ){` |
|    76 | 1931 | `	case 0: case 8: case 11: case 12:` |
|   154 | 1932 | `		break;` |
|    29 | 1933 | `	default:` |
|    59 | 1934 | `		if( bGoOn ){` |
|    31 | 1935 | `			if( iState == 1 ){` |
|     7 | 1936 | `				SyBlobAppend(pOut,"=",1);` |
|     3 | 1937 | `			}` |
|     - | 1938 | `			/* CONTINUE_ON_ERROR clears whatever the scan recorded on its way` |
|     - | 1939 | `			 * through, which is why a header full of bytes the output charset` |
|     - | 1940 | `			 * cannot hold comes back as the empty string rather than FALSE. */` |
|    31 | 1941 | `			rc = ICV_OK;` |
|    16 | 1942 | `		}else{` |
|    29 | 1943 | `			rc = ICV_MALFORMED;` |
|     - | 1944 | `		}` |
|    58 | 1945 | `		break;` |
|   105 | 1946 | `	}` |
|   116 | 1947 | `done:` |
|   234 | 1948 | `	SyBlobRelease(&sWord);` |
|   234 | 1949 | `	if( pNext ){` |
|    77 | 1950 | `		*pNext = i;` |
|    38 | 1951 | `	}` |
|   234 | 1952 | `	if( pbTouched ){` |
|    77 | 1953 | `		*pbTouched = bTouched;` |
|    38 | 1954 | `	}` |
|   234 | 1955 | `	return rc;` |
|     2 | 1956 | `}` |
|     - | 1957 | `#undef ICV_RAW_AT` |
|     - | 1958 | `#undef ICV_RAW` |
|     - | 1959 | `#undef ICV_RAW_HARD` |
|     - | 1960 | `#undef ICV_RAW_SILENT` |
|     - | 1961 |  |
|     - | 1962 | `/*` |
|     - | 1963 | ` * Read one option out of an OPTIONS array. php reads three of the five as` |
|     - | 1964 | `` * STRINGS ONLY -- a non-string `scheme`, `input-charset` or `output-charset` is`` |
|     - | 1965 | ` * not coerced, it is ignored -- so those need no conversion at all and the` |
|     - | 1966 | ` * array is never touched. The other two ARE coerced, and a coercion has to go` |
|     - | 1967 | `` * through a scratch copy: `ph7_value_to_xxx()` converts the value it is handed,`` |
|     - | 1968 | ` * which would rewrite the caller's own array (a recorded defect, four` |
|     - | 1969 | ` * builtins share it).` |
|     - | 1970 | ` */` |
|   180 | 1971 | `static const char * IcvOptionRawStr(ph7_value *pOpt,const char *zKey,int *pnOut)` |
|     2 | 1972 | `{` |
|   182 | 1973 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|   182 | 1974 | `	if( pV == 0 \|\| !ph7_value_is_string(pV) ){` |
|   124 | 1975 | `		return 0;` |
|     - | 1976 | `	}` |
|    60 | 1977 | `	return ph7_value_to_string(pV,pnOut);` |
|    92 | 1978 | `}` |
|    58 | 1979 | `static int IcvOptionInt(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,sxi64 *pOut)` |
|     2 | 1980 | `{` |
|    60 | 1981 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|    60 | 1982 | `	if( pV == 0 ){` |
|    44 | 1983 | `		return 0;` |
|     - | 1984 | `	}` |
|     8 | 1985 | `	SXUNUSED(pCtx);` |
|    17 | 1986 | `	*pOut = PH7_ValuePeekInt64(pV);` |
|    17 | 1987 | `	return 1;` |
|    31 | 1988 | `}` |
|     - | 1989 | `/* Copy one option into pOut as a string, coercing whatever it is. */` |
|    58 | 1990 | `static int IcvOptionCopyStr(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,SyBlob *pOut)` |
|     2 | 1991 | `{` |
|    60 | 1992 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|     - | 1993 | `	ph7_value sScratch,*pCopy;` |
|     - | 1994 | `	const char *z;` |
|    60 | 1995 | `	int n = 0;` |
|    60 | 1996 | `	if( pV == 0 ){` |
|    56 | 1997 | `		return 0;` |
|     - | 1998 | `	}` |
|     5 | 1999 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|     5 | 2000 | `	pCopy = PH7_ValuePeek(pV,&sScratch);` |
|     5 | 2001 | `	z = ph7_value_to_string(pCopy,&n);` |
|     5 | 2002 | `	if( n > 0 ){` |
|     3 | 2003 | `		SyBlobAppend(pOut,z,(sxu32)n);` |
|     1 | 2004 | `	}` |
|     5 | 2005 | `	PH7_MemObjRelease(&sScratch);` |
|     5 | 2006 | `	return 1;` |
|    31 | 2007 | `}` |
|     - | 2008 |  |
|     - | 2009 | `/*` |
|     - | 2010 | ` * string\|false iconv_mime_encode(string $field_name, string $field_value,` |
|     - | 2011 | ` *                                array $options = [])` |
|     - | 2012 | ` *` |
|     - | 2013 | `` * The five options php reads, and the exact way it reads them: `scheme` is one`` |
|     - | 2014 | ` * LETTER (its first, case-insensitively, and anything but B/b/Q/q leaves the` |
|     - | 2015 | `` * default alone rather than failing), `input-charset` and `output-charset` are`` |
|     - | 2016 | `` * only honoured when they are non-empty STRINGS, `line-length` goes through an`` |
|     - | 2017 | `` * ordinary int cast, and `line-break-chars` is taken as a string whatever it is.`` |
|     - | 2018 | ` */` |
|    80 | 2019 | `static int PH7_builtin_iconv_mime_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2020 | `{` |
|    82 | 2021 | `	const char *zName,*zVal,*zLf = "\r\n",*zIn = 0,*zOut = 0;` |
|    82 | 2022 | `	int nName,nVal,nLf = 2,nIn = 0,nOut = 0,iScheme = ICV_SCHEME_B64,err;` |
|    82 | 2023 | `	sxi64 iMaxLine = 76;` |
|     - | 2024 | `	icv_cs sFrom,sTo;` |
|     - | 2025 | `	SyBlob sOut,sIniLf,sIniIn,sIniOut,sIniCharset;` |
|     - | 2026 | `	ph7_value *pOpt;` |
|    82 | 2027 | `	if( nArg < 2 ){` |
|   ! 0 | 2028 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 2029 | `		return PH7_OK;` |
|     - | 2030 | `	}` |
|    82 | 2031 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    82 | 2032 | `	zVal = ph7_value_to_string(apArg[1],&nVal);` |
|    82 | 2033 | `	SyBlobInit(&sIniLf,&pCtx->pVm->sAllocator);` |
|    82 | 2034 | `	SyBlobInit(&sIniIn,&pCtx->pVm->sAllocator);` |
|    82 | 2035 | `	SyBlobInit(&sIniOut,&pCtx->pVm->sAllocator);` |
|    82 | 2036 | `	SyBlobInit(&sIniCharset,&pCtx->pVm->sAllocator);` |
|    82 | 2037 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIniCharset);` |
|    82 | 2038 | `	zIn = (const char *)SyBlobData(&sIniCharset);` |
|    82 | 2039 | `	nIn = (int)SyBlobLength(&sIniCharset);` |
|    82 | 2040 | `	zOut = zIn;` |
|    82 | 2041 | `	nOut = nIn;` |
|    82 | 2042 | `	pOpt = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|    82 | 2043 | `	if( pOpt ){` |
|     - | 2044 | `		const char *zS;` |
|    62 | 2045 | `		int nS = 0;` |
|    62 | 2046 | `		if( (zS = IcvOptionRawStr(pOpt,"scheme",&nS)) != 0 && nS > 0 ){` |
|     - | 2047 | `			/* One LETTER, case-insensitively, and anything but B or Q leaves the` |
|     - | 2048 | `			 * default where it was rather than failing. */` |
|    33 | 2049 | `			int c = IcvUpper((unsigned char)zS[0]);` |
|    33 | 2050 | `			if( c == 'B' ){ iScheme = ICV_SCHEME_B64; }` |
|    27 | 2051 | `			else if( c == 'Q' ){ iScheme = ICV_SCHEME_QP; }` |
|    16 | 2052 | `		}` |
|    62 | 2053 | `		if( (zS = IcvOptionRawStr(pOpt,"input-charset",&nS)) != 0 ){` |
|    12 | 2054 | `			if( nS >= ICV_CSNMAXLEN ){` |
|   ! 0 | 2055 | `				err = ICV_TOO_LONG;` |
|   ! 0 | 2056 | `				goto fail;` |
|     - | 2057 | `			}` |
|    12 | 2058 | `			if( nS > 0 ){` |
|    12 | 2059 | `				SyBlobAppend(&sIniIn,zS,(sxu32)nS);` |
|    12 | 2060 | `				zIn = (const char *)SyBlobData(&sIniIn);` |
|    12 | 2061 | `				nIn = nS;` |
|     5 | 2062 | `			}` |
|     5 | 2063 | `		}` |
|    62 | 2064 | `		if( (zS = IcvOptionRawStr(pOpt,"output-charset",&nS)) != 0 ){` |
|    16 | 2065 | `			if( nS >= ICV_CSNMAXLEN ){` |
|     3 | 2066 | `				err = ICV_TOO_LONG;` |
|     3 | 2067 | `				goto fail;` |
|     - | 2068 | `			}` |
|    14 | 2069 | `			if( nS > 0 ){` |
|    14 | 2070 | `				SyBlobAppend(&sIniOut,zS,(sxu32)nS);` |
|    14 | 2071 | `				zOut = (const char *)SyBlobData(&sIniOut);` |
|    14 | 2072 | `				nOut = nS;` |
|     6 | 2073 | `			}` |
|     6 | 2074 | `		}` |
|    60 | 2075 | `		IcvOptionInt(pCtx,pOpt,"line-length",&iMaxLine);` |
|    60 | 2076 | `		if( IcvOptionCopyStr(pCtx,pOpt,"line-break-chars",&sIniLf) ){` |
|     5 | 2077 | `			zLf = (const char *)SyBlobData(&sIniLf);` |
|     5 | 2078 | `			nLf = (int)SyBlobLength(&sIniLf);` |
|     2 | 2079 | `		}` |
|    29 | 2080 | `	}` |
|     - | 2081 | `	/* php measures the LINE BUDGET before it opens a converter, so a header` |
|     - | 2082 | `	 * that cannot fit is refused whatever the charsets say. (php compares the` |
|     - | 2083 | ``	 * budget as a size_t, so a NEGATIVE `line-length` is a huge one there and it`` |
|     - | 2084 | `	 * dies in the allocator instead -- "Possible integer overflow in memory` |
|     - | 2085 | `	 * allocation"; PHL compares signed, so it lands on the same refusal every` |
|     - | 2086 | `	 * other impossible budget gets. Recorded.) */` |
|    80 | 2087 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nOut + 12 >= iMaxLine ){` |
|     9 | 2088 | `		err = ICV_TOO_BIG;` |
|     9 | 2089 | `		goto fail;` |
|     - | 2090 | `	}` |
|    72 | 2091 | `	IcvParseCharset(zIn,nIn,&sFrom);` |
|    72 | 2092 | `	IcvParseCharset(zOut,nOut,&sTo);` |
|    72 | 2093 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|     5 | 2094 | `		err = ICV_WRONG_CHARSET;` |
|     5 | 2095 | `		goto fail;` |
|     - | 2096 | `	}` |
|    68 | 2097 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   101 | 2098 | `	err = IcvMimeEncode(pCtx,&sOut,zName,nName,zVal,nVal,iMaxLine,zLf,nLf,` |
|    33 | 2099 | `		iScheme,&sTo,zOut,nOut,sFrom.iEnc);` |
|    68 | 2100 | `	if( err == ICV_OK ){` |
|    64 | 2101 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    31 | 2102 | `	}` |
|    68 | 2103 | `	SyBlobRelease(&sOut);` |
|    68 | 2104 | `	if( err != ICV_OK ){` |
|     5 | 2105 | `		goto fail;` |
|     - | 2106 | `	}` |
|    64 | 2107 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    64 | 2108 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    64 | 2109 | `	return PH7_OK;` |
|     9 | 2110 | `fail:` |
|    19 | 2111 | `	if( err == ICV_TOO_LONG ){` |
|     3 | 2112 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 2113 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - | 2114 | `			ICV_CSNMAXLEN);` |
|     2 | 2115 | `	}else{` |
|    17 | 2116 | `		IcvShowError(pCtx,err,zOut,nOut,zIn,nIn);` |
|     - | 2117 | `	}` |
|    19 | 2118 | `	ph7_result_bool(pCtx,0);` |
|    19 | 2119 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    19 | 2120 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    19 | 2121 | `	return PH7_OK;` |
|    42 | 2122 | `}` |
|     - | 2123 |  |
|     - | 2124 | ``/* Resolve the `?string $encoding = null` the two decoders share: php's`` |
|     - | 2125 | ` * internal encoding, with only the length cap raised here. */` |
|   206 | 2126 | `static int IcvMimeDecodeArgs(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 2127 | `	icv_cs *pCs,sxi64 *pMode)` |
|     2 | 2128 | `{` |
|   208 | 2129 | `	*pMode = 0;` |
|   206 | 2130 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1])` |
|   206 | 2131 | `	 && PH7_IntArgResolve(pCtx,apArg[1],"iconv_mime_decode",2,"$mode","int",pMode) != PH7_OK ){` |
|   ! 0 | 2132 | `		return 0;` |
|     - | 2133 | `	}` |
|   208 | 2134 | `	if( !IcvStrEncArg(pCtx,nArg > 2 ? apArg[2] : 0,pCs) ){` |
|     3 | 2135 | `		return 0;` |
|     - | 2136 | `	}` |
|   206 | 2137 | `	if( pCs->iEnc < 0 ){` |
|     - | 2138 | ``		/* php names the source `"???"` here: the decoder learns the real one`` |
|     - | 2139 | `		 * word by word, so there is nothing to report but the target. */` |
|     5 | 2140 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,pCs->zName,pCs->nName,"???",3);` |
|     5 | 2141 | `		return 0;` |
|     - | 2142 | `	}` |
|   202 | 2143 | `	return 1;` |
|   105 | 2144 | `}` |
|     - | 2145 |  |
|     - | 2146 | `/* string\|false iconv_mime_decode(string $string, int $mode = 0,` |
|     - | 2147 | ` *                                ?string $encoding = null) */` |
|   160 | 2148 | `static int PH7_builtin_iconv_mime_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 2149 | `{` |
|     - | 2150 | `	const char *zIn;` |
|     - | 2151 | `	int nIn,err;` |
|     - | 2152 | `	sxi64 iMode;` |
|     - | 2153 | `	icv_cs sCs;` |
|     - | 2154 | `	SyBlob sOut;` |
|   162 | 2155 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     5 | 2156 | `		ph7_result_bool(pCtx,0);` |
|     5 | 2157 | `		return PH7_OK;` |
|     - | 2158 | `	}` |
|   158 | 2159 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   158 | 2160 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   158 | 2161 | `	err = IcvMimeDecode(pCtx,&sOut,zIn,nIn,&sCs,0,0,(int)iMode);` |
|   158 | 2162 | `	if( err == ICV_OK ){` |
|   110 | 2163 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    56 | 2164 | `	}else{` |
|    50 | 2165 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|    50 | 2166 | `		ph7_result_bool(pCtx,0);` |
|     - | 2167 | `	}` |
|   158 | 2168 | `	SyBlobRelease(&sOut);` |
|   158 | 2169 | `	return PH7_OK;` |
|    82 | 2170 | `}` |
|     - | 2171 |  |
|     - | 2172 | `/*` |
|     - | 2173 | ` * array\|false iconv_mime_decode_headers(string $headers, int $mode = 0,` |
|     - | 2174 | ` *                                       ?string $encoding = null)` |
|     - | 2175 | ` *` |
|     - | 2176 | ` * Decode one field at a time, splitting each at its FIRST ':' -- a line with` |
|     - | 2177 | ` * none is dropped entirely, which is what ends the walk at the blank line that` |
|     - | 2178 | ` * separates headers from a body. A name seen twice becomes a LIST, and the` |
|     - | 2179 | ` * first value is kept as a plain string until the second arrives.` |
|     - | 2180 | ` */` |
|    46 | 2181 | `static int PH7_builtin_iconv_mime_decode_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 2182 | `{` |
|     - | 2183 | `	const char *zIn;` |
|    47 | 2184 | `	int nIn,i = 0,err = ICV_OK;` |
|     - | 2185 | `	sxi64 iMode;` |
|     - | 2186 | `	icv_cs sCs;` |
|     - | 2187 | `	ph7_value *pArray,*pVal;` |
|    47 | 2188 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     3 | 2189 | `		ph7_result_bool(pCtx,0);` |
|     3 | 2190 | `		return PH7_OK;` |
|     - | 2191 | `	}` |
|    45 | 2192 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    45 | 2193 | `	pArray = ph7_context_new_array(pCtx);` |
|    45 | 2194 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    45 | 2195 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 2196 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2197 | `	}` |
|   117 | 2198 | `	while( i < nIn ){` |
|     - | 2199 | `		SyBlob sLine;` |
|     - | 2200 | `		const char *zLine;` |
|    77 | 2201 | `		int nLine,iColon,iNext = 0,k,bTouched = 0;` |
|    77 | 2202 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|    77 | 2203 | `		err = IcvMimeDecode(pCtx,&sLine,&zIn[i],nIn - i,&sCs,&iNext,&bTouched,(int)iMode);` |
|    77 | 2204 | `		if( err != ICV_OK ){` |
|     3 | 2205 | `			SyBlobRelease(&sLine);` |
|     4 | 2206 | `			break;` |
|     - | 2207 | `		}` |
|    75 | 2208 | `		nLine = (int)SyBlobLength(&sLine);` |
|    75 | 2209 | `		if( !bTouched ){` |
|     - | 2210 | `			/* php's buffer is still NULL here -- nothing was appended AT ALL,` |
|     - | 2211 | `			 * not even zero bytes -- and it leaves the loop. That is the blank` |
|     - | 2212 | `			 * line that ends a header block, so a body beyond it is never read;` |
|     - | 2213 | `			 * a field whose value is EMPTY did append and keeps the walk going. */` |
|     3 | 2214 | `			SyBlobRelease(&sLine);` |
|     3 | 2215 | `			break;` |
|     - | 2216 | `		}` |
|    73 | 2217 | `		zLine = (const char *)SyBlobData(&sLine);` |
|   333 | 2218 | `		for( iColon = 0 ; iColon < nLine && zLine[iColon] != ':' ; ++iColon ){}` |
|    73 | 2219 | `		if( iColon < nLine ){` |
|     - | 2220 | `			ph7_value *pSlot;` |
|    67 | 2221 | `			k = iColon + 1;` |
|   162 | 2222 | `			while( k < nLine && (zLine[k] == ' ' \|\| zLine[k] == '\t') ){ k++; }` |
|    67 | 2223 | `			ph7_value_string(pVal,&zLine[k],nLine - k);` |
|    67 | 2224 | `			pSlot = ph7_array_fetch(pArray,zLine,iColon);` |
|    67 | 2225 | `			if( pSlot == 0 ){` |
|    85 | 2226 | `				PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|    28 | 2227 | `					zLine,(sxu32)iColon,pVal);` |
|    29 | 2228 | `			}else{` |
|     - | 2229 | `				/* A name seen twice becomes a LIST -- and the first value was` |
|     - | 2230 | `				 * stored as a plain string, so it has to be lifted into one` |
|     - | 2231 | `				 * now, which is php's own shape for a repeated header. */` |
|    11 | 2232 | `				if( !ph7_value_is_array(pSlot) ){` |
|     7 | 2233 | `					ph7_value *pList = ph7_context_new_array(pCtx);` |
|     7 | 2234 | `					if( pList == 0 ){` |
|   ! 0 | 2235 | `						SyBlobRelease(&sLine);` |
|   ! 0 | 2236 | `						ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2237 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 2238 | `					}` |
|     7 | 2239 | `					ph7_array_add_strkey_elem(pList,0,pSlot);` |
|     7 | 2240 | `					ph7_array_add_strkey_elem(pList,0,pVal);` |
|    10 | 2241 | `					PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|     3 | 2242 | `						zLine,(sxu32)iColon,pList);` |
|     7 | 2243 | `					ph7_context_release_value(pCtx,pList);` |
|     4 | 2244 | `				}else{` |
|     5 | 2245 | `					ph7_array_add_strkey_elem(pSlot,0,pVal);` |
|     - | 2246 | `				}` |
|     - | 2247 | `			}` |
|    67 | 2248 | `			ph7_value_reset_string_cursor(pVal);` |
|    33 | 2249 | `		}` |
|    73 | 2250 | `		SyBlobRelease(&sLine);` |
|    73 | 2251 | `		if( iNext <= 0 ){` |
|   ! 0 | 2252 | `			break;` |
|     - | 2253 | `		}` |
|    73 | 2254 | `		i += iNext;` |
|     1 | 2255 | `	}` |
|    45 | 2256 | `	ph7_context_release_value(pCtx,pVal);` |
|    45 | 2257 | `	if( err != ICV_OK ){` |
|     3 | 2258 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|     3 | 2259 | `		ph7_result_bool(pCtx,0);` |
|     2 | 2260 | `	}else{` |
|    43 | 2261 | `		ph7_result_value(pCtx,pArray);` |
|     - | 2262 | `	}` |
|    45 | 2263 | `	return PH7_OK;` |
|    24 | 2264 | `}` |
|     - | 2265 |  |
|   443 | 2266 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv(pCtx,nArg,apArg); }` |
|    38 | 2267 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_get_encoding(pCtx,nArg,apArg); }` |
|    82 | 2268 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_encode(pCtx,nArg,apArg); }` |
|   162 | 2269 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode(pCtx,nArg,apArg); }` |
|    47 | 2270 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode_headers(pCtx,nArg,apArg); }` |
|    43 | 2271 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strlen(pCtx,nArg,apArg); }` |
|    62 | 2272 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_substr(pCtx,nArg,apArg); }` |
|    55 | 2273 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strpos(pCtx,nArg,apArg); }` |
|    25 | 2274 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strrpos(pCtx,nArg,apArg); }` |
|     - | 2275 |  |
|     - | 2276 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2277 |  |
