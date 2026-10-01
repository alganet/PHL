# src/ph7/builtin_iconv.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1112/1212 lines (91.75%)

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
|     - |   51 | ` * US-ASCII only (§10's scope cut, the same one mb_ carries); a php-valid name` |
|     - |   52 | ` * outside those three gets php's own "Wrong encoding" warning, which is what` |
|     - |   53 | ` * php answers for a name the platform's iconv does not have either.` |
|     - |   54 | ` *` |
|     - |   55 | ` * Verified differentially against php 8.5 over 16000 randomized conversions` |
|     - |   56 | ` * (every encoding pair, every suffix spelling, well-formed and malformed` |
|     - |   57 | ` * input). The one recorded divergence is in §7.4: an IGNORE token that is NOT` |
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
|     - |   68 | `#define ICV_UTF8    0` |
|     - |   69 | `#define ICV_LATIN1  1` |
|     - |   70 | `#define ICV_ASCII   2` |
|     - |   71 |  |
|     - |   72 | `/*` |
|     - |   73 | ` * php's own cap on an encoding NAME, checked before the name is looked at:` |
|     - |   74 | `` * ICONV_CSNMAXLEN is 64 and the test is `>=`, so a 64-character name is already`` |
|     - |   75 | ` * "exceeds the maximum allowed length of 64 characters".` |
|     - |   76 | ` */` |
|     - |   77 | `#define ICV_CSNMAXLEN 64` |
|     - |   78 |  |
|     - |   79 | `/*` |
|     - |   80 | ` * The names glibc registers for the three code sets PHL models, minus its` |
|     - |   81 | `` * numeric OSF/IBM aliases (`OSF00010020`, `IBM903`…), which are recorded as`` |
|     - |   82 | ` * refused rather than implemented. Compared after ICV normalisation, so the` |
|     - |   83 | ` * spelling here is the canonical upper-case one.` |
|     - |   84 | ` */` |
|     - |   85 | `static const struct IcvEncName {` |
|     - |   86 | `	const char *zName;` |
|     - |   87 | `	int iEnc;` |
|     - |   88 | `} aIcvEncName[] = {` |
|     - |   89 | `	{ "UTF-8",            ICV_UTF8   },` |
|     - |   90 | `	{ "UTF8",             ICV_UTF8   },` |
|     - |   91 | `	{ "ISO-IR-193",       ICV_UTF8   },` |
|     - |   92 | `	{ "ISO-8859-1",       ICV_LATIN1 },` |
|     - |   93 | `	{ "ISO_8859-1",       ICV_LATIN1 },` |
|     - |   94 | `	{ "ISO8859-1",        ICV_LATIN1 },` |
|     - |   95 | `	{ "ISO88591",         ICV_LATIN1 },` |
|     - |   96 | `	{ "ISO_8859-1:1987",  ICV_LATIN1 },` |
|     - |   97 | `	{ "ISO-IR-100",       ICV_LATIN1 },` |
|     - |   98 | `	{ "LATIN1",           ICV_LATIN1 },` |
|     - |   99 | `	{ "L1",               ICV_LATIN1 },` |
|     - |  100 | `	{ "CP819",            ICV_LATIN1 },` |
|     - |  101 | `	{ "IBM819",           ICV_LATIN1 },` |
|     - |  102 | `	{ "CSISOLATIN1",      ICV_LATIN1 },` |
|     - |  103 | `	{ "8859_1",           ICV_LATIN1 },` |
|     - |  104 | `	{ "ASCII",            ICV_ASCII  },` |
|     - |  105 | `	{ "US-ASCII",         ICV_ASCII  },` |
|     - |  106 | `	{ "US",               ICV_ASCII  },` |
|     - |  107 | `	{ "ANSI_X3.4-1968",   ICV_ASCII  },` |
|     - |  108 | `	{ "ANSI_X3.4-1986",   ICV_ASCII  },` |
|     - |  109 | `	{ "ANSI_X3.4",        ICV_ASCII  },` |
|     - |  110 | `	{ "ISO646-US",        ICV_ASCII  },` |
|     - |  111 | `	{ "ISO_646.IRV:1991", ICV_ASCII  },` |
|     - |  112 | `	{ "ISO-IR-6",         ICV_ASCII  },` |
|     - |  113 | `	{ "CP367",            ICV_ASCII  },` |
|     - |  114 | `	{ "IBM367",           ICV_ASCII  },` |
|     - |  115 | `	{ "CSASCII",          ICV_ASCII  }` |
|     - |  116 | `};` |
|     - |  117 |  |
|     - |  118 | `/* Is c one of the characters the name normaliser KEEPS? */` |
|  8783 |  119 | `static int IcvNameChar(int c)` |
|     4 |  120 | `{` |
|  9383 |  121 | `	return (c >= '0' && c <= '9') \|\| (c >= 'A' && c <= 'Z') \|\| (c >= 'a' && c <= 'z')` |
| 13352 |  122 | `		\|\| c == '_' \|\| c == '-' \|\| c == '.' \|\| c == ',' \|\| c == ':';` |
|     4 |  123 | `}` |
|  8801 |  124 | `static int IcvUpper(int c)` |
|     4 |  125 | `{` |
|  8805 |  126 | `	return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c;` |
|     4 |  127 | `}` |
|     - |  128 |  |
|     - |  129 | `/* What a parsed charset argument came to. */` |
|     - |  130 | `typedef struct icv_cs icv_cs;` |
|     - |  131 | `struct icv_cs {` |
|     - |  132 | `	int iEnc;        /* ICV_* id, or -1 when the name is not one PHL models */` |
|     - |  133 | `	int bTranslit;   /* a TRANSLIT token appeared in the error-handler list */` |
|     - |  134 | `	int bIgnore;     /* an IGNORE token did -- the LIBRARY's ignore, not php's */` |
|     - |  135 | `	/* The name as it was GIVEN, kept because the string family reports an` |
|     - |  136 | `	 * unknown one late -- at the point a conversion would have been opened --` |
|     - |  137 | `	 * and by then the argument that carried it may be gone. Bounded by the` |
|     - |  138 | `	 * length cap, which is checked before any of this. */` |
|     - |  139 | `	char zName[ICV_CSNMAXLEN];` |
|     - |  140 | `	int nName;` |
|     - |  141 | `};` |
|     - |  142 |  |
|     - |  143 | `/*` |
|     - |  144 | ` * How much of a charset argument the converter can SEE. php's ZPP hands the` |
|     - |  145 | ` * whole php string down, but iconv_open() takes a C string, so a name stops at` |
|     - |  146 | ` * its first NUL -- and so does the "Wrong encoding" message, which prints the` |
|     - |  147 | ` * same pointer with %s.` |
|     - |  148 | ` */` |
|  1710 |  149 | `static int IcvNameLen(const char *z,int n)` |
|     4 |  150 | `{` |
|     - |  151 | `	int i;` |
| 14479 |  152 | `	for( i = 0 ; i < n && z[i] != 0 ; ++i ){}` |
|  1714 |  153 | `	return i;` |
|     4 |  154 | `}` |
|     - |  155 | `/* Copy z[0..n-1] into zOut, keeping only the characters the normaliser keeps` |
|     - |  156 | ` * and upper-casing them. Answers the length written, capped at nOut. */` |
|  1502 |  157 | `static int IcvNormalize(char *zOut,int nOut,const char *z,int n)` |
|     4 |  158 | `{` |
|  1506 |  159 | `	int i,k = 0;` |
|  9149 |  160 | `	for( i = 0 ; i < n ; ++i ){` |
|  7647 |  161 | `		int c = (unsigned char)z[i];` |
|  7647 |  162 | `		if( IcvNameChar(c) && k < nOut ){` |
|  7633 |  163 | `			zOut[k++] = (char)IcvUpper(c);` |
|  3637 |  164 | `		}` |
|  3648 |  165 | `	}` |
|  1506 |  166 | `	return k;` |
|     4 |  167 | `}` |
|     - |  168 | `/*` |
|     - |  169 | ` * Parse one charset argument. The code-set name is z[0..] up to the first '/',` |
|     - |  170 | ` * normalised; the segment up to the second '/' must be empty; every remaining` |
|     - |  171 | `` * '/'- or ','-separated token is an error handler, and `TRANSLIT` is the one`` |
|     - |  172 | ` * that means anything here.` |
|     - |  173 | ` */` |
|  1356 |  174 | `static void IcvParseCharset(const char *z,int n,icv_cs *pCs)` |
|     4 |  175 | `{` |
|     - |  176 | `	char zBuf[ICV_CSNMAXLEN];` |
|     - |  177 | `	int iFirst,iSecond,nBuf,k;` |
|  1360 |  178 | `	pCs->iEnc = -1;` |
|  1360 |  179 | `	pCs->bTranslit = 0;` |
|  1360 |  180 | `	pCs->bIgnore = 0;` |
|  1360 |  181 | `	n = IcvNameLen(z,n);` |
|  1360 |  182 | `	pCs->nName = n < ICV_CSNMAXLEN ? n : ICV_CSNMAXLEN;` |
|  1360 |  183 | `	SyMemcpy(z,pCs->zName,(sxu32)pCs->nName);` |
|  8991 |  184 | `	for( iFirst = 0 ; iFirst < n && z[iFirst] != '/' ; ++iFirst ){}` |
|  1360 |  185 | `	nBuf = IcvNormalize(zBuf,(int)sizeof(zBuf),z,iFirst);` |
|  1360 |  186 | `	if( nBuf == 0 ){` |
|     - |  187 | `		/* php hands the name straight to iconv_open(), where an empty CODE SET` |
|     - |  188 | ``		 * means the LOCALE's charset -- which is why `"//IGNORE"` on its own`` |
|     - |  189 | `		 * names one. PHL has no locale and one charset it is written in, so the` |
|     - |  190 | `		 * empty name is UTF-8 here: what php answers on any UTF-8 locale,` |
|     - |  191 | `		 * deterministically rather than by environment. */` |
|     3 |  192 | `		pCs->iEnc = ICV_UTF8;` |
|     1 |  193 | `	}` |
|  6600 |  194 | `	for( k = 0 ; nBuf > 0 && k < (int)SX_ARRAYSIZE(aIcvEncName) ; ++k ){` |
|  6525 |  195 | `		const char *zCand = aIcvEncName[k].zName;` |
|  6525 |  196 | `		int nCand = (int)SyStrlen(zCand);` |
|  6525 |  197 | `		if( nCand == nBuf && SyMemcmp(zCand,zBuf,(sxu32)nCand) == 0 ){` |
|  1285 |  198 | `			pCs->iEnc = aIcvEncName[k].iEnc;` |
|  1285 |  199 | `			break;` |
|     - |  200 | `		}` |
|  2609 |  201 | `	}` |
|  1360 |  202 | `	if( iFirst >= n ){` |
|  1214 |  203 | `		return;` |
|     - |  204 | `	}` |
|     - |  205 | `	/* Between the first and second '/' is the segment that carries no handler,` |
|     - |  206 | ``	 * and it has to be empty: `ASCII/x/TRANSLIT` names nothing at all, which is`` |
|     - |  207 | ``	 * why php answers "Wrong encoding" for it while `ASCII/ /TRANSLIT` -- whose`` |
|     - |  208 | `	 * space the normaliser drops -- converts. */` |
|   159 |  209 | `	for( iSecond = iFirst + 1 ; iSecond < n && z[iSecond] != '/' ; ++iSecond ){}` |
|   147 |  210 | `	if( IcvNormalize(zBuf,(int)sizeof(zBuf),&z[iFirst+1],iSecond - iFirst - 1) > 0 ){` |
|     5 |  211 | `		pCs->iEnc = -1;` |
|     2 |  212 | `	}` |
|     - |  213 | `	/* Everything past it is a '/'- or ','-separated list of error handlers. */` |
|   147 |  214 | `	nBuf = 0;` |
|  1457 |  215 | `	for( k = iSecond + 1 ; k <= n ; ++k ){` |
|  1311 |  216 | `		int c = (k < n) ? (unsigned char)z[k] : '/';` |
|  1311 |  217 | `		if( c == '/' \|\| c == ',' ){` |
|   171 |  218 | `			if( nBuf == 8 && SyMemcmp(zBuf,"TRANSLIT",8) == 0 ){` |
|   109 |  219 | `				pCs->bTranslit = 1;` |
|    54 |  220 | `			}` |
|   171 |  221 | `			if( nBuf == 6 && SyMemcmp(zBuf,"IGNORE",6) == 0 ){` |
|    37 |  222 | `				pCs->bIgnore = 1;` |
|    18 |  223 | `			}` |
|   171 |  224 | `			nBuf = 0;` |
|   171 |  225 | `			continue;` |
|     - |  226 | `		}` |
|  1141 |  227 | `		if( IcvNameChar(c) ){` |
|  1141 |  228 | `			if( nBuf < (int)sizeof(zBuf) ){` |
|  1141 |  229 | `				zBuf[nBuf++] = (char)IcvUpper(c);` |
|   571 |  230 | `			}else{` |
|   ! 0 |  231 | `				nBuf = (int)sizeof(zBuf) + 1;` |
|     - |  232 | `			}` |
|   570 |  233 | `		}` |
|   571 |  234 | `	}` |
|   648 |  235 | `}` |
|     - |  236 |  |
|     - |  237 | `/*` |
|     - |  238 | `` * php's `_php_check_ignore()`: a case-SENSITIVE suffix test on the RAW name,`` |
|     - |  239 | ` * with the length guard that keeps the bare "//IGNORE" from matching.` |
|     - |  240 | ` */` |
|   282 |  241 | `static int IcvCheckIgnore(const char *z,int n)` |
|     1 |  242 | `{` |
|   283 |  243 | `	n = IcvNameLen(z,n);` |
|   283 |  244 | `	if( n >= 9 && SyMemcmp(&z[n-8],"//IGNORE",8) == 0 ){` |
|    29 |  245 | `		return 1;` |
|     - |  246 | `	}` |
|   255 |  247 | `	if( n >= 19 && SyMemcmp(&z[n-18],"//IGNORE//TRANSLIT",18) == 0 ){` |
|     3 |  248 | `		return 1;` |
|     - |  249 | `	}` |
|   253 |  250 | `	return 0;` |
|   142 |  251 | `}` |
|     - |  252 |  |
|     - |  253 | `/* --- Transliteration --------------------------------------------------- */` |
|     - |  254 |  |
|     - |  255 | `/*` |
|     - |  256 | ` * One transliteration row: the source code point and the offset/length of its` |
|     - |  257 | ` * replacement in the shared blob beside it. glibc's table gives an ORDERED list` |
|     - |  258 | ` * of candidate replacements and takes the first that fits the target, which for` |
|     - |  259 | ` * the two targets PHL can miss with comes to exactly two columns: the general` |
|     - |  260 | ` * (ASCII) answer here, and the 55 rows whose ISO-8859-1 answer is a different` |
|     - |  261 | ` * string.` |
|     - |  262 | ` */` |
|     - |  263 | `typedef struct icv_translit icv_translit;` |
|     - |  264 | `struct icv_translit {` |
|     - |  265 | `	sxu32 cp;` |
|     - |  266 | `	sxu16 iOfft;` |
|     - |  267 | `	sxu16 nByte;` |
|     - |  268 | `};` |
|     - |  269 | `#include "builtin_iconv_translit.h"` |
|     - |  270 |  |
|     - |  271 | `/* Binary-search a cp-sorted transliteration table. */` |
|   120 |  272 | `static const icv_translit * IcvTranslitFind(const icv_translit *aTab,int nTab,sxu32 cp)` |
|     1 |  273 | `{` |
|   121 |  274 | `	int lo = 0,hi = nTab - 1;` |
|  1271 |  275 | `	while( lo <= hi ){` |
|  1257 |  276 | `		int mid = lo + (hi - lo) / 2;` |
|  1257 |  277 | `		if( aTab[mid].cp == cp ){` |
|   107 |  278 | `			return &aTab[mid];` |
|     - |  279 | `		}` |
|  1151 |  280 | `		if( aTab[mid].cp < cp ){` |
|   525 |  281 | `			lo = mid + 1;` |
|   263 |  282 | `		}else{` |
|   627 |  283 | `			hi = mid - 1;` |
|     - |  284 | `		}` |
|     1 |  285 | `	}` |
|    15 |  286 | `	return 0;` |
|    61 |  287 | `}` |
|     - |  288 |  |
|     - |  289 | `/* --- The converter ----------------------------------------------------- */` |
|     - |  290 |  |
|     - |  291 | `/* php's php_iconv_err_t, in php's own order of preference. */` |
|     - |  292 | `#define ICV_OK            0` |
|     - |  293 | `#define ICV_ILLEGAL_CHAR  1   /* an incomplete sequence at the end of the input */` |
|     - |  294 | `#define ICV_ILLEGAL_SEQ   2   /* anything else the source encoding refuses */` |
|     - |  295 | `#define ICV_WRONG_CHARSET 3` |
|     - |  296 | `#define ICV_TOO_BIG       4` |
|     - |  297 | `#define ICV_MALFORMED     5` |
|     - |  298 | `#define ICV_OUT_BY_BOUNDS 6` |
|     - |  299 | `#define ICV_UNKNOWN_ERR   7` |
|     - |  300 | `#define ICV_TOO_LONG      8` |
|     - |  301 |  |
|     - |  302 | `/*` |
|     - |  303 | ` * Decode the character at z[0..n-1] under iEnc. Answers its byte length with` |
|     - |  304 | ` * *pCp set to the code point, or 0 with *pErr set to the diagnostic the` |
|     - |  305 | ` * sequence earns. The UTF-8 accepted is the original six-byte encoding, minus` |
|     - |  306 | ` * overlongs, the surrogate range and the C0/C1/FE/FF lead bytes -- glibc's set,` |
|     - |  307 | ` * not Unicode's.` |
|     - |  308 | ` */` |
|  6939 |  309 | `static int IcvDecode(const unsigned char *z,int n,int iEnc,sxu32 *pCp,int *pErr)` |
|     4 |  310 | `{` |
|     - |  311 | `	static const struct { unsigned char iLow,iHigh; int nSeq; sxu32 iMin; } aLead[] = {` |
|     - |  312 | `		{ 0xC2,0xDF,2,0x80       },` |
|     - |  313 | `		{ 0xE0,0xEF,3,0x800      },` |
|     - |  314 | `		{ 0xF0,0xF7,4,0x10000    },` |
|     - |  315 | `		{ 0xF8,0xFB,5,0x200000   },` |
|     - |  316 | `		{ 0xFC,0xFD,6,0x4000000  }` |
|     - |  317 | `	};` |
|  6943 |  318 | `	unsigned int c = z[0];` |
|     - |  319 | `	int i,k;` |
|  6943 |  320 | `	if( iEnc == ICV_LATIN1 ){` |
|   160 |  321 | `		*pCp = c;` |
|   160 |  322 | `		return 1;` |
|     - |  323 | `	}` |
|  6785 |  324 | `	if( iEnc == ICV_ASCII ){` |
|  1215 |  325 | `		if( c > 0x7F ){` |
|     8 |  326 | `			*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  327 | `			return 0;` |
|     - |  328 | `		}` |
|  1209 |  329 | `		*pCp = c;` |
|  1209 |  330 | `		return 1;` |
|     - |  331 | `	}` |
|  5572 |  332 | `	if( c < 0x80 ){` |
|  4819 |  333 | `		*pCp = c;` |
|  4819 |  334 | `		return 1;` |
|     - |  335 | `	}` |
|  1226 |  336 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(aLead) ; ++k ){` |
|     - |  337 | `		int nSeq;` |
|     - |  338 | `		sxu32 cp;` |
|  1174 |  339 | `		if( c < aLead[k].iLow \|\| c > aLead[k].iHigh ){` |
|   472 |  340 | `			continue;` |
|     - |  341 | `		}` |
|   704 |  342 | `		nSeq = aLead[k].nSeq;` |
|   704 |  343 | `		cp = c & (sxu32)(0x7F >> nSeq);` |
|  1531 |  344 | `		for( i = 1 ; i < nSeq ; ++i ){` |
|   864 |  345 | `			if( i >= n ){` |
|     - |  346 | `				/* Ran out mid-sequence with everything so far valid: php's` |
|     - |  347 | `				 * "incomplete multibyte character", not its "illegal" one. */` |
|    29 |  348 | `				*pErr = ICV_ILLEGAL_CHAR;` |
|    29 |  349 | `				return 0;` |
|     - |  350 | `			}` |
|   836 |  351 | `			if( (z[i] & 0xC0) != 0x80 ){` |
|     8 |  352 | `				*pErr = ICV_ILLEGAL_SEQ;` |
|     8 |  353 | `				return 0;` |
|     - |  354 | `			}` |
|   830 |  355 | `			cp = (cp << 6) \| (sxu32)(z[i] & 0x3F);` |
|   416 |  356 | `		}` |
|   670 |  357 | `		if( cp < aLead[k].iMin \|\| (cp >= 0xD800 && cp <= 0xDFFF) ){` |
|     9 |  358 | `			*pErr = ICV_ILLEGAL_SEQ;   /* overlong, or a lone surrogate */` |
|     9 |  359 | `			return 0;` |
|     - |  360 | `		}` |
|   662 |  361 | `		*pCp = cp;` |
|   662 |  362 | `		return nSeq;` |
|   ! 0 |  363 | `	}` |
|    53 |  364 | `	*pErr = ICV_ILLEGAL_SEQ;` |
|    53 |  365 | `	return 0;` |
|  3472 |  366 | `}` |
|     - |  367 |  |
|     - |  368 | `/* Write cp in iEnc when the encoding can hold it; 0 when it cannot. */` |
|  6053 |  369 | `static int IcvEncodeDirect(SyBlob *pOut,sxu32 cp,int iEnc)` |
|     3 |  370 | `{` |
|     - |  371 | `	unsigned char zEnc[6];` |
|  6056 |  372 | `	int n = 0;` |
|  6056 |  373 | `	if( iEnc == ICV_ASCII ){` |
|   653 |  374 | `		if( cp > 0x7F ){` |
|   137 |  375 | `			return 0;` |
|     1 |  376 | `		}` |
|  5662 |  377 | `	}else if( iEnc == ICV_LATIN1 ){` |
|   169 |  378 | `		if( cp > 0xFF ){` |
|    25 |  379 | `			return 0;` |
|     - |  380 | `		}` |
|    72 |  381 | `	}else{` |
|     - |  382 | `		/* The six-byte encoding, so every code point IcvDecode() produced can` |
|     - |  383 | `		 * be written back. */` |
|  5236 |  384 | `		if( cp < 0x80 ){` |
|  4856 |  385 | `			zEnc[n++] = (unsigned char)cp;` |
|  2808 |  386 | `		}else if( cp < 0x800 ){` |
|   371 |  387 | `			zEnc[n++] = (unsigned char)(0xC0 \| (cp >> 6));` |
|   371 |  388 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|   196 |  389 | `		}else if( cp < 0x10000 ){` |
|   ! 0 |  390 | `			zEnc[n++] = (unsigned char)(0xE0 \| (cp >> 12));` |
|   ! 0 |  391 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|   ! 0 |  392 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|    11 |  393 | `		}else if( cp < 0x200000 ){` |
|     7 |  394 | `			zEnc[n++] = (unsigned char)(0xF0 \| (cp >> 18));` |
|     7 |  395 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     7 |  396 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     7 |  397 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     8 |  398 | `		}else if( cp < 0x4000000 ){` |
|     3 |  399 | `			zEnc[n++] = (unsigned char)(0xF8 \| (cp >> 24));` |
|     3 |  400 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  401 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  402 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  403 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     2 |  404 | `		}else{` |
|     3 |  405 | `			zEnc[n++] = (unsigned char)(0xFC \| (cp >> 30));` |
|     3 |  406 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 24) & 0x3F));` |
|     3 |  407 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 18) & 0x3F));` |
|     3 |  408 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 12) & 0x3F));` |
|     3 |  409 | `			zEnc[n++] = (unsigned char)(0x80 \| ((cp >> 6) & 0x3F));` |
|     3 |  410 | `			zEnc[n++] = (unsigned char)(0x80 \| (cp & 0x3F));` |
|     - |  411 | `		}` |
|  5236 |  412 | `		SyBlobAppend(pOut,zEnc,(sxu32)n);` |
|  5236 |  413 | `		return 1;` |
|     - |  414 | `	}` |
|   661 |  415 | `	zEnc[0] = (unsigned char)cp;` |
|   661 |  416 | `	SyBlobAppend(pOut,zEnc,1);` |
|   661 |  417 | `	return 1;` |
|  3028 |  418 | `}` |
|     - |  419 |  |
|     - |  420 | `/*` |
|     - |  421 | ` * Write cp in iEnc, transliterating when bTranslit is set. Answers 0 only when` |
|     - |  422 | ` * the encoding cannot hold cp and transliteration is off -- the '?' a` |
|     - |  423 | ` * transliterated miss produces is glibc's own answer, and it is a SUCCESS, as` |
|     - |  424 | ` * is the EMPTY replacement a combining mark transliterates to.` |
|     - |  425 | ` */` |
|  6053 |  426 | `static int IcvEncode(SyBlob *pOut,sxu32 cp,int iEnc,int bTranslit)` |
|     3 |  427 | `{` |
|     - |  428 | `	const icv_translit *pRow;` |
|  6056 |  429 | `	if( IcvEncodeDirect(pOut,cp,iEnc) ){` |
|  5896 |  430 | `		return 1;` |
|     - |  431 | `	}` |
|   161 |  432 | `	if( !bTranslit ){` |
|    45 |  433 | `		return 0;` |
|     - |  434 | `	}` |
|   116 |  435 | `	if( iEnc == ICV_LATIN1` |
|    66 |  436 | `	 && (pRow = IcvTranslitFind(aIcvTranslit1,(int)SX_ARRAYSIZE(aIcvTranslit1),cp)) != 0 ){` |
|    11 |  437 | `		SyBlobAppend(pOut,&zIcvTranslit1[pRow->iOfft],pRow->nByte);` |
|    11 |  438 | `		return 1;` |
|     - |  439 | `	}` |
|   107 |  440 | `	pRow = IcvTranslitFind(aIcvTranslit,(int)SX_ARRAYSIZE(aIcvTranslit),cp);` |
|   107 |  441 | `	if( pRow ){` |
|    97 |  442 | `		SyBlobAppend(pOut,&zIcvTranslit[pRow->iOfft],pRow->nByte);` |
|    97 |  443 | `		return 1;` |
|     - |  444 | `	}` |
|    11 |  445 | `	SyBlobAppend(pOut,"?",1);` |
|    11 |  446 | `	return 1;` |
|  3028 |  447 | `}` |
|     - |  448 |  |
|     - |  449 | `/*` |
|     - |  450 | ` * One iconv(3) CALL over zIn[*pi..nIn-1]: convert until the input runs out or` |
|     - |  451 | ` * the library would stop, leaving *pi where it stopped and answering the status` |
|     - |  452 | ` * php's loop then reads.` |
|     - |  453 | ` *` |
|     - |  454 | ` * There are TWO ignore mechanisms and they are not the same one. The LIBRARY's` |
|     - |  455 | `` * (`pTo->bIgnore`, an IGNORE token anywhere in the error-handler list) drops a`` |
|     - |  456 | ` * character the TARGET cannot hold and carries on, and reports the whole call` |
|     - |  457 | ` * as an illegal sequence AFTERWARDS -- so on its own it only ever turns one` |
|     - |  458 | `` * failure into another, which is why `iconv("UTF-8","ASCII//IGNORE,X",…)` is`` |
|     - |  459 | `` * still FALSE. php's own (`bIgnore` in IcvConvert()) is what makes //IGNORE`` |
|     - |  460 | ` * useful, and it lives one level up.` |
|     - |  461 | ` *` |
|     - |  462 | ` * *pbDropped is that afterwards, and it is STICKY across the calls php makes:` |
|     - |  463 | ` * once a character has been dropped because the target could not hold it, a` |
|     - |  464 | ` * TRUNCATED tail is reported as an illegal sequence rather than an incomplete` |
|     - |  465 | ` * character -- which is what lets` |
|     - |  466 | `` * `iconv("UTF-8","ISO-8859-1//IGNORE","\xE1\x88\xA2ab\xFD")` answer "ab"`` |
|     - |  467 | ` * while the same string without the unconvertible character in front of it is a` |
|     - |  468 | ` * hard failure.` |
|     - |  469 | ` */` |
|  1287 |  470 | `static int IcvConvertCall(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  471 | `	int iFrom,const icv_cs *pTo,int *pbDropped)` |
|     3 |  472 | `{` |
|  1290 |  473 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|  1290 |  474 | `	int i = *pi,rc = ICV_OK;` |
|  3447 |  475 | `	while( i < nIn ){` |
|  2246 |  476 | `		sxu32 cp = 0;` |
|  2246 |  477 | `		int err = ICV_OK;` |
|  2246 |  478 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err);` |
|  2246 |  479 | `		if( nSeq == 0 ){` |
|    69 |  480 | `			rc = err;` |
|    78 |  481 | `			break;` |
|     - |  482 | `		}` |
|  2178 |  483 | `		if( IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit) ){` |
|  2136 |  484 | `			i += nSeq;` |
|  2148 |  485 | `			continue;` |
|     - |  486 | `		}` |
|    43 |  487 | `		if( pTo->bIgnore ){` |
|    25 |  488 | `			i += nSeq;` |
|    25 |  489 | `			*pbDropped = 1;` |
|    25 |  490 | `			continue;` |
|     - |  491 | `		}` |
|    19 |  492 | `		rc = ICV_ILLEGAL_SEQ;` |
|    19 |  493 | `		break;` |
|   ! 0 |  494 | `	}` |
|  1290 |  495 | `	if( *pbDropped && (rc == ICV_OK \|\| rc == ICV_ILLEGAL_CHAR) ){` |
|    15 |  496 | `		rc = ICV_ILLEGAL_SEQ;` |
|     7 |  497 | `	}` |
|  1290 |  498 | `	*pi = i;` |
|  1290 |  499 | `	return rc;` |
|     3 |  500 | `}` |
|     - |  501 |  |
|     - |  502 | `/*` |
|     - |  503 | ` * The whole of php_iconv_string(): drive IcvConvertCall() the way php's loop` |
|     - |  504 | `` * drives iconv(3). php's own `//IGNORE` handling sits HERE and is byte-granular`` |
|     - |  505 | ` * -- on an illegal sequence it steps the input forward by one byte and converts` |
|     - |  506 | ` * again, and a failure with a single byte left is taken for a success.` |
|     - |  507 | ` */` |
|  1271 |  508 | `static int IcvConvert(SyBlob *pOut,const char *zIn,int nIn,int iFrom,const icv_cs *pTo,int bIgnore)` |
|     3 |  509 | `{` |
|  1274 |  510 | `	int i = 0,bDropped = 0;` |
|   643 |  511 | `	for(;;){` |
|  1290 |  512 | `		int rc = IcvConvertCall(pOut,zIn,nIn,&i,iFrom,pTo,&bDropped);` |
|  1290 |  513 | `		if( rc == ICV_OK ){` |
|  1194 |  514 | `			return ICV_OK;` |
|     - |  515 | `		}` |
|    97 |  516 | `		if( bIgnore && rc == ICV_ILLEGAL_SEQ ){` |
|    31 |  517 | `			if( nIn - i <= 1 ){` |
|    15 |  518 | `				return ICV_OK;` |
|     - |  519 | `			}` |
|    17 |  520 | `			i++;` |
|    17 |  521 | `			continue;` |
|     - |  522 | `		}` |
|    67 |  523 | `		return rc;` |
|   ! 0 |  524 | `	}` |
|   638 |  525 | `}` |
|     - |  526 |  |
|     - |  527 | `/*` |
|     - |  528 | ` * Convert from zIn[*pi..] until the OUTPUT would pass iRoom bytes, leaving *pi` |
|     - |  529 | ` * on a character boundary and *pnTook set to how many characters went. This is` |
|     - |  530 | ` * how the MIME encoder fills one encoded word: it has a byte budget for the` |
|     - |  531 | ` * line and has to stop on a whole character, which is exactly what iconv(3)` |
|     - |  532 | ` * does when its output buffer runs out.` |
|     - |  533 | ` */` |
|   222 |  534 | `static int IcvConvertBounded(SyBlob *pOut,const char *zIn,int nIn,int *pi,` |
|     - |  535 | `	int iFrom,const icv_cs *pTo,sxi64 iRoom,int *pnTook)` |
|     1 |  536 | `{` |
|   223 |  537 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   223 |  538 | `	int i = *pi,nTook = 0;` |
|  4019 |  539 | `	while( i < nIn ){` |
|  3881 |  540 | `		sxu32 cp = 0;` |
|  3881 |  541 | `		int err = ICV_OK;` |
|  3881 |  542 | `		sxu32 nBefore = SyBlobLength(pOut);` |
|  3881 |  543 | `		int nSeq = IcvDecode(&z[i],nIn - i,iFrom,&cp,&err);` |
|  3881 |  544 | `		if( nSeq == 0 ){` |
|     3 |  545 | `			*pi = i;` |
|     3 |  546 | `			*pnTook = nTook;` |
|     4 |  547 | `			return err;` |
|     - |  548 | `		}` |
|  3879 |  549 | `		if( !IcvEncode(pOut,cp,pTo->iEnc,pTo->bTranslit) ){` |
|     3 |  550 | `			*pi = i;` |
|     3 |  551 | `			*pnTook = nTook;` |
|     3 |  552 | `			return ICV_ILLEGAL_SEQ;` |
|     - |  553 | `		}` |
|  3877 |  554 | `		if( (sxi64)SyBlobLength(pOut) > iRoom ){` |
|     - |  555 | `			/* One character too many: give the bytes back and stop here. */` |
|    81 |  556 | `			SyBlobLength(pOut) = nBefore;` |
|    81 |  557 | `			break;` |
|     - |  558 | `		}` |
|  3797 |  559 | `		i += nSeq;` |
|  3797 |  560 | `		nTook++;` |
|     1 |  561 | `	}` |
|   219 |  562 | `	*pi = i;` |
|   219 |  563 | `	*pnTook = nTook;` |
|   219 |  564 | `	return ICV_OK;` |
|   112 |  565 | `}` |
|     - |  566 |  |
|     - |  567 | `/*` |
|     - |  568 | ` * One buffer converted between two charsets NAMED the way a MO catalog's` |
|     - |  569 | `` * `Content-Type` header names them: ext/gettext's whole conversion door.`` |
|     - |  570 | ` *` |
|     - |  571 | ` * Transliteration is on unconditionally because glibc's gettext opens its own` |
|     - |  572 | `` * conversion that way (`norm_add_slashes (outcharset, "TRANSLIT")` in`` |
|     - |  573 | ` * loadmsgcat.c), which is why a translated string a narrower target cannot hold` |
|     - |  574 | ` * comes back approximated rather than refused. Answers -1 when either name is` |
|     - |  575 | ` * outside the three code sets PHL models, which is the case php answers the` |
|     - |  576 | ` * msgid UNTRANSLATED for -- the same thing its iconv_open() failure does.` |
|     - |  577 | ` */` |
|    48 |  578 | `PH7_PRIVATE int PH7_IconvTranslate(SyBlob *pOut,const char *zIn,int nIn,` |
|     - |  579 | `	const char *zFrom,int nFrom,const char *zTo,int nTo)` |
|     2 |  580 | `{` |
|     - |  581 | `	icv_cs sFrom,sTo;` |
|    50 |  582 | `	if( nFrom < 1 \|\| nTo < 1 ){` |
|   ! 0 |  583 | `		return -1;` |
|     - |  584 | `	}` |
|    50 |  585 | `	IcvParseCharset(zFrom,nFrom,&sFrom);` |
|    50 |  586 | `	IcvParseCharset(zTo,nTo,&sTo);` |
|    50 |  587 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|     2 |  588 | `		return -1;` |
|     - |  589 | `	}` |
|    49 |  590 | `	if( sFrom.iEnc == sTo.iEnc ){` |
|     - |  591 | `		/* Same code set: glibc's conversion is a copy, and so is this. */` |
|    48 |  592 | `		SyBlobAppend(pOut,zIn,(sxu32)nIn);` |
|    48 |  593 | `		return PH7_OK;` |
|     - |  594 | `	}` |
|     2 |  595 | `	sTo.bTranslit = 1;` |
|     2 |  596 | `	sTo.bIgnore = 0;` |
|     2 |  597 | `	return IcvConvert(pOut,zIn,nIn,sFrom.iEnc,&sTo,0) == ICV_OK ? PH7_OK : -1;` |
|     9 |  598 | `}` |
|     - |  599 |  |
|     - |  600 | `/* --- Diagnostics ------------------------------------------------------- */` |
|     - |  601 |  |
|     - |  602 | `/*` |
|     - |  603 | `` * php's `_php_iconv_show_error()`. The context prefixes the function name, so`` |
|     - |  604 | ` * these are the message bodies only; the two decoder diagnostics are E_NOTICE` |
|     - |  605 | ` * and everything else E_WARNING, which is php's own split.` |
|     - |  606 | ` */` |
|   196 |  607 | `static void IcvShowError(ph7_context *pCtx,int err,const char *zTo,int nTo,` |
|     - |  608 | `	const char *zFrom,int nFrom)` |
|     3 |  609 | `{` |
|   199 |  610 | `	switch( err ){` |
|   ! 0 |  611 | `	case ICV_OK:` |
|   ! 0 |  612 | `		break;` |
|    30 |  613 | `	case ICV_WRONG_CHARSET:` |
|    92 |  614 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  615 | `			"Wrong encoding, conversion from \"%.*s\" to \"%.*s\" is not allowed",` |
|    30 |  616 | `			nFrom,zFrom,nTo,zTo);` |
|    62 |  617 | `		break;` |
|    12 |  618 | `	case ICV_ILLEGAL_CHAR:` |
|    25 |  619 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  620 | `			"Detected an incomplete multibyte character in input string");` |
|    25 |  621 | `		break;` |
|    33 |  622 | `	case ICV_ILLEGAL_SEQ:` |
|    68 |  623 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  624 | `			"Detected an illegal character in input string");` |
|    68 |  625 | `		break;` |
|     4 |  626 | `	case ICV_TOO_BIG:` |
|     9 |  627 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Buffer length exceeded");` |
|     9 |  628 | `		break;` |
|    16 |  629 | `	case ICV_MALFORMED:` |
|    33 |  630 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Malformed string");` |
|    33 |  631 | `		break;` |
|     3 |  632 | `	case ICV_UNKNOWN_ERR:` |
|     - |  633 | ``		/* php prints `errno` here, and nothing has SET it -- the number is`` |
|     - |  634 | `		 * whatever the last libc call in the process left behind (22 and 84` |
|     - |  635 | `		 * are what the same input produces on this box, in the same run). PHL` |
|     - |  636 | `		 * has no stale errno to leak, so it says 0; twin-paired. */` |
|     7 |  637 | `		ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,"Unknown error (0)");` |
|     6 |  638 | `		break;` |
|   ! 0 |  639 | `	default:` |
|   ! 0 |  640 | `		break;` |
|     - |  641 | `	}` |
|   199 |  642 | `}` |
|     - |  643 |  |
|     - |  644 | `/*` |
|     - |  645 | ` * Read one charset ARGUMENT: php's length cap first (it is checked before the` |
|     - |  646 | ` * name is looked at, so an over-long name never reaches the "Wrong encoding"` |
|     - |  647 | ` * message), then the grammar. Answers 0 after raising the length warning.` |
|     - |  648 | ` */` |
|  1008 |  649 | `static int IcvCharsetArg(ph7_context *pCtx,const char *z,int n,icv_cs *pCs)` |
|     3 |  650 | `{` |
|  1011 |  651 | `	if( n >= ICV_CSNMAXLEN ){` |
|    11 |  652 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - |  653 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - |  654 | `			ICV_CSNMAXLEN);` |
|    11 |  655 | `		return 0;` |
|     - |  656 | `	}` |
|  1001 |  657 | `	IcvParseCharset(z,n,pCs);` |
|  1001 |  658 | `	return 1;` |
|   507 |  659 | `}` |
|     - |  660 |  |
|     - |  661 | `/* --- The functions ----------------------------------------------------- */` |
|     - |  662 |  |
|     - |  663 | `/* string\|false iconv(string $from_encoding, string $to_encoding, string $string) */` |
|   322 |  664 | `static int PH7_builtin_iconv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  665 | `{` |
|     - |  666 | `	const char *zFrom,*zTo,*zIn;` |
|     - |  667 | `	int nFrom,nTo,nIn,err;` |
|     - |  668 | `	icv_cs sFrom,sTo;` |
|     - |  669 | `	SyBlob sOut;` |
|   324 |  670 | `	if( nArg < 3 ){` |
|   ! 0 |  671 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  672 | `		return PH7_OK;` |
|     - |  673 | `	}` |
|   324 |  674 | `	zFrom = ph7_value_to_string(apArg[0],&nFrom);` |
|   324 |  675 | `	zTo = ph7_value_to_string(apArg[1],&nTo);` |
|   324 |  676 | `	zIn = ph7_value_to_string(apArg[2],&nIn);` |
|   324 |  677 | `	if( !IcvCharsetArg(pCtx,zFrom,nFrom,&sFrom) \|\| !IcvCharsetArg(pCtx,zTo,nTo,&sTo) ){` |
|     5 |  678 | `		ph7_result_bool(pCtx,0);` |
|     5 |  679 | `		return PH7_OK;` |
|     - |  680 | `	}` |
|   320 |  681 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|    56 |  682 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,` |
|    18 |  683 | `			zTo,IcvNameLen(zTo,nTo),zFrom,IcvNameLen(zFrom,nFrom));` |
|    38 |  684 | `		ph7_result_bool(pCtx,0);` |
|    38 |  685 | `		return PH7_OK;` |
|     - |  686 | `	}` |
|   283 |  687 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   283 |  688 | `	err = IcvConvert(&sOut,zIn,nIn,sFrom.iEnc,&sTo,IcvCheckIgnore(zTo,nTo));` |
|   283 |  689 | `	if( err != ICV_OK ){` |
|    63 |  690 | `		SyBlobRelease(&sOut);` |
|    63 |  691 | `		IcvShowError(pCtx,err,zTo,nTo,zFrom,nFrom);` |
|    63 |  692 | `		ph7_result_bool(pCtx,0);` |
|    63 |  693 | `		return PH7_OK;` |
|     - |  694 | `	}` |
|   221 |  695 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|   221 |  696 | `	SyBlobRelease(&sOut);` |
|   221 |  697 | `	return PH7_OK;` |
|   163 |  698 | `}` |
|     - |  699 |  |
|     - |  700 |  |
|     - |  701 | `/* --- The string quartet ------------------------------------------------ */` |
|     - |  702 |  |
|     - |  703 | `/*` |
|     - |  704 | ` * php's own name for the wide intermediate every one of these four converts` |
|     - |  705 | ` * THROUGH, and the one that shows up in their "Wrong encoding" message: the` |
|     - |  706 | ` * conversion that fails is the one INTO it, so the message always reads` |
|     - |  707 | `` * `from "<the argument>" to "UCS-4LE"`.`` |
|     - |  708 | ` */` |
|     - |  709 | `#define ICV_SUPERSET "UCS-4LE"` |
|     - |  710 |  |
|     - |  711 | `/*` |
|     - |  712 | `` * The `?string $encoding = null` the string family shares. A missing or null`` |
|     - |  713 | ``  * argument is php's INTERNAL encoding, which is `iconv.internal_encoding` `` |
|     - |  714 | `` * falling back to `default_charset` -- and since §10 removes the deprecated`` |
|     - |  715 | `` * `iconv.*` directives, `default_charset` is the whole of it here.`` |
|     - |  716 | ` *` |
|     - |  717 | ` * Only php's LENGTH cap is a diagnostic at this point (answers 0 for it). An` |
|     - |  718 | ` * unknown NAME is not, because php does not learn of one until it opens a` |
|     - |  719 | `` * conversion -- which is why `iconv_strpos($h, "", 0, "NOPE")` is a silent`` |
|     - |  720 | ` * false while the same call with a needle warns. IcvEncReady() is that moment.` |
|     - |  721 | ` */` |
|   366 |  722 | `static int IcvStrEncArg(ph7_context *pCtx,ph7_value *pArg,icv_cs *pCs)` |
|     3 |  723 | `{` |
|     - |  724 | `	SyBlob sIni;` |
|     - |  725 | `	int rc;` |
|   369 |  726 | `	if( pArg != 0 && !ph7_value_is_null(pArg) ){` |
|     - |  727 | `		int nEnc;` |
|    44 |  728 | `		const char *zEnc = ph7_value_to_string(pArg,&nEnc);` |
|    44 |  729 | `		return IcvCharsetArg(pCtx,zEnc,nEnc,pCs);` |
|     - |  730 | `	}` |
|   327 |  731 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|   327 |  732 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|   327 |  733 | `	rc = IcvCharsetArg(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni),pCs);` |
|   327 |  734 | `	SyBlobRelease(&sIni);` |
|   327 |  735 | `	return rc;` |
|   186 |  736 | `}` |
|     - |  737 | `/*` |
|     - |  738 | ` * The "Wrong encoding" php raises where it would have opened the conversion.` |
|     - |  739 | ` * The conversion the string family opens is the one INTO the wide intermediate,` |
|     - |  740 | ` * so the message always names UCS-4LE as the target. Answers 0 after raising.` |
|     - |  741 | ` */` |
|   152 |  742 | `static int IcvEncReady(ph7_context *pCtx,const icv_cs *pCs)` |
|     2 |  743 | `{` |
|   154 |  744 | `	if( pCs->iEnc >= 0 ){` |
|   148 |  745 | `		return 1;` |
|     - |  746 | `	}` |
|    10 |  747 | `	IcvShowError(pCtx,ICV_WRONG_CHARSET,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,` |
|     6 |  748 | `		pCs->zName,pCs->nName);` |
|     7 |  749 | `	return 0;` |
|    78 |  750 | `}` |
|     - |  751 |  |
|     - |  752 | `/*` |
|     - |  753 | ` * A decoded string: one code point per character plus the byte offset each one` |
|     - |  754 | ` * starts at (nChar+1 entries, so the last is the buffer length). php works the` |
|     - |  755 | ` * same way -- it converts to UCS-4 and operates there -- and it is what lets a` |
|     - |  756 | ` * search answer in CHARACTERS while a slice is taken in BYTES.` |
|     - |  757 | ` */` |
|     - |  758 | `typedef struct icv_text icv_text;` |
|     - |  759 | `struct icv_text {` |
|     - |  760 | `	const char *zIn;` |
|     - |  761 | `	int nByte;` |
|     - |  762 | `	sxu32 *aCode;` |
|     - |  763 | `	int *aOfft;` |
|     - |  764 | `	int nChar;` |
|     - |  765 | `};` |
|     - |  766 | `#define ICV_TEXT_MAX 0x0FFFFFFF` |
|     - |  767 |  |
|     - |  768 | `/*` |
|     - |  769 | ` * Decode zIn under iEnc into pText. Answers PH7_OK, or PH7_OK with *pErr set to` |
|     - |  770 | ` * the diagnostic the input earns -- in which case pText is not usable but was` |
|     - |  771 | ` * still allocated, so IcvTextRelease() is safe either way.` |
|     - |  772 | ` */` |
|   200 |  773 | `static int IcvTextDecode(ph7_context *pCtx,icv_text *pText,const char *zIn,int nByte,` |
|     - |  774 | `	int iEnc,int *pErr)` |
|     2 |  775 | `{` |
|   202 |  776 | `	const unsigned char *z = (const unsigned char *)zIn;` |
|   202 |  777 | `	int i = 0,n = 0,nSlot;` |
|   202 |  778 | `	pText->zIn = zIn;` |
|   202 |  779 | `	pText->nByte = nByte;` |
|   202 |  780 | `	pText->nChar = 0;` |
|   202 |  781 | `	pText->aCode = 0;` |
|   202 |  782 | `	pText->aOfft = 0;` |
|   202 |  783 | `	*pErr = ICV_OK;` |
|   202 |  784 | `	if( nByte > ICV_TEXT_MAX ){` |
|   ! 0 |  785 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  786 | `	}` |
|   202 |  787 | `	nSlot = nByte + 1;` |
|   402 |  788 | `	pText->aCode = (sxu32 *)ph7_context_alloc_chunk(pCtx,` |
|   200 |  789 | `		(unsigned int)((sxu32)nSlot * (sizeof(sxu32) + sizeof(int))),FALSE,TRUE);` |
|   202 |  790 | `	if( pText->aCode == 0 ){` |
|   ! 0 |  791 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  792 | `	}` |
|   202 |  793 | `	pText->aOfft = (int *)&pText->aCode[nSlot];` |
|   988 |  794 | `	while( i < nByte ){` |
|   818 |  795 | `		sxu32 cp = 0;` |
|   818 |  796 | `		int nSeq = IcvDecode(&z[i],nByte - i,iEnc,&cp,pErr);` |
|   818 |  797 | `		if( nSeq == 0 ){` |
|     - |  798 | `			/* The good PREFIX is the answer here, not zero: php's own walk` |
|     - |  799 | `			 * stops at the bad character and everything before it has already` |
|     - |  800 | `			 * been converted, so nChar is how far a search got and the count` |
|     - |  801 | `			 * an out-of-bounds $offset is measured against. */` |
|    32 |  802 | `			pText->aOfft[n] = i;` |
|    32 |  803 | `			pText->nChar = n;` |
|    32 |  804 | `			return PH7_OK;` |
|     - |  805 | `		}` |
|   788 |  806 | `		pText->aCode[n] = cp;` |
|   788 |  807 | `		pText->aOfft[n] = i;` |
|   788 |  808 | `		i += nSeq;` |
|   788 |  809 | `		n++;` |
|     2 |  810 | `	}` |
|   172 |  811 | `	pText->aOfft[n] = nByte;` |
|   172 |  812 | `	pText->nChar = n;` |
|   172 |  813 | `	return PH7_OK;` |
|   102 |  814 | `}` |
|   200 |  815 | `static void IcvTextRelease(ph7_context *pCtx,icv_text *pText)` |
|     2 |  816 | `{` |
|   202 |  817 | `	if( pText->aCode ){` |
|   202 |  818 | `		ph7_context_free_chunk(pCtx,pText->aCode);` |
|   202 |  819 | `		pText->aCode = 0;` |
|   100 |  820 | `	}` |
|   202 |  821 | `}` |
|     - |  822 |  |
|     - |  823 | `/* int\|false iconv_strlen(string $string, ?string $encoding = null) */` |
|    38 |  824 | `static int PH7_builtin_iconv_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  825 | `{` |
|     - |  826 | `	const char *zIn;` |
|    40 |  827 | `	int nIn,err = ICV_OK;` |
|     - |  828 | `	icv_cs sCs;` |
|     - |  829 | `	icv_text sText;` |
|    38 |  830 | `	if( nArg < 1 \|\| !IcvStrEncArg(pCtx,nArg > 1 ? apArg[1] : 0,&sCs)` |
|    39 |  831 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     5 |  832 | `		ph7_result_bool(pCtx,0);` |
|     5 |  833 | `		return PH7_OK;` |
|     - |  834 | `	}` |
|    36 |  835 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    36 |  836 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  837 | `		return PH7_OK;` |
|     - |  838 | `	}` |
|    36 |  839 | `	IcvTextRelease(pCtx,&sText);` |
|    36 |  840 | `	if( err != ICV_OK ){` |
|    12 |  841 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|    12 |  842 | `		ph7_result_bool(pCtx,0);` |
|    12 |  843 | `		return PH7_OK;` |
|     - |  844 | `	}` |
|    26 |  845 | `	ph7_result_int(pCtx,sText.nChar);` |
|    26 |  846 | `	return PH7_OK;` |
|    21 |  847 | `}` |
|     - |  848 |  |
|     - |  849 | `/*` |
|     - |  850 | ` * string\|false iconv_substr(string $string, int $offset, ?int $length = null,` |
|     - |  851 | ` *                           ?string $encoding = null)` |
|     - |  852 | ` *` |
|     - |  853 | ` * php clamps in the order its own code does, and the order is visible: a null` |
|     - |  854 | ` * $length starts life as the string's BYTE count and is then clamped to the` |
|     - |  855 | ` * character count, so it can never reach past the end however the two differ.` |
|     - |  856 | ` */` |
|    52 |  857 | `static int PH7_builtin_iconv_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 |  858 | `{` |
|     - |  859 | `	const char *zIn;` |
|    54 |  860 | `	int nIn,err = ICV_OK,iStart,iStop;` |
|     - |  861 | `	sxi64 iOfft,iLen;` |
|     - |  862 | `	icv_cs sCs;` |
|     - |  863 | `	icv_text sText;` |
|    52 |  864 | `	if( nArg < 2 \|\| !IcvStrEncArg(pCtx,nArg > 3 ? apArg[3] : 0,&sCs)` |
|    54 |  865 | `	 \|\| !IcvEncReady(pCtx,&sCs) ){` |
|     3 |  866 | `		ph7_result_bool(pCtx,0);` |
|     3 |  867 | `		return PH7_OK;` |
|     - |  868 | `	}` |
|    52 |  869 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    52 |  870 | `	if( PH7_IntArgResolve(pCtx,apArg[1],"iconv_substr",2,"$offset","int",&iOfft) != PH7_OK ){` |
|   ! 0 |  871 | `		return PH7_OK;` |
|     - |  872 | `	}` |
|    52 |  873 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|    32 |  874 | `		if( PH7_IntArgResolve(pCtx,apArg[2],"iconv_substr",3,"$length","?int",&iLen) != PH7_OK ){` |
|   ! 0 |  875 | `			return PH7_OK;` |
|     - |  876 | `		}` |
|    17 |  877 | `	}else{` |
|    22 |  878 | `		iLen = nIn;` |
|     - |  879 | `	}` |
|    52 |  880 | `	if( IcvTextDecode(pCtx,&sText,zIn,nIn,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 |  881 | `		return PH7_OK;` |
|     - |  882 | `	}` |
|    52 |  883 | `	if( err != ICV_OK ){` |
|     5 |  884 | `		IcvTextRelease(pCtx,&sText);` |
|     5 |  885 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 |  886 | `		ph7_result_bool(pCtx,0);` |
|     5 |  887 | `		return PH7_OK;` |
|     - |  888 | `	}` |
|    48 |  889 | `	if( iOfft < 0 ){` |
|    11 |  890 | `		iOfft += sText.nChar;` |
|    11 |  891 | `		if( iOfft < 0 ){` |
|     7 |  892 | `			iOfft = 0;` |
|     4 |  893 | `		}` |
|    43 |  894 | `	}else if( iOfft > sText.nChar ){` |
|     5 |  895 | `		iOfft = sText.nChar;` |
|     2 |  896 | `	}` |
|    48 |  897 | `	if( iLen < 0 ){` |
|     9 |  898 | `		iLen += sText.nChar - iOfft;` |
|     9 |  899 | `		if( iLen < 0 ){` |
|     5 |  900 | `			iLen = 0;` |
|     3 |  901 | `		}` |
|    44 |  902 | `	}else if( iLen > sText.nChar ){` |
|    23 |  903 | `		iLen = sText.nChar;` |
|    11 |  904 | `	}` |
|    48 |  905 | `	if( iOfft + iLen > sText.nChar ){` |
|    16 |  906 | `		iLen = sText.nChar - iOfft;` |
|     7 |  907 | `	}` |
|    48 |  908 | `	iStart = sText.aOfft[iOfft];` |
|    48 |  909 | `	iStop = sText.aOfft[iOfft + iLen];` |
|    48 |  910 | `	ph7_result_string(pCtx,&zIn[iStart],iStop - iStart);` |
|    48 |  911 | `	IcvTextRelease(pCtx,&sText);` |
|    48 |  912 | `	return PH7_OK;` |
|    28 |  913 | `}` |
|     - |  914 |  |
|     - |  915 | `/*` |
|     - |  916 | ` * The search both position builtins run: the first match at or after iFrom, or` |
|     - |  917 | ` * the LAST one when bReverse is set. Answers the character index or -1.` |
|     - |  918 | ` */` |
|    54 |  919 | `static int IcvSearch(const icv_text *pH,const icv_text *pN,int iFrom,int bReverse)` |
|     2 |  920 | `{` |
|    56 |  921 | `	int i,iFound = -1;` |
|    56 |  922 | `	if( pN->nChar < 1 \|\| pN->nChar > pH->nChar ){` |
|    11 |  923 | `		return -1;` |
|     - |  924 | `	}` |
|   128 |  925 | `	for( i = iFrom ; i + pN->nChar <= pH->nChar ; ++i ){` |
|     - |  926 | `		int k;` |
|   180 |  927 | `		for( k = 0 ; k < pN->nChar && pH->aCode[i+k] == pN->aCode[k] ; ++k ){}` |
|   106 |  928 | `		if( k == pN->nChar ){` |
|    48 |  929 | `			if( !bReverse ){` |
|    24 |  930 | `				return i;` |
|     - |  931 | `			}` |
|    25 |  932 | `			iFound = i;` |
|    12 |  933 | `		}` |
|    43 |  934 | `	}` |
|    23 |  935 | `	return iFound;` |
|    29 |  936 | `}` |
|     - |  937 |  |
|     - |  938 | `/*` |
|     - |  939 | ` * int\|false iconv_strpos(string $haystack, string $needle, int $offset = 0,` |
|     - |  940 | ` *                        ?string $encoding = null)` |
|     - |  941 | ` * int\|false iconv_strrpos(string $haystack, string $needle,` |
|     - |  942 | ` *                         ?string $encoding = null)` |
|     - |  943 | ` *` |
|     - |  944 | ` * One body, because php's two differ only in four places: strrpos has no` |
|     - |  945 | ` * $offset at all, it tests the empty needle BEFORE the encoding is looked at` |
|     - |  946 | ` * (so an over-long name is silent there and warns in strpos), it keeps looking` |
|     - |  947 | ` * after a match instead of stopping at the first, and it has no out-of-bounds` |
|     - |  948 | ` * ValueError to raise.` |
|     - |  949 | ` *` |
|     - |  950 | ` * The ORDER below is php's and it is visible from the outside, because the two` |
|     - |  951 | ` * halves of the search report differently. The NEEDLE goes through a whole` |
|     - |  952 | ` * conversion, so an ill-formed one raises the same two diagnostics anything` |
|     - |  953 | ` * else does. The HAYSTACK is walked one character at a time into a buffer` |
|     - |  954 | ` * exactly one wide, and php's loop leaves that walk the moment a character` |
|     - |  955 | ` * fails to convert -- WITHOUT recording why. So an ill-formed haystack is` |
|     - |  956 | ` * silent: the search simply cannot see past the bad byte, and` |
|     - |  957 | `` * `iconv_strpos("ab\xFFcd","cd")` is FALSE with nothing said. What it does`` |
|     - |  958 | ` * decide is the count the out-of-bounds ValueError is measured against, which` |
|     - |  959 | ` * is why an $offset past the first bad byte raises where the same call on a` |
|     - |  960 | ` * clean string answers false.` |
|     - |  961 | ` */` |
|    74 |  962 | `static int IcvStrposBody(ph7_context *pCtx,int nArg,ph7_value **apArg,int bReverse)` |
|     2 |  963 | `{` |
|     - |  964 | `	const char *zH,*zN;` |
|    76 |  965 | `	int nH,nN,err = ICV_OK,iScanned,iFound;` |
|    76 |  966 | `	sxi64 iOfft = 0;` |
|     - |  967 | `	icv_cs sCs;` |
|     - |  968 | `	icv_text sH,sN;` |
|    76 |  969 | `	if( nArg < 2 ){` |
|   ! 0 |  970 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  971 | `		return PH7_OK;` |
|     - |  972 | `	}` |
|    76 |  973 | `	zH = ph7_value_to_string(apArg[0],&nH);` |
|    76 |  974 | `	zN = ph7_value_to_string(apArg[1],&nN);` |
|    76 |  975 | `	if( bReverse && nN < 1 ){` |
|     - |  976 | `		/* php's order: the empty needle answers false before the name is` |
|     - |  977 | `		 * measured, which is why an over-long $encoding is silent here. */` |
|     5 |  978 | `		ph7_result_bool(pCtx,0);` |
|     5 |  979 | `		return PH7_OK;` |
|     - |  980 | `	}` |
|    72 |  981 | `	if( !IcvStrEncArg(pCtx,nArg > (bReverse ? 2 : 3) ? apArg[bReverse ? 2 : 3] : 0,&sCs) ){` |
|     3 |  982 | `		ph7_result_bool(pCtx,0);` |
|     3 |  983 | `		return PH7_OK;` |
|     - |  984 | `	}` |
|    68 |  985 | `	if( !bReverse && nArg > 2` |
|    38 |  986 | `	 && PH7_IntArgResolve(pCtx,apArg[2],"iconv_strpos",3,"$offset","int",&iOfft) != PH7_OK ){` |
|     - |  987 | ``		/* $offset is php's plain `int`, so a null is the deprecation §10 turns`` |
|     - |  988 | `		 * into a TypeError -- and it has to be REFUSED here rather than skipped,` |
|     - |  989 | `		 * which is what treating a null argument as "not passed" would do. */` |
|   ! 0 |  990 | `		return PH7_OK;` |
|     - |  991 | `	}` |
|    70 |  992 | `	if( iOfft < 0 ){` |
|     - |  993 | `		/* A negative offset counts from the end, so THIS is the one path that` |
|     - |  994 | `		 * measures the haystack before the needle -- and the one place an` |
|     - |  995 | `		 * ill-formed haystack is heard about at all. */` |
|     5 |  996 | `		if( !IcvEncReady(pCtx,&sCs) ){` |
|   ! 0 |  997 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  998 | `			return PH7_OK;` |
|     - |  999 | `		}` |
|     5 | 1000 | `		if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1001 | `			return PH7_OK;` |
|     - | 1002 | `		}` |
|     5 | 1003 | `		IcvTextRelease(pCtx,&sH);` |
|     5 | 1004 | `		if( err != ICV_OK ){` |
|   ! 0 | 1005 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|   ! 0 | 1006 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 | 1007 | `			return PH7_OK;` |
|     - | 1008 | `		}` |
|     5 | 1009 | `		iOfft += sH.nChar;` |
|     5 | 1010 | `		if( iOfft < 0 ){` |
|     3 | 1011 | `			PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1012 | `				"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     3 | 1013 | `			return PH7_OK;` |
|     - | 1014 | `		}` |
|     1 | 1015 | `	}` |
|    68 | 1016 | `	if( nN < 1 ){` |
|     7 | 1017 | `		ph7_result_bool(pCtx,0);` |
|     7 | 1018 | `		return PH7_OK;` |
|     - | 1019 | `	}` |
|     - | 1020 | `	/* php converts the NEEDLE whole before it scans, so an ill-formed needle is` |
|     - | 1021 | `	 * the same two diagnostics as an ill-formed argument anywhere else. */` |
|    62 | 1022 | `	if( !IcvEncReady(pCtx,&sCs) ){` |
|     3 | 1023 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1024 | `		return PH7_OK;` |
|     - | 1025 | `	}` |
|    60 | 1026 | `	if( IcvTextDecode(pCtx,&sN,zN,nN,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1027 | `		return PH7_OK;` |
|     - | 1028 | `	}` |
|    60 | 1029 | `	if( err != ICV_OK ){` |
|     5 | 1030 | `		IcvTextRelease(pCtx,&sN);` |
|     5 | 1031 | `		IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1032 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1033 | `		return PH7_OK;` |
|     - | 1034 | `	}` |
|    56 | 1035 | `	if( IcvTextDecode(pCtx,&sH,zH,nH,sCs.iEnc,&err) != PH7_OK ){` |
|   ! 0 | 1036 | `		IcvTextRelease(pCtx,&sN);` |
|   ! 0 | 1037 | `		return PH7_OK;` |
|     - | 1038 | `	}` |
|    56 | 1039 | `	iScanned = sH.nChar;` |
|    56 | 1040 | `	iFound = IcvSearch(&sH,&sN,bReverse ? 0 : (int)iOfft,bReverse);` |
|    56 | 1041 | `	IcvTextRelease(pCtx,&sN);` |
|    56 | 1042 | `	IcvTextRelease(pCtx,&sH);` |
|    56 | 1043 | `	if( err != ICV_OK ){` |
|     - | 1044 | `		/* Whether the walk SAYS anything about the bad character depends on how` |
|     - | 1045 | `		 * far it got, because php hears about it from the call that converted` |
|     - | 1046 | `		 * the character BEFORE it. An error at index 0 is therefore silent, and` |
|     - | 1047 | `		 * so is one the walk never reached -- a full match ends the walk, so` |
|     - | 1048 | ``		 * `iconv_strpos(str_repeat("x",100)."\xFF","xx")` answers 0 with`` |
|     - | 1049 | ``		 * nothing said while `iconv_strpos("ab\xFF","ab")` answers FALSE with`` |
|     - | 1050 | `		 * the notice. */` |
|    13 | 1051 | `		int iStop = (!bReverse && iFound >= 0) ? iFound + sN.nChar - 1 : iScanned;` |
|    13 | 1052 | `		if( iScanned >= 1 && iStop >= iScanned - 1 ){` |
|     5 | 1053 | `			IcvShowError(pCtx,err,ICV_SUPERSET,(int)sizeof(ICV_SUPERSET)-1,"",0);` |
|     5 | 1054 | `			ph7_result_bool(pCtx,0);` |
|     5 | 1055 | `			return PH7_OK;` |
|     - | 1056 | `		}` |
|     4 | 1057 | `	}` |
|    52 | 1058 | `	if( !bReverse && iOfft > iScanned ){` |
|     5 | 1059 | `		PH7_VmThrowException(pCtx,"ValueError",` |
|     - | 1060 | `			"iconv_strpos(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|     5 | 1061 | `		return PH7_OK;` |
|     - | 1062 | `	}` |
|    48 | 1063 | `	if( iFound < 0 ){` |
|    15 | 1064 | `		ph7_result_bool(pCtx,0);` |
|     8 | 1065 | `	}else{` |
|    34 | 1066 | `		ph7_result_int(pCtx,iFound);` |
|     - | 1067 | `	}` |
|    48 | 1068 | `	return PH7_OK;` |
|    39 | 1069 | `}` |
|    50 | 1070 | `static int PH7_builtin_iconv_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1071 | `{` |
|    52 | 1072 | `	return IcvStrposBody(pCtx,nArg,apArg,0);` |
|     2 | 1073 | `}` |
|    24 | 1074 | `static int PH7_builtin_iconv_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1075 | `{` |
|    25 | 1076 | `	return IcvStrposBody(pCtx,nArg,apArg,1);` |
|     1 | 1077 | `}` |
|     - | 1078 |  |
|     - | 1079 | `/* --- The encoding settings --------------------------------------------- */` |
|     - | 1080 |  |
|     - | 1081 | `/*` |
|     - | 1082 | ` * array\|string\|false iconv_get_encoding(string $type = "all")` |
|     - | 1083 | ` *` |
|     - | 1084 | `` * php answers `iconv.input_encoding` / `output_encoding` / `internal_encoding`,`` |
|     - | 1085 | `` * each falling back to `default_charset`. §10 removes all three of those`` |
|     - | 1086 | ` * directives -- every one of them is deprecated, which is also why this` |
|     - | 1087 | ` * function's SETTER counterpart is not here at all (see the twin pair in` |
|     - | 1088 | `` * 002-integration) -- so `default_charset` is what all three answer, and`` |
|     - | 1089 | ` * moving it moves them together, exactly as php does when the iconv directives` |
|     - | 1090 | ` * are left unset. $type matches case-insensitively; anything else is FALSE,` |
|     - | 1091 | ` * with no diagnostic of any kind.` |
|     - | 1092 | ` */` |
|    36 | 1093 | `static int PH7_builtin_iconv_get_encoding(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1094 | `{` |
|     - | 1095 | `	static const char * const azType[] = { "input_encoding", "output_encoding", "internal_encoding" };` |
|     - | 1096 | `	SyBlob sIni;` |
|    38 | 1097 | `	const char *zType = "all";` |
|    38 | 1098 | `	int nType = 3,k;` |
|    38 | 1099 | `	if( nArg > 0 ){` |
|    34 | 1100 | `		zType = ph7_value_to_string(apArg[0],&nType);` |
|    16 | 1101 | `	}` |
|    38 | 1102 | `	SyBlobInit(&sIni,&pCtx->pVm->sAllocator);` |
|    38 | 1103 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIni);` |
|    38 | 1104 | `	if( nType == 3 && SyStrnicmp(zType,"all",3) == 0 ){` |
|    11 | 1105 | `		ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    11 | 1106 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    11 | 1107 | `		if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 1108 | `			SyBlobRelease(&sIni);` |
|   ! 0 | 1109 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 1110 | `		}` |
|    11 | 1111 | `		ph7_value_string(pVal,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    41 | 1112 | `		for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    31 | 1113 | `			ph7_array_add_strkey_elem(pArray,azType[k],pVal);` |
|    16 | 1114 | `		}` |
|    11 | 1115 | `		ph7_result_value(pCtx,pArray);` |
|    11 | 1116 | `		ph7_context_release_value(pCtx,pVal);` |
|    11 | 1117 | `		SyBlobRelease(&sIni);` |
|    11 | 1118 | `		return PH7_OK;` |
|     - | 1119 | `	}` |
|    80 | 1120 | `	for( k = 0 ; k < (int)SX_ARRAYSIZE(azType) ; ++k ){` |
|    70 | 1121 | `		if( nType == (int)SyStrlen(azType[k]) && SyStrnicmp(zType,azType[k],(sxu32)nType) == 0 ){` |
|    18 | 1122 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&sIni),(int)SyBlobLength(&sIni));` |
|    18 | 1123 | `			SyBlobRelease(&sIni);` |
|    18 | 1124 | `			return PH7_OK;` |
|     - | 1125 | `		}` |
|    28 | 1126 | `	}` |
|    11 | 1127 | `	SyBlobRelease(&sIni);` |
|    11 | 1128 | `	ph7_result_bool(pCtx,0);` |
|    11 | 1129 | `	return PH7_OK;` |
|    20 | 1130 | `}` |
|     - | 1131 |  |
|     - | 1132 | `/* --- MIME header words -------------------------------------------------- */` |
|     - | 1133 |  |
|     - | 1134 | `/*` |
|     - | 1135 | ` * Convert zIn into pOut, appending NOTHING unless the whole run converts. This` |
|     - | 1136 | ` * is php's _php_iconv_appendl(), which builds its own buffer and only hands it` |
|     - | 1137 | ` * over on success -- so a header run that fails part-way leaves no partial` |
|     - | 1138 | ` * bytes behind, which is visible whenever CONTINUE_ON_ERROR lets the scan go on` |
|     - | 1139 | ` * afterwards.` |
|     - | 1140 | ` */` |
|   988 | 1141 | `static int IcvAppendConv(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1142 | `	int iFrom,const icv_cs *pTo)` |
|     2 | 1143 | `{` |
|     - | 1144 | `	SyBlob sTmp;` |
|     - | 1145 | `	int rc;` |
|   990 | 1146 | `	SyBlobInit(&sTmp,&pCtx->pVm->sAllocator);` |
|   990 | 1147 | `	rc = IcvConvert(&sTmp,zIn,nIn,iFrom,pTo,0);` |
|   990 | 1148 | `	if( rc == ICV_OK ){` |
|   986 | 1149 | `		SyBlobAppend(pOut,SyBlobData(&sTmp),SyBlobLength(&sTmp));` |
|   492 | 1150 | `	}` |
|   990 | 1151 | `	SyBlobRelease(&sTmp);` |
|   990 | 1152 | `	return rc;` |
|     2 | 1153 | `}` |
|     - | 1154 |  |
|     - | 1155 |  |
|     - | 1156 | `/*` |
|     - | 1157 | ` * base64, in the two shapes RFC 2047 needs. The DECODER is php's` |
|     - | 1158 | ` * php_base64_decode() in its non-strict mode: every byte outside the alphabet` |
|     - | 1159 | ` * is skipped, and a partial final group contributes what its bits allow, so` |
|     - | 1160 | ` * "!!!" decodes to the empty string rather than failing.` |
|     - | 1161 | ` */` |
|     - | 1162 | `static const signed char aIcvB64[128] = {` |
|     - | 1163 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1164 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,` |
|     - | 1165 | `	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,` |
|     - | 1166 | `	52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,` |
|     - | 1167 | `	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,` |
|     - | 1168 | `	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,` |
|     - | 1169 | `	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,` |
|     - | 1170 | `	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1` |
|     - | 1171 | `};` |
|    50 | 1172 | `static void IcvBase64Encode(SyBlob *pOut,const unsigned char *z,int n)` |
|     1 | 1173 | `{` |
|     - | 1174 | `	static const char zAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";` |
|     - | 1175 | `	char zGrp[4];` |
|    51 | 1176 | `	int i = 0;` |
|   193 | 1177 | `	while( i + 2 < n ){` |
|   143 | 1178 | `		sxu32 v = ((sxu32)z[i] << 16) \| ((sxu32)z[i+1] << 8) \| z[i+2];` |
|   143 | 1179 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F]; zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|   143 | 1180 | `		zGrp[2] = zAlpha[(v >> 6) & 0x3F];  zGrp[3] = zAlpha[v & 0x3F];` |
|   143 | 1181 | `		SyBlobAppend(pOut,zGrp,4);` |
|   143 | 1182 | `		i += 3;` |
|     1 | 1183 | `	}` |
|    51 | 1184 | `	if( i < n ){` |
|    45 | 1185 | `		sxu32 v = (sxu32)z[i] << 16;` |
|    45 | 1186 | `		int nRem = n - i;` |
|    45 | 1187 | `		if( nRem > 1 ){` |
|    35 | 1188 | `			v \|= (sxu32)z[i+1] << 8;` |
|    17 | 1189 | `		}` |
|    45 | 1190 | `		zGrp[0] = zAlpha[(v >> 18) & 0x3F];` |
|    45 | 1191 | `		zGrp[1] = zAlpha[(v >> 12) & 0x3F];` |
|    45 | 1192 | `		zGrp[2] = nRem > 1 ? zAlpha[(v >> 6) & 0x3F] : '=';` |
|    45 | 1193 | `		zGrp[3] = '=';` |
|    45 | 1194 | `		SyBlobAppend(pOut,zGrp,4);` |
|    22 | 1195 | `	}` |
|    51 | 1196 | `}` |
|     - | 1197 | `/* How many base64 characters n input bytes become. */` |
|   100 | 1198 | `static int IcvBase64Len(int n)` |
|     1 | 1199 | `{` |
|   101 | 1200 | `	return ((n + 2) / 3) * 4;` |
|     1 | 1201 | `}` |
|    48 | 1202 | `static void IcvBase64Decode(SyBlob *pOut,const char *z,int n)` |
|     1 | 1203 | `{` |
|    49 | 1204 | `	sxu32 v = 0;` |
|    49 | 1205 | `	int i,nBit = 0;` |
|   357 | 1206 | `	for( i = 0 ; i < n ; ++i ){` |
|   309 | 1207 | `		int c = (unsigned char)z[i];` |
|   309 | 1208 | `		int d = (c < 128) ? aIcvB64[c] : -1;` |
|   309 | 1209 | `		if( d < 0 ){` |
|     - | 1210 | `			/* php's non-strict decoder skips EVERYTHING outside the alphabet,` |
|     - | 1211 | ``			 * and that includes the padding: `=` is counted and stepped over,`` |
|     - | 1212 | `			 * not treated as the end, so ":!!!=ZZ" still decodes its "ZZ". */` |
|    57 | 1213 | `			continue;` |
|     - | 1214 | `		}` |
|   253 | 1215 | `		v = (v << 6) \| (sxu32)d;` |
|   253 | 1216 | `		nBit += 6;` |
|   253 | 1217 | `		if( nBit >= 8 ){` |
|   179 | 1218 | `			unsigned char b = (unsigned char)((v >> (nBit - 8)) & 0xFF);` |
|   179 | 1219 | `			SyBlobAppend(pOut,&b,1);` |
|   179 | 1220 | `			nBit -= 8;` |
|    89 | 1221 | `		}` |
|   127 | 1222 | `	}` |
|    49 | 1223 | `}` |
|     - | 1224 |  |
|     - | 1225 | `/*` |
|     - | 1226 | `` * quoted-printable, php's php_quot_print_decode() with `replace_us_by_ws` on --`` |
|     - | 1227 | `` * which is the `Q` of RFC 2047, where `_` stands for a space. Answers 0 for the`` |
|     - | 1228 | `` * strings php refuses (a `=` followed by one hex digit and a non-hex one, and a`` |
|     - | 1229 | ` * soft break that runs off the end); those are what make its caller fall back` |
|     - | 1230 | ` * to the raw encoded word. Stops at a NUL, as php's does.` |
|     - | 1231 | ` */` |
|    20 | 1232 | `static int IcvQPrintDecode(SyBlob *pOut,const char *z,int n,int bUnderscore)` |
|     2 | 1233 | `{` |
|    22 | 1234 | `	int i = 0;` |
|    56 | 1235 | `	while( i < n && z[i] != 0 ){` |
|    44 | 1236 | `		int c = (unsigned char)z[i];` |
|    44 | 1237 | `		if( c != '=' ){` |
|    23 | 1238 | `			unsigned char b = (unsigned char)((bUnderscore && c == '_') ? ' ' : c);` |
|    23 | 1239 | `			SyBlobAppend(pOut,&b,1);` |
|    23 | 1240 | `			i++;` |
|    23 | 1241 | `			continue;` |
|     - | 1242 | `		}` |
|    22 | 1243 | `		i++;` |
|    22 | 1244 | `		if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1245 | `			break;` |
|     - | 1246 | `		}` |
|    22 | 1247 | `		if( SyisHex(z[i]) ){` |
|    13 | 1248 | `			int hi = SyHexToint(z[i]);` |
|    13 | 1249 | `			if( i + 1 >= n \|\| !SyisHex(z[i+1]) ){` |
|   ! 0 | 1250 | `				return 0;` |
|     - | 1251 | `			}` |
|     - | 1252 | `			{` |
|    13 | 1253 | `				unsigned char b = (unsigned char)((hi << 4) \| SyHexToint(z[i+1]));` |
|    13 | 1254 | `				SyBlobAppend(pOut,&b,1);` |
|     - | 1255 | `			}` |
|    13 | 1256 | `			i += 2;` |
|    13 | 1257 | `			continue;` |
|     - | 1258 | `		}` |
|     - | 1259 | `		/* A soft line break: any run of spaces and tabs, then the newline. */` |
|     9 | 1260 | `		while( z[i] == ' ' \|\| z[i] == '\t' ){` |
|   ! 0 | 1261 | `			i++;` |
|   ! 0 | 1262 | `			if( i >= n \|\| z[i] == 0 ){` |
|   ! 0 | 1263 | `				return 0;` |
|     - | 1264 | `			}` |
|   ! 0 | 1265 | `		}` |
|     9 | 1266 | `		if( z[i] != '\r' && z[i] != '\n' ){` |
|     9 | 1267 | `			return 0;` |
|     - | 1268 | `		}` |
|   ! 0 | 1269 | `		if( z[i] == '\r' && i + 1 < n && z[i+1] == '\n' ){` |
|   ! 0 | 1270 | `			i++;` |
|   ! 0 | 1271 | `		}` |
|   ! 0 | 1272 | `		i++;` |
|   ! 0 | 1273 | `	}` |
|    13 | 1274 | `	return 1;` |
|    12 | 1275 | `}` |
|     - | 1276 |  |
|     - | 1277 | ``/* php's qp_table: 1 = the byte may be written as itself, 3 = it needs `=XX`. */`` |
|  4064 | 1278 | `static int IcvQPrintCost(int c)` |
|     1 | 1279 | `{` |
|  4065 | 1280 | `	if( c < 0x21 \|\| c > 0x7E \|\| c == '=' \|\| c == '?' \|\| c == '_' ){` |
|  1231 | 1281 | `		return 3;` |
|     - | 1282 | `	}` |
|  2835 | 1283 | `	return 1;` |
|  2033 | 1284 | `}` |
|     - | 1285 |  |
|     - | 1286 | `#define ICV_SCHEME_B64 0` |
|     - | 1287 | `#define ICV_SCHEME_QP  1` |
|     - | 1288 |  |
|     - | 1289 | `/*` |
|     - | 1290 | ` * php's _php_iconv_mime_encode(). The line budget is a single running counter` |
|     - | 1291 | ` * that every piece of the header subtracts from, and the two schemes spend it` |
|     - | 1292 | ` * differently: base64 works out how many INPUT bytes fit a line by arithmetic` |
|     - | 1293 | `` * (`(char_cnt - 2) / 4 * 3`, minus a four-byte reserve), while quoted-printable`` |
|     - | 1294 | ` * converts a candidate run, PRICES it through the table above, and shrinks the` |
|     - | 1295 | ` * run until the price fits -- which is why a value made of three-byte` |
|     - | 1296 | ` * characters wraps where it does. The counter is charged the field name's RAW` |
|     - | 1297 | ` * byte length even when the name is dropped for not being ASCII.` |
|     - | 1298 | ` */` |
|    64 | 1299 | `static int IcvMimeEncode(ph7_context *pCtx,SyBlob *pOut,` |
|     - | 1300 | `	const char *zName,int nName,const char *zVal,int nVal,` |
|     - | 1301 | `	sxi64 iMaxLine,const char *zLf,int nLf,int iScheme,` |
|     - | 1302 | `	const icv_cs *pTo,const char *zToName,int nToName,int iFrom)` |
|     1 | 1303 | `{` |
|     - | 1304 | `	SyBlob sChunk;` |
|     - | 1305 | `	sxi64 iBudget;` |
|    65 | 1306 | `	int i = 0,rc = ICV_OK;` |
|    65 | 1307 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nToName + 12 >= iMaxLine ){` |
|   ! 0 | 1308 | `		return ICV_TOO_BIG;` |
|     - | 1309 | `	}` |
|    65 | 1310 | `	iBudget = iMaxLine;` |
|    65 | 1311 | `	SyBlobInit(&sChunk,&pCtx->pVm->sAllocator);` |
|     - | 1312 | `	/* The field NAME goes out in ASCII, and php ignores whether that worked --` |
|     - | 1313 | `	 * so a name carrying a byte ASCII cannot hold contributes NOTHING (the` |
|     - | 1314 | `	 * append is all-or-nothing) while still costing its raw length. */` |
|     - | 1315 | `	{` |
|     - | 1316 | `		icv_cs sAscii;` |
|    65 | 1317 | `		sAscii.iEnc = ICV_ASCII;` |
|    65 | 1318 | `		sAscii.bTranslit = 0;` |
|    65 | 1319 | `		sAscii.bIgnore = 0;` |
|    65 | 1320 | `		sAscii.nName = 0;` |
|    65 | 1321 | `		(void)IcvAppendConv(pCtx,pOut,zName,nName,iFrom,&sAscii);` |
|     - | 1322 | `	}` |
|    65 | 1323 | `	iBudget -= nName;` |
|    65 | 1324 | `	SyBlobAppend(pOut,": ",2);` |
|    65 | 1325 | `	iBudget -= 2;` |
|    32 | 1326 | `	do{` |
|     - | 1327 | ``		/* `=?`, the charset, `?`, the scheme letter, `?` and the closing `?=`;`` |
|     - | 1328 | `		 * base64 needs one more character than quoted-printable can get away` |
|     - | 1329 | `		 * with, which is what makes the two minimums differ. */` |
|   103 | 1330 | `		sxi64 iMinWord = 7 + nToName + (iScheme == ICV_SCHEME_B64 ? 4 : 3);` |
|   103 | 1331 | `		if( iBudget < iMinWord + nLf + 1 ){` |
|    41 | 1332 | `			SyBlobAppend(pOut,zLf,(sxu32)nLf);` |
|    41 | 1333 | `			SyBlobAppend(pOut," ",1);` |
|    41 | 1334 | `			iBudget = iMaxLine - 1;` |
|    20 | 1335 | `		}` |
|   103 | 1336 | `		SyBlobAppend(pOut,"=?",2);` |
|   103 | 1337 | `		SyBlobAppend(pOut,zToName,(sxu32)nToName);` |
|   103 | 1338 | `		SyBlobAppend(pOut,"?",1);` |
|   103 | 1339 | `		SyBlobAppend(pOut,iScheme == ICV_SCHEME_B64 ? "B" : "Q",1);` |
|   103 | 1340 | `		SyBlobAppend(pOut,"?",1);` |
|   103 | 1341 | `		iBudget -= 2 + nToName + 3;` |
|   103 | 1342 | `		if( iScheme == ICV_SCHEME_B64 ){` |
|    55 | 1343 | `			sxi64 iRoom = (iBudget - 2) / 4 * 3 - 4;` |
|     - | 1344 | `			int nTook;` |
|    55 | 1345 | `			if( iRoom <= 0 ){` |
|   ! 0 | 1346 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1347 | `				break;` |
|     - | 1348 | `			}` |
|    55 | 1349 | `			SyBlobReset(&sChunk);` |
|    55 | 1350 | `			rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|    55 | 1351 | `			if( rc != ICV_OK ){` |
|     5 | 1352 | `				break;` |
|     - | 1353 | `			}` |
|    51 | 1354 | `			if( nTook == 0 && i < nVal ){` |
|     - | 1355 | `				/* The line has room for a word but not for one CHARACTER of the` |
|     - | 1356 | `				 * value: php's iconv comes back E2BIG having consumed nothing,` |
|     - | 1357 | `				 * and refuses rather than emitting an empty word forever. */` |
|   ! 0 | 1358 | `				rc = ICV_TOO_BIG;` |
|   ! 0 | 1359 | `				break;` |
|     - | 1360 | `			}` |
|    51 | 1361 | `			if( IcvBase64Len((int)SyBlobLength(&sChunk)) > iBudget ){` |
|   ! 0 | 1362 | `				rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1363 | `				break;` |
|     - | 1364 | `			}` |
|    76 | 1365 | `			IcvBase64Encode(pOut,(const unsigned char *)SyBlobData(&sChunk),` |
|    50 | 1366 | `				(int)SyBlobLength(&sChunk));` |
|    51 | 1367 | `			iBudget -= IcvBase64Len((int)SyBlobLength(&sChunk));` |
|    26 | 1368 | `		}else{` |
|    49 | 1369 | `			sxi64 iRoom = iBudget - 2;` |
|    49 | 1370 | `			int nTook = 0,k,iCost = 0;` |
|   144 | 1371 | `			for(;;){` |
|   169 | 1372 | `				int iStart = i;` |
|   169 | 1373 | `				if( iRoom <= 0 ){` |
|   ! 0 | 1374 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1375 | `					break;` |
|     - | 1376 | `				}` |
|   169 | 1377 | `				SyBlobReset(&sChunk);` |
|   169 | 1378 | `				rc = IcvConvertBounded(&sChunk,zVal,nVal,&i,iFrom,pTo,iRoom,&nTook);` |
|   169 | 1379 | `				if( rc != ICV_OK ){` |
|   ! 0 | 1380 | `					break;` |
|     - | 1381 | `				}` |
|   169 | 1382 | `				if( nTook == 0 && i < nVal ){` |
|     - | 1383 | `					/* Not even one character fits: php's own E2BIG-with-no-progress` |
|     - | 1384 | `					 * refusal, which is what keeps this from emitting an empty` |
|     - | 1385 | `					 * encoded word for ever. */` |
|   ! 0 | 1386 | `					rc = ICV_UNKNOWN_ERR;` |
|   ! 0 | 1387 | `					break;` |
|     - | 1388 | `				}` |
|   169 | 1389 | `				iCost = 0;` |
|  3775 | 1390 | `				for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|  3607 | 1391 | `					iCost += IcvQPrintCost(((const unsigned char *)SyBlobData(&sChunk))[k]);` |
|  1804 | 1392 | `				}` |
|   169 | 1393 | `				if( iCost <= iBudget - 2 ){` |
|    49 | 1394 | `					break;` |
|     - | 1395 | `				}` |
|   121 | 1396 | `				iRoom -= ((iCost - (iBudget - 2)) + 2) / 3;` |
|   121 | 1397 | `				i = iStart;` |
|     1 | 1398 | `			}` |
|    49 | 1399 | `			if( rc != ICV_OK ){` |
|   ! 0 | 1400 | `				break;` |
|     - | 1401 | `			}` |
|   507 | 1402 | `			for( k = 0 ; k < (int)SyBlobLength(&sChunk) ; ++k ){` |
|   459 | 1403 | `				int c = ((const unsigned char *)SyBlobData(&sChunk))[k];` |
|   459 | 1404 | `				if( IcvQPrintCost(c) == 1 ){` |
|   333 | 1405 | `					char b = (char)c;` |
|   333 | 1406 | `					SyBlobAppend(pOut,&b,1);` |
|   333 | 1407 | `					iBudget--;` |
|   167 | 1408 | `				}else{` |
|     - | 1409 | `					static const char zHex[] = "0123456789ABCDEF";` |
|     - | 1410 | `					char zEsc[3];` |
|   127 | 1411 | `					zEsc[0] = '='; zEsc[1] = zHex[(c >> 4) & 0x0F]; zEsc[2] = zHex[c & 0x0F];` |
|   127 | 1412 | `					SyBlobAppend(pOut,zEsc,3);` |
|   127 | 1413 | `					iBudget -= 3;` |
|     - | 1414 | `				}` |
|   230 | 1415 | `			}` |
|     - | 1416 | `		}` |
|    99 | 1417 | `		SyBlobAppend(pOut,"?=",2);` |
|    99 | 1418 | `		iBudget -= 2;` |
|    99 | 1419 | `	}while( i < nVal );` |
|    65 | 1420 | `	SyBlobRelease(&sChunk);` |
|    65 | 1421 | `	return rc;` |
|    33 | 1422 | `}` |
|     - | 1423 |  |
|     - | 1424 | `/*` |
|     - | 1425 | ` * php's _php_iconv_mime_decode(), a thirteen-state scanner over the header` |
|     - | 1426 | ` * text. What makes it a scanner rather than a matcher is what it does with the` |
|     - | 1427 | ` * things RFC 2047 does not allow: an encoded word that turns out not to be one` |
|     - | 1428 | ` * is re-emitted as the RAW TEXT it was, from the '=' the scan started at, and` |
|     - | 1429 | ` * the whitespace BETWEEN two encoded words is dropped while whitespace between` |
|     - | 1430 | `` * a word and plain text is kept -- which is why `=?..?= =?..?=` joins and`` |
|     - | 1431 | `` * `=?..?= x` does not.`` |
|     - | 1432 | ` *` |
|     - | 1433 | ` * $mode carries two bits. STRICT (1) refuses the non-RFC forms php otherwise` |
|     - | 1434 | ` * accepts -- an encoded word not followed by whitespace, above all -- and` |
|     - | 1435 | ` * CONTINUE_ON_ERROR (2) turns every refusal into "emit the raw word and carry` |
|     - | 1436 | ` * on". *pNext is left at the byte the scan stopped on, which is how` |
|     - | 1437 | ` * iconv_mime_decode_headers() walks a whole header block one field at a time.` |
|     - | 1438 | ` */` |
|   232 | 1439 | `static int IcvMimeDecode(ph7_context *pCtx,SyBlob *pOut,const char *zIn,int nIn,` |
|     - | 1440 | `	const icv_cs *pTo,int *pNext,int *pbTouched,int iMode)` |
|     2 | 1441 | `{` |
|     - | 1442 | `	SyBlob sWord;` |
|   234 | 1443 | `	const char *z = zIn;` |
|   234 | 1444 | `	int i = 0,rc = ICV_OK;` |
|   234 | 1445 | `	int iState = 0,iScheme = ICV_SCHEME_B64,nLeft,bTouched = 0;` |
|   234 | 1446 | `	int iWord = -1,iSpaces = -1,iCsName = -1,nCsName = 0,iText = -1,nText = 0;` |
|     - | 1447 | `	icv_cs sWordCs;` |
|   234 | 1448 | `	int bStrict = (iMode & 1) != 0,bGoOn = (iMode & 2) != 0;` |
|   234 | 1449 | `	SyBlobInit(&sWord,&pCtx->pVm->sAllocator);` |
|   234 | 1450 | `	sWordCs.iEnc = -1;` |
|   234 | 1451 | `	sWordCs.bTranslit = 0;` |
|   234 | 1452 | `	sWordCs.bIgnore = 0;` |
|   234 | 1453 | `	sWordCs.nName = 0;` |
|     - | 1454 | `	/*` |
|     - | 1455 | `	 * Emit z[iFrom..iTo) as literal text. php converts it from US-ASCII into the` |
|     - | 1456 | `	 * output charset, so a byte over 0x7F cannot go -- and what happens then is` |
|     - | 1457 | `	 * NOT one rule but three, because php checks the conversion's answer at some` |
|     - | 1458 | `	 * of these sites and not others. HARD fails whatever $mode says; CHECKED is` |
|     - | 1459 | `	 * the one CONTINUE_ON_ERROR was written for; SILENT drops the byte and says` |
|     - | 1460 | `	 * nothing, which is why a stray high byte AFTER a complete encoded word` |
|     - | 1461 | `	 * disappears while the same byte inside one is a failure.` |
|     - | 1462 | `	 */` |
|     - | 1463 | `/* php's _php_iconv_appendl() is ALL OR NOTHING: it converts into a buffer of` |
|     - | 1464 | ` * its own and only hands that to the output when the whole run went, so a run` |
|     - | 1465 | ` * that fails part-way appends nothing at all. And with a NULL source it appends` |
|     - | 1466 | ``  * nothing and answers success, which is what an already-consumed `encoded_word` `` |
|     - | 1467 | ` * becomes -- load-bearing, because state 9 stays in state 9 and re-runs for` |
|     - | 1468 | ` * every character after a word it could not convert. */` |
|     - | 1469 | `#define ICV_RAW_AT(iFrom,iTo,eMode) do { \` |
|     - | 1470 | `		int rcRaw = (iFrom) < 0 ? ICV_OK \` |
|     - | 1471 | `			: IcvAppendConv(pCtx,pOut,&z[iFrom],(iTo) - (iFrom),ICV_ASCII,pTo); \` |
|     - | 1472 | `		bTouched = 1; \` |
|     - | 1473 | `		if( (eMode) != 2 ){ \` |
|     - | 1474 | ``			/* php's `err` is a RUNNING variable at the checked sites: a later \`` |
|     - | 1475 | `			 * successful append assigns SUCCESS over an earlier failure, so a \` |
|     - | 1476 | `			 * refusal recorded mid-scan can still be forgotten. The silent \` |
|     - | 1477 | `			 * sites never touch it at all. */ \` |
|     - | 1478 | `			rc = rcRaw; \` |
|     - | 1479 | `			if( rcRaw != ICV_OK && ((eMode) == 1 \|\| !bGoOn) ){ \` |
|     - | 1480 | `				goto done; \` |
|     - | 1481 | `			} \` |
|     - | 1482 | `			if( rcRaw != ICV_OK ){ rc = ICV_OK; } \` |
|     - | 1483 | `		} \` |
|     - | 1484 | `	} while(0)` |
|     - | 1485 | `#define ICV_RAW(iFrom,iTo)        ICV_RAW_AT(iFrom,iTo,0)` |
|     - | 1486 | `#define ICV_RAW_HARD(iFrom,iTo)   ICV_RAW_AT(iFrom,iTo,1)` |
|     - | 1487 | `#define ICV_RAW_SILENT(iFrom,iTo) ICV_RAW_AT(iFrom,iTo,2)` |
|     - | 1488 | `	/*` |
|     - | 1489 | `	 * php walks with a POSITION and a separate COUNT, and the two go out of step` |
|     - | 1490 | ``	 * on purpose: several states rewind the position by one (`--p1`) without`` |
|     - | 1491 | `	 * giving the count back, so the scan re-reads a byte and then stops one byte` |
|     - | 1492 | `	 * SHORT of the end. That is not an accident of style -- it is what makes` |
|     - | 1493 | ``	 * `iconv_mime_decode("UHLDvGZ1bmc=\r\n")` a "Malformed string": the final`` |
|     - | 1494 | `	 * "\n" is never reached, so the scanner ends mid-EOL instead of after one.` |
|     - | 1495 | `	 */` |
|   234 | 1496 | `	nLeft = nIn;` |
|  3188 | 1497 | `	while( nLeft > 0 ){` |
|  2978 | 1498 | `		int c = (unsigned char)z[i];` |
|  2978 | 1499 | `		int bEos = 0;` |
|  2978 | 1500 | `		switch( iState ){` |
|   393 | 1501 | `		case 0:   /* anything at all */` |
|   788 | 1502 | `			if( c == '\r' ){ iState = 7; break; }` |
|   736 | 1503 | `			if( c == '\n' ){ iState = 8; break; }` |
|   730 | 1504 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|   613 | 1505 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   529 | 1506 | `			ICV_RAW(i,i+1);` |
|   529 | 1507 | `			iWord = -1;` |
|   529 | 1508 | `			if( bStrict ){ iState = 12; }` |
|   529 | 1509 | `			break;` |
|    75 | 1510 | `		case 1:   /* after '=': expecting '?' */` |
|   152 | 1511 | `			if( c != '?' ){` |
|   ! 0 | 1512 | `				if( c == '\r' \|\| c == '\n' ){ i--; }` |
|   ! 0 | 1513 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1514 | `				iWord = -1;` |
|   ! 0 | 1515 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1516 | `				break;` |
|     - | 1517 | `			}` |
|   152 | 1518 | `			iCsName = i + 1;` |
|   152 | 1519 | `			iState = 2;` |
|   152 | 1520 | `			break;` |
|   405 | 1521 | `		case 2:   /* the charset name */` |
|   812 | 1522 | `			if( c == '\r' \|\| c == '\n' ){` |
|   ! 0 | 1523 | `				i--;` |
|   ! 0 | 1524 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1525 | `				iCsName = -1;` |
|   ! 0 | 1526 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1527 | `				break;` |
|     - | 1528 | `			}` |
|   812 | 1529 | `			if( c != '?' && c != '*' ){` |
|   686 | 1530 | `				break;` |
|     - | 1531 | `			}` |
|   128 | 1532 | `			nCsName = i - iCsName;` |
|   128 | 1533 | `			if( nCsName > ICV_CSNMAXLEN + 15 ){` |
|     - | 1534 | `				/* php's own 80-byte scratch buffer for the name. */` |
|   ! 0 | 1535 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1536 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1537 | `				iWord = -1;` |
|   ! 0 | 1538 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1539 | `				break;` |
|     - | 1540 | `			}` |
|   128 | 1541 | `			IcvParseCharset(&z[iCsName],nCsName,&sWordCs);` |
|   128 | 1542 | `			if( sWordCs.iEnc < 0 ){` |
|    23 | 1543 | `				if( !bGoOn ){` |
|    11 | 1544 | `					rc = ICV_WRONG_CHARSET;` |
|    11 | 1545 | `					goto done;` |
|     - | 1546 | `				}` |
|     - | 1547 | `				/* php skips to the end of the word and hands it over raw. */` |
|     - | 1548 | `				{` |
|    13 | 1549 | `					int nQ = 2;` |
|    97 | 1550 | `					while( nQ > 0 && nLeft > 1 ){` |
|    85 | 1551 | `						if( z[++i] == '?' ){ nQ--; }` |
|    85 | 1552 | `						nLeft--;` |
|     1 | 1553 | `					}` |
|    13 | 1554 | `					if( i + 1 < nIn && z[i+1] == '=' ){` |
|    13 | 1555 | `						i++;` |
|    13 | 1556 | `						if( nLeft > 1 ){ nLeft--; }` |
|     6 | 1557 | `					}` |
|     - | 1558 | `				}` |
|    13 | 1559 | `				ICV_RAW_HARD(iWord,i+1);` |
|    13 | 1560 | `				iState = 12;` |
|    13 | 1561 | `				break;` |
|     - | 1562 | `			}` |
|   106 | 1563 | `			iState = (c == '*') ? 10 : 3;` |
|   106 | 1564 | `			break;` |
|    48 | 1565 | `		case 3:   /* the scheme letter */` |
|    98 | 1566 | `			if( c == 'b' \|\| c == 'B' ){ iScheme = ICV_SCHEME_B64; iState = 4; break; }` |
|    30 | 1567 | `			if( c == 'q' \|\| c == 'Q' ){ iScheme = ICV_SCHEME_QP; iState = 4; break; }` |
|     9 | 1568 | `			if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|     5 | 1569 | `			ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1570 | `			iWord = -1;` |
|     5 | 1571 | `			iState = bStrict ? 12 : 0;` |
|     5 | 1572 | `			break;` |
|    44 | 1573 | `		case 4:   /* expecting '?' */` |
|    90 | 1574 | `			if( c != '?' ){` |
|   ! 0 | 1575 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1576 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1577 | `				iWord = -1;` |
|   ! 0 | 1578 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1579 | `				break;` |
|     - | 1580 | `			}` |
|    90 | 1581 | `			iText = i + 1;` |
|    90 | 1582 | `			iState = 5;` |
|    90 | 1583 | `			break;` |
|   271 | 1584 | `		case 5:   /* the encoded text */` |
|   544 | 1585 | `			if( c == '?' ){` |
|    82 | 1586 | `				nText = i - iText;` |
|    82 | 1587 | `				iState = 6;` |
|    40 | 1588 | `			}` |
|   544 | 1589 | `			break;` |
|    36 | 1590 | `		case 6:   /* expecting the closing '=' */` |
|    74 | 1591 | `			if( c != '=' ){` |
|   ! 0 | 1592 | `				if( !bGoOn ){ rc = ICV_MALFORMED; goto done; }` |
|   ! 0 | 1593 | `				ICV_RAW_HARD(iWord,i+1);` |
|   ! 0 | 1594 | `				iWord = -1;` |
|   ! 0 | 1595 | `				iState = bStrict ? 12 : 0;` |
|   ! 0 | 1596 | `				break;` |
|     - | 1597 | `			}` |
|    74 | 1598 | `			iState = 9;` |
|    96 | 1599 | `			if( nLeft == 1 ){` |
|    46 | 1600 | `				bEos = 1;` |
|    24 | 1601 | `			}else{` |
|    29 | 1602 | `				break;` |
|     - | 1603 | `			}` |
|     - | 1604 | `			/* fall through -- the word ended with the string */` |
|     - | 1605 | `			/* FALLTHROUGH */` |
|     - | 1606 | `		case 9:   /* what follows a complete word */` |
|    74 | 1607 | `			if( !bEos && c != '\r' && c != '\n' && c != ' ' && c != '\t' && bStrict ){` |
|     5 | 1608 | `				ICV_RAW_HARD(iWord,i+1);` |
|     5 | 1609 | `				iState = 12;` |
|     5 | 1610 | `				break;` |
|     - | 1611 | `			}` |
|    70 | 1612 | `			SyBlobReset(&sWord);` |
|    70 | 1613 | `			if( iScheme == ICV_SCHEME_B64 ){` |
|    49 | 1614 | `				IcvBase64Decode(&sWord,&z[iText],nText);` |
|    46 | 1615 | `			}else if( !IcvQPrintDecode(&sWord,&z[iText],nText,1) ){` |
|     9 | 1616 | `				if( !bGoOn ){ rc = ICV_UNKNOWN_ERR; goto done; }` |
|     3 | 1617 | `				ICV_RAW_HARD(iWord,i+1);` |
|     3 | 1618 | `				iWord = -1;` |
|     3 | 1619 | `				iState = bStrict ? 12 : 0;` |
|     3 | 1620 | `				break;` |
|     - | 1621 | `			}` |
|     - | 1622 | `			{` |
|     - | 1623 | `				int rcW;` |
|    61 | 1624 | `				bTouched = 1;` |
|    91 | 1625 | `				rcW = IcvAppendConv(pCtx,pOut,(const char *)SyBlobData(&sWord),` |
|    60 | 1626 | `					(int)SyBlobLength(&sWord),sWordCs.iEnc,pTo);` |
|    61 | 1627 | `				if( rcW != ICV_OK ){` |
|     3 | 1628 | `					if( !bGoOn ){ rc = rcW; goto done; }` |
|     - | 1629 | `					/* php hands the raw word over and carries on. If THAT will` |
|     - | 1630 | ``					 * not convert either it keeps the failure in `err` and stays`` |
|     - | 1631 | `					 * in this state, so the next character re-runs the whole` |
|     - | 1632 | `					 * branch; when it DOES convert the state moves on below. */` |
|   ! 0 | 1633 | `				rc = iWord < 0 ? ICV_OK` |
|   ! 0 | 1634 | `						: IcvAppendConv(pCtx,pOut,&z[iWord],i - iWord,ICV_ASCII,pTo);` |
|   ! 0 | 1635 | `					bTouched = 1;` |
|   ! 0 | 1636 | `					iWord = -1;` |
|   ! 0 | 1637 | `					if( rc != ICV_OK ){` |
|   ! 0 | 1638 | `						break;` |
|     - | 1639 | `					}` |
|   ! 0 | 1640 | `				}` |
|     - | 1641 | `			}` |
|    59 | 1642 | `			if( bEos ){` |
|    35 | 1643 | `				iState = 0;` |
|    35 | 1644 | `				break;` |
|     - | 1645 | `			}` |
|    25 | 1646 | `			if( c == '\r' ){ iState = 7; break; }` |
|    15 | 1647 | `			if( c == '\n' ){ iState = 8; break; }` |
|    13 | 1648 | `			if( c == '=' ){ iWord = i; iState = 1; break; }` |
|    13 | 1649 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|     7 | 1650 | `			bTouched = 1;` |
|     7 | 1651 | `			ICV_RAW_SILENT(i,i+1);` |
|     7 | 1652 | `			iState = 12;` |
|     7 | 1653 | `			break;` |
|    36 | 1654 | `		case 7:   /* after CR: expecting LF */` |
|    73 | 1655 | `			if( c == '\n' ){` |
|    71 | 1656 | `				iState = 8;` |
|    36 | 1657 | `			}else{` |
|     3 | 1658 | `				SyBlobAppend(pOut,"\r",1);` |
|     3 | 1659 | `				bTouched = 1;` |
|     3 | 1660 | `				ICV_RAW_SILENT(i,i+1);` |
|     3 | 1661 | `				iState = 0;` |
|     - | 1662 | `			}` |
|    73 | 1663 | `			break;` |
|    22 | 1664 | `		case 8:   /* after a newline: is the next line a continuation? */` |
|    45 | 1665 | `			if( c != ' ' && c != '\t' ){` |
|     - | 1666 | `				/* The field ended here. Rewinding the position and setting the` |
|     - | 1667 | `				 * count to one leaves *pNext pointing AT this character, so the` |
|     - | 1668 | `				 * caller's next field starts on it. */` |
|    39 | 1669 | `				nLeft = 1;` |
|    39 | 1670 | `				i--;` |
|    39 | 1671 | `				break;` |
|     - | 1672 | `			}` |
|     7 | 1673 | `			if( iWord < 0 ){` |
|     7 | 1674 | `				SyBlobAppend(pOut," ",1);` |
|     7 | 1675 | `				bTouched = 1;` |
|     3 | 1676 | `			}` |
|     7 | 1677 | `			iSpaces = -1;` |
|     7 | 1678 | `			iState = 11;` |
|     7 | 1679 | `			break;` |
|     3 | 1680 | `		case 10:  /* a language tag after the charset: dismissed */` |
|     7 | 1681 | `			if( c == '?' ){ iState = 3; }` |
|     7 | 1682 | `			break;` |
|    67 | 1683 | `		case 11:  /* a run of whitespace */` |
|   135 | 1684 | `			if( c == '\r' ){ iState = 7; break; }` |
|   135 | 1685 | `			if( c == '\n' ){ iState = 8; break; }` |
|   135 | 1686 | `			if( c == '=' ){` |
|     - | 1687 | `				/* Whitespace BETWEEN two encoded words disappears; whitespace` |
|     - | 1688 | `				 * that followed plain text does not. */` |
|    43 | 1689 | `				if( iSpaces >= 0 && iWord < 0 ){` |
|    39 | 1690 | `					bTouched = 1;` |
|    39 | 1691 | `					ICV_RAW_SILENT(iSpaces,i);` |
|    39 | 1692 | `					iSpaces = -1;` |
|    19 | 1693 | `				}` |
|    43 | 1694 | `				iWord = i;` |
|    43 | 1695 | `				iState = 1;` |
|    43 | 1696 | `				break;` |
|     - | 1697 | `			}` |
|    93 | 1698 | `			if( c == ' ' \|\| c == '\t' ){` |
|     4 | 1699 | `				break;` |
|     - | 1700 | `			}` |
|    87 | 1701 | `			if( iSpaces >= 0 ){` |
|    81 | 1702 | `				ICV_RAW_SILENT(iSpaces,i);` |
|    40 | 1703 | `			}` |
|    87 | 1704 | `			iSpaces = -1;` |
|    87 | 1705 | `			bTouched = 1;` |
|    87 | 1706 | `			ICV_RAW_SILENT(i,i+1);` |
|    87 | 1707 | `			iWord = -1;` |
|    87 | 1708 | `			iState = bStrict ? 12 : 0;` |
|    87 | 1709 | `			break;` |
|    74 | 1710 | `		case 12:  /* plain text */` |
|   149 | 1711 | `			if( c == '\r' ){ iState = 7; break; }` |
|   139 | 1712 | `			if( c == '\n' ){ iState = 8; break; }` |
|   139 | 1713 | `			if( c == ' ' \|\| c == '\t' ){ iSpaces = i; iState = 11; break; }` |
|   105 | 1714 | `			if( c == '=' && !bStrict ){ iWord = i; iState = 1; break; }` |
|   103 | 1715 | `			bTouched = 1;` |
|   103 | 1716 | `			ICV_RAW_SILENT(i,i+1);` |
|   102 | 1717 | `			break;` |
|     - | 1718 | `		}` |
|  2956 | 1719 | `		nLeft--;` |
|  2956 | 1720 | `		i++;` |
|     2 | 1721 | `	}` |
|   212 | 1722 | `	switch( iState ){` |
|    76 | 1723 | `	case 0: case 8: case 11: case 12:` |
|   154 | 1724 | `		break;` |
|    29 | 1725 | `	default:` |
|    59 | 1726 | `		if( bGoOn ){` |
|    31 | 1727 | `			if( iState == 1 ){` |
|     7 | 1728 | `				SyBlobAppend(pOut,"=",1);` |
|     3 | 1729 | `			}` |
|     - | 1730 | `			/* CONTINUE_ON_ERROR clears whatever the scan recorded on its way` |
|     - | 1731 | `			 * through, which is why a header full of bytes the output charset` |
|     - | 1732 | `			 * cannot hold comes back as the empty string rather than FALSE. */` |
|    31 | 1733 | `			rc = ICV_OK;` |
|    16 | 1734 | `		}else{` |
|    29 | 1735 | `			rc = ICV_MALFORMED;` |
|     - | 1736 | `		}` |
|    58 | 1737 | `		break;` |
|   105 | 1738 | `	}` |
|   116 | 1739 | `done:` |
|   234 | 1740 | `	SyBlobRelease(&sWord);` |
|   234 | 1741 | `	if( pNext ){` |
|    77 | 1742 | `		*pNext = i;` |
|    38 | 1743 | `	}` |
|   234 | 1744 | `	if( pbTouched ){` |
|    77 | 1745 | `		*pbTouched = bTouched;` |
|    38 | 1746 | `	}` |
|   234 | 1747 | `	return rc;` |
|     2 | 1748 | `}` |
|     - | 1749 | `#undef ICV_RAW_AT` |
|     - | 1750 | `#undef ICV_RAW` |
|     - | 1751 | `#undef ICV_RAW_HARD` |
|     - | 1752 | `#undef ICV_RAW_SILENT` |
|     - | 1753 |  |
|     - | 1754 | `/*` |
|     - | 1755 | ` * Read one option out of an OPTIONS array. php reads three of the five as` |
|     - | 1756 | `` * STRINGS ONLY -- a non-string `scheme`, `input-charset` or `output-charset` is`` |
|     - | 1757 | ` * not coerced, it is ignored -- so those need no conversion at all and the` |
|     - | 1758 | ` * array is never touched. The other two ARE coerced, and a coercion has to go` |
|     - | 1759 | `` * through a scratch copy: `ph7_value_to_xxx()` converts the value it is handed,`` |
|     - | 1760 | ` * which would rewrite the caller's own array (the defect §2 records four` |
|     - | 1761 | ` * builtins sharing).` |
|     - | 1762 | ` */` |
|   174 | 1763 | `static const char * IcvOptionRawStr(ph7_value *pOpt,const char *zKey,int *pnOut)` |
|     1 | 1764 | `{` |
|   175 | 1765 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|   175 | 1766 | `	if( pV == 0 \|\| !ph7_value_is_string(pV) ){` |
|   121 | 1767 | `		return 0;` |
|     - | 1768 | `	}` |
|    55 | 1769 | `	return ph7_value_to_string(pV,pnOut);` |
|    88 | 1770 | `}` |
|    56 | 1771 | `static int IcvOptionInt(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,sxi64 *pOut)` |
|     1 | 1772 | `{` |
|    57 | 1773 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|    57 | 1774 | `	if( pV == 0 ){` |
|    41 | 1775 | `		return 0;` |
|     - | 1776 | `	}` |
|     8 | 1777 | `	SXUNUSED(pCtx);` |
|    17 | 1778 | `	*pOut = PH7_ValuePeekInt64(pV);` |
|    17 | 1779 | `	return 1;` |
|    29 | 1780 | `}` |
|     - | 1781 | `/* Copy one option into pOut as a string, coercing whatever it is. */` |
|    56 | 1782 | `static int IcvOptionCopyStr(ph7_context *pCtx,ph7_value *pOpt,const char *zKey,SyBlob *pOut)` |
|     1 | 1783 | `{` |
|    57 | 1784 | `	ph7_value *pV = ph7_array_fetch(pOpt,zKey,(int)SyStrlen(zKey));` |
|     - | 1785 | `	ph7_value sScratch,*pCopy;` |
|     - | 1786 | `	const char *z;` |
|    57 | 1787 | `	int n = 0;` |
|    57 | 1788 | `	if( pV == 0 ){` |
|    53 | 1789 | `		return 0;` |
|     - | 1790 | `	}` |
|     5 | 1791 | `	PH7_MemObjInit(pCtx->pVm,&sScratch);` |
|     5 | 1792 | `	pCopy = PH7_ValuePeek(pV,&sScratch);` |
|     5 | 1793 | `	z = ph7_value_to_string(pCopy,&n);` |
|     5 | 1794 | `	if( n > 0 ){` |
|     3 | 1795 | `		SyBlobAppend(pOut,z,(sxu32)n);` |
|     1 | 1796 | `	}` |
|     5 | 1797 | `	PH7_MemObjRelease(&sScratch);` |
|     5 | 1798 | `	return 1;` |
|    29 | 1799 | `}` |
|     - | 1800 |  |
|     - | 1801 | `/*` |
|     - | 1802 | ` * string\|false iconv_mime_encode(string $field_name, string $field_value,` |
|     - | 1803 | ` *                                array $options = [])` |
|     - | 1804 | ` *` |
|     - | 1805 | `` * The five options php reads, and the exact way it reads them: `scheme` is one`` |
|     - | 1806 | ` * LETTER (its first, case-insensitively, and anything but B/b/Q/q leaves the` |
|     - | 1807 | `` * default alone rather than failing), `input-charset` and `output-charset` are`` |
|     - | 1808 | `` * only honoured when they are non-empty STRINGS, `line-length` goes through an`` |
|     - | 1809 | `` * ordinary int cast, and `line-break-chars` is taken as a string whatever it is.`` |
|     - | 1810 | ` */` |
|    78 | 1811 | `static int PH7_builtin_iconv_mime_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1812 | `{` |
|    79 | 1813 | `	const char *zName,*zVal,*zLf = "\r\n",*zIn = 0,*zOut = 0;` |
|    79 | 1814 | `	int nName,nVal,nLf = 2,nIn = 0,nOut = 0,iScheme = ICV_SCHEME_B64,err;` |
|    79 | 1815 | `	sxi64 iMaxLine = 76;` |
|     - | 1816 | `	icv_cs sFrom,sTo;` |
|     - | 1817 | `	SyBlob sOut,sIniLf,sIniIn,sIniOut,sIniCharset;` |
|     - | 1818 | `	ph7_value *pOpt;` |
|    79 | 1819 | `	if( nArg < 2 ){` |
|   ! 0 | 1820 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 | 1821 | `		return PH7_OK;` |
|     - | 1822 | `	}` |
|    79 | 1823 | `	zName = ph7_value_to_string(apArg[0],&nName);` |
|    79 | 1824 | `	zVal = ph7_value_to_string(apArg[1],&nVal);` |
|    79 | 1825 | `	SyBlobInit(&sIniLf,&pCtx->pVm->sAllocator);` |
|    79 | 1826 | `	SyBlobInit(&sIniIn,&pCtx->pVm->sAllocator);` |
|    79 | 1827 | `	SyBlobInit(&sIniOut,&pCtx->pVm->sAllocator);` |
|    79 | 1828 | `	SyBlobInit(&sIniCharset,&pCtx->pVm->sAllocator);` |
|    79 | 1829 | `	PH7_VmIniGetStr(pCtx->pVm,"default_charset",&sIniCharset);` |
|    79 | 1830 | `	zIn = (const char *)SyBlobData(&sIniCharset);` |
|    79 | 1831 | `	nIn = (int)SyBlobLength(&sIniCharset);` |
|    79 | 1832 | `	zOut = zIn;` |
|    79 | 1833 | `	nOut = nIn;` |
|    79 | 1834 | `	pOpt = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|    79 | 1835 | `	if( pOpt ){` |
|     - | 1836 | `		const char *zS;` |
|    59 | 1837 | `		int nS = 0;` |
|    59 | 1838 | `		if( (zS = IcvOptionRawStr(pOpt,"scheme",&nS)) != 0 && nS > 0 ){` |
|     - | 1839 | `			/* One LETTER, case-insensitively, and anything but B or Q leaves the` |
|     - | 1840 | `			 * default where it was rather than failing. */` |
|    33 | 1841 | `			int c = IcvUpper((unsigned char)zS[0]);` |
|    33 | 1842 | `			if( c == 'B' ){ iScheme = ICV_SCHEME_B64; }` |
|    27 | 1843 | `			else if( c == 'Q' ){ iScheme = ICV_SCHEME_QP; }` |
|    16 | 1844 | `		}` |
|    59 | 1845 | `		if( (zS = IcvOptionRawStr(pOpt,"input-charset",&nS)) != 0 ){` |
|     9 | 1846 | `			if( nS >= ICV_CSNMAXLEN ){` |
|   ! 0 | 1847 | `				err = ICV_TOO_LONG;` |
|   ! 0 | 1848 | `				goto fail;` |
|     - | 1849 | `			}` |
|     9 | 1850 | `			if( nS > 0 ){` |
|     9 | 1851 | `				SyBlobAppend(&sIniIn,zS,(sxu32)nS);` |
|     9 | 1852 | `				zIn = (const char *)SyBlobData(&sIniIn);` |
|     9 | 1853 | `				nIn = nS;` |
|     4 | 1854 | `			}` |
|     4 | 1855 | `		}` |
|    59 | 1856 | `		if( (zS = IcvOptionRawStr(pOpt,"output-charset",&nS)) != 0 ){` |
|    13 | 1857 | `			if( nS >= ICV_CSNMAXLEN ){` |
|     3 | 1858 | `				err = ICV_TOO_LONG;` |
|     3 | 1859 | `				goto fail;` |
|     - | 1860 | `			}` |
|    11 | 1861 | `			if( nS > 0 ){` |
|    11 | 1862 | `				SyBlobAppend(&sIniOut,zS,(sxu32)nS);` |
|    11 | 1863 | `				zOut = (const char *)SyBlobData(&sIniOut);` |
|    11 | 1864 | `				nOut = nS;` |
|     5 | 1865 | `			}` |
|     5 | 1866 | `		}` |
|    57 | 1867 | `		IcvOptionInt(pCtx,pOpt,"line-length",&iMaxLine);` |
|    57 | 1868 | `		if( IcvOptionCopyStr(pCtx,pOpt,"line-break-chars",&sIniLf) ){` |
|     5 | 1869 | `			zLf = (const char *)SyBlobData(&sIniLf);` |
|     5 | 1870 | `			nLf = (int)SyBlobLength(&sIniLf);` |
|     2 | 1871 | `		}` |
|    28 | 1872 | `	}` |
|     - | 1873 | `	/* php measures the LINE BUDGET before it opens a converter, so a header` |
|     - | 1874 | `	 * that cannot fit is refused whatever the charsets say. (php compares the` |
|     - | 1875 | ``	 * budget as a size_t, so a NEGATIVE `line-length` is a huge one there and it`` |
|     - | 1876 | `	 * dies in the allocator instead -- "Possible integer overflow in memory` |
|     - | 1877 | `	 * allocation"; PHL compares signed, so it lands on the same refusal every` |
|     - | 1878 | `	 * other impossible budget gets. §7.4.) */` |
|    77 | 1879 | `	if( (sxi64)nName + 2 >= iMaxLine \|\| (sxi64)nOut + 12 >= iMaxLine ){` |
|     9 | 1880 | `		err = ICV_TOO_BIG;` |
|     9 | 1881 | `		goto fail;` |
|     - | 1882 | `	}` |
|    69 | 1883 | `	IcvParseCharset(zIn,nIn,&sFrom);` |
|    69 | 1884 | `	IcvParseCharset(zOut,nOut,&sTo);` |
|    69 | 1885 | `	if( sFrom.iEnc < 0 \|\| sTo.iEnc < 0 ){` |
|     5 | 1886 | `		err = ICV_WRONG_CHARSET;` |
|     5 | 1887 | `		goto fail;` |
|     - | 1888 | `	}` |
|    65 | 1889 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    97 | 1890 | `	err = IcvMimeEncode(pCtx,&sOut,zName,nName,zVal,nVal,iMaxLine,zLf,nLf,` |
|    32 | 1891 | `		iScheme,&sTo,zOut,nOut,sFrom.iEnc);` |
|    65 | 1892 | `	if( err == ICV_OK ){` |
|    61 | 1893 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    30 | 1894 | `	}` |
|    65 | 1895 | `	SyBlobRelease(&sOut);` |
|    65 | 1896 | `	if( err != ICV_OK ){` |
|     5 | 1897 | `		goto fail;` |
|     - | 1898 | `	}` |
|    61 | 1899 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    61 | 1900 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    61 | 1901 | `	return PH7_OK;` |
|     9 | 1902 | `fail:` |
|    19 | 1903 | `	if( err == ICV_TOO_LONG ){` |
|     3 | 1904 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     - | 1905 | `			"Encoding parameter exceeds the maximum allowed length of %d characters",` |
|     - | 1906 | `			ICV_CSNMAXLEN);` |
|     2 | 1907 | `	}else{` |
|    17 | 1908 | `		IcvShowError(pCtx,err,zOut,nOut,zIn,nIn);` |
|     - | 1909 | `	}` |
|    19 | 1910 | `	ph7_result_bool(pCtx,0);` |
|    19 | 1911 | `	SyBlobRelease(&sIniLf); SyBlobRelease(&sIniIn);` |
|    19 | 1912 | `	SyBlobRelease(&sIniOut); SyBlobRelease(&sIniCharset);` |
|    19 | 1913 | `	return PH7_OK;` |
|    40 | 1914 | `}` |
|     - | 1915 |  |
|     - | 1916 | ``/* Resolve the `?string $encoding = null` the two decoders share: php's`` |
|     - | 1917 | ` * internal encoding, with only the length cap raised here. */` |
|   206 | 1918 | `static int IcvMimeDecodeArgs(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - | 1919 | `	icv_cs *pCs,sxi64 *pMode)` |
|     2 | 1920 | `{` |
|   208 | 1921 | `	*pMode = 0;` |
|   206 | 1922 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1])` |
|   206 | 1923 | `	 && PH7_IntArgResolve(pCtx,apArg[1],"iconv_mime_decode",2,"$mode","int",pMode) != PH7_OK ){` |
|   ! 0 | 1924 | `		return 0;` |
|     - | 1925 | `	}` |
|   208 | 1926 | `	if( !IcvStrEncArg(pCtx,nArg > 2 ? apArg[2] : 0,pCs) ){` |
|     3 | 1927 | `		return 0;` |
|     - | 1928 | `	}` |
|   206 | 1929 | `	if( pCs->iEnc < 0 ){` |
|     - | 1930 | ``		/* php names the source `"???"` here: the decoder learns the real one`` |
|     - | 1931 | `		 * word by word, so there is nothing to report but the target. */` |
|     5 | 1932 | `		IcvShowError(pCtx,ICV_WRONG_CHARSET,pCs->zName,pCs->nName,"???",3);` |
|     5 | 1933 | `		return 0;` |
|     - | 1934 | `	}` |
|   202 | 1935 | `	return 1;` |
|   105 | 1936 | `}` |
|     - | 1937 |  |
|     - | 1938 | `/* string\|false iconv_mime_decode(string $string, int $mode = 0,` |
|     - | 1939 | ` *                                ?string $encoding = null) */` |
|   160 | 1940 | `static int PH7_builtin_iconv_mime_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     2 | 1941 | `{` |
|     - | 1942 | `	const char *zIn;` |
|     - | 1943 | `	int nIn,err;` |
|     - | 1944 | `	sxi64 iMode;` |
|     - | 1945 | `	icv_cs sCs;` |
|     - | 1946 | `	SyBlob sOut;` |
|   162 | 1947 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     5 | 1948 | `		ph7_result_bool(pCtx,0);` |
|     5 | 1949 | `		return PH7_OK;` |
|     - | 1950 | `	}` |
|   158 | 1951 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|   158 | 1952 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|   158 | 1953 | `	err = IcvMimeDecode(pCtx,&sOut,zIn,nIn,&sCs,0,0,(int)iMode);` |
|   158 | 1954 | `	if( err == ICV_OK ){` |
|   110 | 1955 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    56 | 1956 | `	}else{` |
|    50 | 1957 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|    50 | 1958 | `		ph7_result_bool(pCtx,0);` |
|     - | 1959 | `	}` |
|   158 | 1960 | `	SyBlobRelease(&sOut);` |
|   158 | 1961 | `	return PH7_OK;` |
|    82 | 1962 | `}` |
|     - | 1963 |  |
|     - | 1964 | `/*` |
|     - | 1965 | ` * array\|false iconv_mime_decode_headers(string $headers, int $mode = 0,` |
|     - | 1966 | ` *                                       ?string $encoding = null)` |
|     - | 1967 | ` *` |
|     - | 1968 | ` * Decode one field at a time, splitting each at its FIRST ':' -- a line with` |
|     - | 1969 | ` * none is dropped entirely, which is what ends the walk at the blank line that` |
|     - | 1970 | ` * separates headers from a body. A name seen twice becomes a LIST, and the` |
|     - | 1971 | ` * first value is kept as a plain string until the second arrives.` |
|     - | 1972 | ` */` |
|    46 | 1973 | `static int PH7_builtin_iconv_mime_decode_headers(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 | 1974 | `{` |
|     - | 1975 | `	const char *zIn;` |
|    47 | 1976 | `	int nIn,i = 0,err = ICV_OK;` |
|     - | 1977 | `	sxi64 iMode;` |
|     - | 1978 | `	icv_cs sCs;` |
|     - | 1979 | `	ph7_value *pArray,*pVal;` |
|    47 | 1980 | `	if( nArg < 1 \|\| !IcvMimeDecodeArgs(pCtx,nArg,apArg,&sCs,&iMode) ){` |
|     3 | 1981 | `		ph7_result_bool(pCtx,0);` |
|     3 | 1982 | `		return PH7_OK;` |
|     - | 1983 | `	}` |
|    45 | 1984 | `	zIn = ph7_value_to_string(apArg[0],&nIn);` |
|    45 | 1985 | `	pArray = ph7_context_new_array(pCtx);` |
|    45 | 1986 | `	pVal = ph7_context_new_scalar(pCtx);` |
|    45 | 1987 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 | 1988 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 1989 | `	}` |
|   117 | 1990 | `	while( i < nIn ){` |
|     - | 1991 | `		SyBlob sLine;` |
|     - | 1992 | `		const char *zLine;` |
|    77 | 1993 | `		int nLine,iColon,iNext = 0,k,bTouched = 0;` |
|    77 | 1994 | `		SyBlobInit(&sLine,&pCtx->pVm->sAllocator);` |
|    77 | 1995 | `		err = IcvMimeDecode(pCtx,&sLine,&zIn[i],nIn - i,&sCs,&iNext,&bTouched,(int)iMode);` |
|    77 | 1996 | `		if( err != ICV_OK ){` |
|     3 | 1997 | `			SyBlobRelease(&sLine);` |
|     4 | 1998 | `			break;` |
|     - | 1999 | `		}` |
|    75 | 2000 | `		nLine = (int)SyBlobLength(&sLine);` |
|    75 | 2001 | `		if( !bTouched ){` |
|     - | 2002 | `			/* php's buffer is still NULL here -- nothing was appended AT ALL,` |
|     - | 2003 | `			 * not even zero bytes -- and it leaves the loop. That is the blank` |
|     - | 2004 | `			 * line that ends a header block, so a body beyond it is never read;` |
|     - | 2005 | `			 * a field whose value is EMPTY did append and keeps the walk going. */` |
|     3 | 2006 | `			SyBlobRelease(&sLine);` |
|     3 | 2007 | `			break;` |
|     - | 2008 | `		}` |
|    73 | 2009 | `		zLine = (const char *)SyBlobData(&sLine);` |
|   333 | 2010 | `		for( iColon = 0 ; iColon < nLine && zLine[iColon] != ':' ; ++iColon ){}` |
|    73 | 2011 | `		if( iColon < nLine ){` |
|     - | 2012 | `			ph7_value *pSlot;` |
|    67 | 2013 | `			k = iColon + 1;` |
|   162 | 2014 | `			while( k < nLine && (zLine[k] == ' ' \|\| zLine[k] == '\t') ){ k++; }` |
|    67 | 2015 | `			ph7_value_string(pVal,&zLine[k],nLine - k);` |
|    67 | 2016 | `			pSlot = ph7_array_fetch(pArray,zLine,iColon);` |
|    67 | 2017 | `			if( pSlot == 0 ){` |
|    85 | 2018 | `				PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|    28 | 2019 | `					zLine,(sxu32)iColon,pVal);` |
|    29 | 2020 | `			}else{` |
|     - | 2021 | `				/* A name seen twice becomes a LIST -- and the first value was` |
|     - | 2022 | `				 * stored as a plain string, so it has to be lifted into one` |
|     - | 2023 | `				 * now, which is php's own shape for a repeated header. */` |
|    11 | 2024 | `				if( !ph7_value_is_array(pSlot) ){` |
|     7 | 2025 | `					ph7_value *pList = ph7_context_new_array(pCtx);` |
|     7 | 2026 | `					if( pList == 0 ){` |
|   ! 0 | 2027 | `						SyBlobRelease(&sLine);` |
|   ! 0 | 2028 | `						ph7_context_release_value(pCtx,pVal);` |
|   ! 0 | 2029 | `						return PH7_ContextMemoryError(pCtx);` |
|     - | 2030 | `					}` |
|     7 | 2031 | `					ph7_array_add_strkey_elem(pList,0,pSlot);` |
|     7 | 2032 | `					ph7_array_add_strkey_elem(pList,0,pVal);` |
|    10 | 2033 | `					PH7_HashmapInsertRawKey((ph7_hashmap *)pArray->x.pOther,` |
|     3 | 2034 | `						zLine,(sxu32)iColon,pList);` |
|     7 | 2035 | `					ph7_context_release_value(pCtx,pList);` |
|     4 | 2036 | `				}else{` |
|     5 | 2037 | `					ph7_array_add_strkey_elem(pSlot,0,pVal);` |
|     - | 2038 | `				}` |
|     - | 2039 | `			}` |
|    67 | 2040 | `			ph7_value_reset_string_cursor(pVal);` |
|    33 | 2041 | `		}` |
|    73 | 2042 | `		SyBlobRelease(&sLine);` |
|    73 | 2043 | `		if( iNext <= 0 ){` |
|   ! 0 | 2044 | `			break;` |
|     - | 2045 | `		}` |
|    73 | 2046 | `		i += iNext;` |
|     1 | 2047 | `	}` |
|    45 | 2048 | `	ph7_context_release_value(pCtx,pVal);` |
|    45 | 2049 | `	if( err != ICV_OK ){` |
|     3 | 2050 | `		IcvShowError(pCtx,err,sCs.zName,sCs.nName,"???",3);` |
|     3 | 2051 | `		ph7_result_bool(pCtx,0);` |
|     2 | 2052 | `	}else{` |
|    43 | 2053 | `		ph7_result_value(pCtx,pArray);` |
|     - | 2054 | `	}` |
|    45 | 2055 | `	return PH7_OK;` |
|    24 | 2056 | `}` |
|     - | 2057 |  |
|   324 | 2058 | `PH7_PRIVATE int PH7_builtin_iconv_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv(pCtx,nArg,apArg); }` |
|    38 | 2059 | `PH7_PRIVATE int PH7_builtin_iconv_get_encoding_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_get_encoding(pCtx,nArg,apArg); }` |
|    79 | 2060 | `PH7_PRIVATE int PH7_builtin_iconv_mime_encode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_encode(pCtx,nArg,apArg); }` |
|   162 | 2061 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode(pCtx,nArg,apArg); }` |
|    47 | 2062 | `PH7_PRIVATE int PH7_builtin_iconv_mime_decode_headers_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_mime_decode_headers(pCtx,nArg,apArg); }` |
|    40 | 2063 | `PH7_PRIVATE int PH7_builtin_iconv_strlen_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strlen(pCtx,nArg,apArg); }` |
|    54 | 2064 | `PH7_PRIVATE int PH7_builtin_iconv_substr_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_substr(pCtx,nArg,apArg); }` |
|    52 | 2065 | `PH7_PRIVATE int PH7_builtin_iconv_strpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strpos(pCtx,nArg,apArg); }` |
|    25 | 2066 | `PH7_PRIVATE int PH7_builtin_iconv_strrpos_f(ph7_context *pCtx,int nArg,ph7_value **apArg){ return PH7_builtin_iconv_strrpos(pCtx,nArg,apArg); }` |
|     - | 2067 |  |
|     - | 2068 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|     - | 2069 |  |
